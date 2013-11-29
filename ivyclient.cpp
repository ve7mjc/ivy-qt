#include "ivyclient.h"

// This will be a client that has announced over UDP
IvyClient::IvyClient(IvyQt *ivyQt, QHostAddress *host, quint16 *port, QString *name, QByteArray *appId, QObject *parent) :
    QObject(parent)
{
    this->ivyQt = ivyQt;

    socket = NULL; // could need this

    // Initialize Members
    this->hostAddress = host;
    this->port = *port;
    this->name = *name;
    if (appId != 0) this->appId = *appId;

    socket = new QTcpSocket();
    connect(socket, SIGNAL(readyRead()), this, SLOT(onSocketReadyRead()));
    connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
                           this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    connect(socket, SIGNAL(bytesWritten(qint64)),
            this, SLOT(onSocketBytesWritten(qint64)));

    socket->connectToHost(*hostAddress,this->port);

    init();
}

// Ivy Client Initialized witha *QTcpSocket
// This would be an incoming TCP connection from a new client
IvyClient::IvyClient(IvyQt *ivyQt, QTcpSocket *socket, QObject *parent) :
    QObject(parent)
{

    this->ivyQt = ivyQt;

    // Bind Socket
    this->socket = socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(onSocketReadyRead()));
    connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
                           this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    connect(socket, SIGNAL(bytesWritten(qint64)),
            this, SLOT(onSocketBytesWritten(qint64)));

    // TODO: Can we assume socket comes with a peerAddress
    // in all scenarios?
    this->hostAddress = new QHostAddress(socket->peerAddress());

    this->port = socket->peerPort();

    init();
}

// Initialization common to all constructors
void IvyClient::init()
{
    ready = false;
    pingId = 0;
    receivedByeRequest = false;

    connect(&pingTimeoutTimer, SIGNAL(timeout()),
            this, SLOT(onPingTimeoutTimerTimeout()));

    statsTcpBytesIn = 0;
    statsTcpBytesOut = 0;
    statsUdpBytesIn = 0;
    statsUdpBytesOut = 0;

//    messageCountStatsIn[10];
//    messageCountStatsOut[10];
    messageCountTotalIn = 0;
    messageCountTotalOut = 0;
}


// TCP socket state has changed
void IvyClient::onSocketStateChanged(QAbstractSocket::SocketState state)
{
    // TCP Connected
    if (state == QAbstractSocket::ConnectedState) {
        sendPeerId();
        sendSubscriptions();
        emit ivyQt->logMessage(QString("TCP CONNECT TO %1:%2").arg(socket->peerAddress().toString()).arg(QString::number(socket->peerPort())),1);
    }

    // TCP Disconnected
    if (state == QAbstractSocket::UnconnectedState) {

        ready = false;

        emit ivyQt->logMessage(QString("Client %1 disconnected!").arg(name),1);

        // Was this expected? Do we have a previous Bye?
        // This could be a result of our own disconnect
        if (!receivedByeRequest) emit ivyClientBye(this,receivedByeRequest);

    }

}

void IvyClient::onSocketReadyRead()
{
    // TODO: Take into account packet fragmentation
    rcvBuffer.clear();

    // Log TCP Bytes Sent In
    logTrafficStats(TCP,In,socket->bytesAvailable());

    rcvBuffer.append(socket->readAll());

    // Proceed if EOM delimiter located
    if (rcvBuffer.contains("\n")) {
        QList<QByteArray> dat = this->rcvBuffer.split('\n');
        for(int i = 0; i < dat.count(); i++) {
            QByteArray *data = new QByteArray(dat.at(i));
            if (data->size()) {
                IvyMessage *msg = new IvyMessage(data,this);
                processMessage(msg);
            }
        }
    }

}

void IvyClient::onSocketBytesWritten(qint64 bytes)
{

}

void IvyClient::processMessage(IvyMessage *msg)
{
    if (!msg->isValid()) return; // abort if message is invalid

    this->messages.append(msg);

    QString message = QString("LOCAL <- %1:%2: %3")
            .arg(socket->peerAddress().toString())
            .arg(QString::number(socket->peerPort()))
            .arg(QString(*msg->data))
            .append("<EOL>");
    emit ivyQt->logMessage(&message,1);

    // Log Message Statistics
    logMessageStats(msg->type,In);

    // Message Type 0: Bye
    if (msg->type == Bye) processBye();

    // Message Type 1: Subscription
    if (msg->type == AddRegexp) {
        Subscription *s = subscriptionByIdentifier(msg->identifier);
        if (s && msg->subscription) {
            // modify existing subscription
            s->setPattern(msg->subscription->pattern());
            // emit signal if this is a post-ready subscription
            if (ready) emit ivyClientSubscription(this,s,true);
        }
        else {
            // append subscription to list
            subscriptions.append(msg->subscription);
            // emit signal if this is a post-ready subscription
            if (ready) emit ivyClientSubscription(this,s,false);
        }

    }

    // Message Type 2: Text Message
    if (msg->type == Msg) {
        emit ivyMessageReceived(msg);
    }

    // End of Initial Subscriptions
    // An agent is not considered ready until
    // it is deemed all of the subscriptions have
    // been relayed
    if (msg->type == EndRegexp)
        setReady();

    // Peer ID Message
    if (msg->type == StartRegexp)
        this->name = msg->getPeerName();

    // Message Type 8: Die Message
    // We are being asked politely to die
    if (msg->type == Die) {
        emit ivyQt->logMessage(QString("Asked politely to die from %1, complying").arg(name),1);
        // TODO, pass this through a control mechanism!
        // a client should be able to prevent a die
        if (ivyQt) ivyQt->IvyDie();
    }

    // Message Type 9: Ping
    // Peer is requesting pong
    if (msg->type == Ping) {
        sendPong(msg->identifier);
        emit ivyQt->logMessage(QString("Received PING (%1) from %2").arg(QString::number(msg->identifier)).arg(name),1);
    }

    // Peer has responded to our ping
    if (msg->type == Pong) {
        processPong(msg->identifier);
        emit ivyQt->logMessage(QString("Received PONG from %3").arg(name),1);
    }

}

// will add EOL here
int IvyClient::sendMessage(MsgType type, quint32 identifier, QByteArray *data)
{
    QByteArray msg;
    msg.append(QString::number(type).toUtf8());
    msg.append(' ');
    msg.append(QString::number(identifier).toUtf8());
    msg.append(ARG_START);

    if (data != 0) msg.append(*data);

    QString message = QString("LOCAL -> %1:%2 %3")
            .arg(socket->peerAddress().toString())
            .arg(QString::number(socket->peerPort()))
            .arg(QString(msg))
            .append("<EOL>");

    msg.append("\n");

    if (socket->isValid()) {
        logTrafficStats(TCP,Out,socket->write(msg));
        logMessageStats(type,Out);
        ivyQt->logMessage(&message,1);
    }

    return false;
}

void IvyClient::sendBye()
{
    sendMessage(Bye,0);

    // Disconnect TCP Connection
    QString msg = QString("Disconnected from %1").arg(socket->peerAddress().toString());
    emit ivyQt->logMessage(&msg,1);

    socket->disconnectFromHost(); // will this go?
}

void IvyClient::IvySendPing()
{
    sendMessage(Ping,++pingId);

    // Start the ping timer for elapsed time
    pingElapsedTimer.start();
    pingTimeoutTimer.start(16 * 1000);

    emit ivyQt->logMessage(QString("Sent PING to %1 (%2:%3)")
                           .arg(name)
                           .arg(socket->peerAddress().toString())
                           .arg(QString::number(socket->peerPort())), 1);
}

void IvyClient::sendPong(qint16 id)
{
    sendMessage(Pong,id);

    emit ivyQt->logMessage(QString("Sent PONG to %1 (%2:%3)")
                           .arg(name)
                           .arg(socket->peerAddress().toString())
                           .arg(QString::number(socket->peerPort())), 1);
}

void IvyClient::IvySendDieMsg(void)
{
    if (isReady()) {
        sendMessage(Die,0);
        emit ivyQt->logMessage(QString("Sent DIE to %1 (%2:%3)")
                               .arg(name)
                               .arg(socket->peerAddress().toString())
                               .arg(QString::number(socket->peerPort())),1);
    } else {
        emit ivyQt->logMessage(QString("ERROR: Unable to send DIE to %1 (%2:%3)")
                               .arg(name)
                               .arg(socket->peerAddress().toString())
                               .arg(QString::number(socket->peerPort())),1);
    }
}

void IvyClient::processPong(qint16 id)
{
    qint64 elapsedTimeus = pingElapsedTimer.nsecsElapsed() / 1000;

    pingTimeoutTimer.stop();

    emit ivyPongReceived(this,id,elapsedTimeus);
}

// We have received a BYE from the remote client
void IvyClient::processBye()
{
    receivedByeRequest = true;

    ready = false;

    // Disconnect
    if (socket->isOpen()) socket->disconnectFromHost();

    // Careful after this as it could result in clean ups
    emit ivyClientBye(this, receivedByeRequest);
}

void IvyClient::setReady(bool value)
{
    this->ready = value;
    if (isReady()) emit ivyClientReady(this);
}

// Return a pointer to Subscription from identifier
// Returns a NULL pointer if not found
Subscription* IvyClient::subscriptionByIdentifier(quint16 identifier)
{
    Subscription *subscription = NULL;
    for(int i = 0; i < subscriptions.count(); i++)
        if (subscriptions.at(i)->identifier == identifier) subscription = subscriptions.at(i);
    return subscription;
}

void IvyClient::sendPeerId()
{
    QByteArray data = ivyQt->agentName.toUtf8();
    sendMessage(StartRegexp,ivyQt->localTcpPort,&data);
}

int IvyClient::sendTextMessage(quint16 ident, QList<QByteArray*> *parameters)
{
    // Buil Parameter String from QList of parameters
    QByteArray data;
    for(int i = 0; i < parameters->count(); i++) {
        data.append(*parameters->at(i));
        data.append(0x03); // always trails a parameter
    }

    sendMessage(Msg,ident,&data);

    return false;
}

// Send subscriptions to remote client
void IvyClient::sendSubscriptions()
{
    QByteArray data;
    // Iterate through local subscriptions
    for(int i = 0; i < ivyQt->subscriptions.count(); i++) {
        // Build TCP Text Message (Message Type 1)
        data = ivyQt->subscriptions.at(i)->pattern().toUtf8();
        sendMessage(AddRegexp,ivyQt->subscriptions.at(i)->identifier,&data);
        data.clear();
    }
    sendMessage(EndRegexp,0);
}

// Update subscription to client based on pointer to Subscription
// Used for post IvyInit binding of new subscriptions as well
void IvyClient::updateSubscription(Subscription *subscription)
{
    // Build TCP Text Message (Message Type 1)
    QByteArray data;
    data = subscription->pattern().toUtf8();
    sendMessage(AddRegexp,subscription->identifier,&data);
}

// Request remote client to delete subscription
// based on identifier
void IvyClient::sendSubscriptionDeletion(quint16 identifier)
{
    // Assemble Message Type 4 - Subscription Deletion
    sendMessage(DelRegexp,identifier);
}

//int IvyClient::sendSubscribeMessage(quint16 ident, QString *expression)
//{
//    // Build TCP Text Message (Message Type 1)
//    QByteArray message = QByteArray("2 ");
//    message.append(QString::number(ident));
//    message.append(*expression);
//    sendMessage(Msg&message);

//    return false;
//}

void IvyClient::onPingTimeoutTimerTimeout()
{
    // Stop the timer or it will timeout endlessly
    pingTimeoutTimer.stop();

    qDebug() << "Ping timeout";
}

void IvyClient::logMessageStats(quint8 type, BusTrafficDirection direction)
{
    switch (direction) {
    case In:
        messageCountStatsIn[type]++;
        messageCountTotalIn++;
        break;

    case Out:
        messageCountStatsOut[type]++;
        messageCountTotalOut++;
        break;
    }

    emit ivyBusMessageStats(type,direction,this);
}

void IvyClient::logTrafficStats(BusTrafficProtocol protocol, BusTrafficDirection direction, quint16 bytes)
{

    if (protocol == TCP) {
        if (direction == In) statsTcpBytesIn += bytes;
        if (direction == Out) statsTcpBytesOut += bytes;
    }

    emit ivyBusTrafficStats(direction,bytes,protocol,this);
}
