#include "ivyqt.h"

#include <QDebug>

const QString IvyQt::defaultBusNetwork = "127:2010";

IvyQt::IvyQt(QObject *parent) :
    QObject(parent)
{
    init();
}

IvyQt::IvyQt(QString name, QObject *parent) :
    QObject(parent)
{
    this->agentName = name;

    init();
}

void IvyQt::init()
{
    obeyDieRequest = true;

    // Apply default log level
    _logLevel = defaultLogLevel;

    // Default to any available interface
    localTcpAddress = QHostAddress::Any;

    // Create UDP socket for multicast
    udpSocket = new QUdpSocket(this);
    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));

    tcpServer = new QTcpServer(this);
    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(onTcpServerNewConnection()));
}


void IvyQt::IvyInit(char *appName, char *readyMsg)
{
    this->agentName = QString(appName);
}

// Return -1 if error
int IvyQt::IvyBind(const QString *pattern, QObject *receiver, const char *member)
{
    // Create new Subscription
    // Create incrementing identifier
    // Add Subscription to local list of subscriptions
    Subscription *sub = new Subscription(pattern,this);
    if (!subscriptions.count()) sub->setIdentifier(0);
    else sub->setIdentifier(subscriptions.at(subscriptions.count()-1)->identifier+1);
    subscriptions.append(sub);

    // Manage slot if specified
    if (receiver && member) {
        const char* bracketPosition = strchr(member, '(');
        if (!bracketPosition || !(member[0] >= '0' && member[0] <= '3')) {
            qWarning("IvyQt::IvyBind: Invalid slot specification");
            return -1;
        }

        // Extract method name
        sub->slotMember = QByteArray(member+1, bracketPosition - 1 - member);

        // Extract Parameters
        QByteArray parameters = QByteArray(bracketPosition);
        sub->slotParameters = parameters.mid(1,parameters.count()-2); // remove ()

        sub->slotReceiver = receiver;
    }

    // Update to connected clients
    // this will of course do nothing if there are
    // no connnected clients
    for (int i = 0; i < this->clients.count(); i++)
        this->clients.at(i)->updateSubscription(sub);

    // Return Subscription Identifier
    return sub->identifier;

}

int IvyQt::IvyUnBind(quint16 identifier)
{
    // Remove
    subscriptions.removeOne(subscriptionByIdentifier(identifier));

    if (this->active)
        for (int i = 0; i < this->clients.count(); i++)
            this->clients.at(i)->deleteSubscription(identifier);

    return false;
}

QByteArray IvyQt::generateAppId(quint16 port)
{
    // Example AppId:
    // 3 47632 724109996:1858343141:47632 IVYPROBE

    QByteArray appId;

    // Milliseconds since epoch
    qint64 msec = QDateTime::currentMSecsSinceEpoch();

    qsrand(msec); // Seed PRNG

    appId = QByteArray(QString("%1:%2:%3")
                       .arg(QString::number(qrand()))
                       .arg(QString::number(msec))
                       .arg(QString::number(port)).toUtf8());

    return appId;
}


void IvyQt::setNetworks(QString networksString)
{
    // clear existing busses
    busNetworks.clear();

    // apply defaults if no network specified
    if (!networksString.length())
        networksString = defaultBusNetwork;

    // split string by commas and iterate results
    QStringList networks = networksString.split(",");
    for(int i=0;i<networks.count();i++) {

        Bus *bus = new Bus;
        bus->port = 0;

        QString network = networks.at(i);

        // Recover and remove port number if present
        // Set port number if valid
        qint16 pos = network.indexOf(':');
        if (pos>=0) {
            bus->port = network.mid(pos+1,network.length()-pos).toInt();
            network.remove(pos,network.length()-pos);
        }

        QStringList octets;
        QString networkMask;
//        busNetworkMasks.clear();

//        for (int i = 0; i < busNetworks.count(); i++)
//        {
        octets = network.split('.');
        for(int i = octets.count(); i < 4; i++)
            octets.append("255");
        for(int i = 0; i < octets.count(); i++) {
            if (octets.at(i) != "0") networkMask.append(octets.at(i));
            else networkMask.append("255");
            if (i<3) networkMask.append(".");
        }
        busNetworkMasks.append(networkMask);
//        octets.clear();
//        networkMask.clear();

        bus->network = network;
        bus->mask = networkMask;
        busNetworks.append(bus);

        qDebug() << qPrintable(QString("Ivy Bus at %1:%2 -> %3").arg(network).arg(QString::number(bus->port)).arg(bus->mask));
        // }
//        // Apply defaults if not defined
//        if (!network.isEmpty()) {
//            QStringList networks = networks.at(i).split(",");
//            for (int i = 0; i < networks.count(); i++)
//                qDebug() << networks.at(i);

//        } else
//            busNetworks.append(defaultBusNetwork);
//        if (!port) port = defaultBusPort;

        // Build Network Broadcast Addresses
        // Replace missing octets with 255
        // Replace 0 octets with 255


    }

    //todo reassess this one
    // Assign results to IvyQt members
    //this->busPort = port;
}

// Acceptable network formats:
// <blank>
// 172.23:2010
// 172.23.0.0:2010
// 172.23.255.255:2010
// 172.23.2:2010,127:2010
void IvyQt::IvyStart(QString networks)
{
    setNetworks(networks);

    // Request TCPServer to listen on all available interfaces
    // todo: limit interfaces to those in networks above
    tcpServer->listen(QHostAddress::Any);
    localTcpPort = tcpServer->serverPort(); // os binds an unused port

    // Bind UDP Socket to Network Address
    // Allow multiple processes to share same port (QString(validNetwork))
    // busNetworkAddress = QHostAddress("255.255.255.255");
    // todo: manage multiple bus network interfaces and ports
    udpSocket->bind(QHostAddress::Any, busNetworks.at(0)->port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

    udpSocket->joinMulticastGroup(QHostAddress::Any); // busNetworkAddress
    qDebug() << qPrintable(QString("udpSocket->joinMulticastGroup(%1)").arg(busNetworkAddress.toString()));

    // Generate AppId now that we know the port
    // todo: take into account multiple interfaces and multiple ports!
    appId = generateAppId(localTcpPort);

    // Broadcast our presence via UDP Multicast
    broadcast();

    if (tcpServer->isListening() && udpSocket->isValid())
    {
        // logMessage(QString("Joined Ivy Bus %1:%2").arg(QString(this->busNetwork)).arg(QString::number(busPort)),1);
        active = true;
        emit joinedIvyBus();
    } else {
        // logMessage(QString("FAILED to join Ivy Bus %1:%2").arg(QString(this->busNetwork)).arg(QString::number(busPort)),1);
    }
}

void IvyQt::broadcast()
{
    QString datagram = QString("3 %1 %2 %3")
                                 .arg(QString::number(localTcpPort))
                                 .arg(QString(appId))
                                 .arg(agentName);

    datagram.append("\n");
    qDebug() << qPrintable(QString("Datagram: %1").arg(datagram));

    // iterate list of networks!
    for (int i=0;i<busNetworks.count();i++) {
        qint64 bytes = udpSocket->writeDatagram(datagram.toUtf8(), datagram.size(), QHostAddress(busNetworks.at(i)->mask), busNetworks.at(i)->port);
        emit ivyBusTrafficStats(Out,bytes,UDP);
        logMessage(QString("LOCAL -> %1:%2(UDP): %3").arg(busNetworks.at(i)->mask).arg(QString::number(busNetworks.at(i)->port)).arg(datagram),1);
    }
}

void IvyQt::IvyDie()
{
    if (obeyDieRequest) IvyStop();
}

void IvyQt::logMessage(QString *logmsg, quint16 level)
{
    // Substitute non-printable control characters
    // into more verbose representations
    logmsg->replace(0x02,"<STX>");
    logmsg->replace(0x03,"<ETX>");
    logmsg->replace(0x0A,"<EOL>");

    emit formattedLogMessage(logmsg, level);
}

void IvyQt::setLogLevel(quint16 level)
{
    _logLevel = level;
}

quint16 IvyQt::logLevel()
{
    return _logLevel;
}

void IvyQt::IvyStop()
{
    // Send our goodbyes to clients
    // and terminate TCP socket connections
    for (int i = 0; i < clients.count(); i++) {
        clients.at(i)->sendBye();
        clients.at(i)->deleteLater();
    }

    // Clear local QList of clients
    clients.clear();

    // Stop TCP listening
    tcpServer->close();

    // Stop UDP Socket
    udpSocket->disconnectFromHost();

    active = false;

    emit leftIvyBus();
}

// TCP Server has received a TCP connection
// from peer
//
void IvyQt::onTcpServerNewConnection()
{
    // Process pending connections
    while(tcpServer->hasPendingConnections()) {

        // Create new IvyClient and pass address of
        // next TCP connection socket
        IvyClient *client = new IvyClient(this,tcpServer->nextPendingConnection());
        addIvyClient(client);

        client->sendPeerId();
        client->sendSubscriptions();

        logMessage(QString("New TCP connection from %1:%2").arg(client->socket->peerAddress().toString()).arg(QString::number(client->socket->peerPort())),1);
    }
}

// UDP Socket has received datagram from peer
//
void IvyQt::readPendingDatagrams()
{
    // Loop over pending datagrams
    while (udpSocket->hasPendingDatagrams()) {

        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udpSocket->readDatagram(datagram.data(), datagram.size(),
                                &sender, &senderPort);

        processBroadcastDatagram(&datagram, &sender, &senderPort);
        logMessage(QString("UDP Broadcast from %1:%2 - %3").arg(sender.toString()).arg(QString::number(senderPort)).arg(QString(datagram)),1);
    }
}

// Process received UDP Multicast Datagram
// Example: 3 59399 16961:1384811808974:59399 IvyExplorer
int IvyQt::processBroadcastDatagram(QByteArray* datagram, QHostAddress* host, quint16* udpPort)
{
    // Parse Message
    QList<QByteArray> dg = datagram->split(' ');

    bool failed = false;

    // Check for minimum number of elements
    if (dg.count() >= 3) {

        // Check first character (Version Number)
        if (dg.at(0).toInt() == protocolVersionMajor) {

            QString name = dg.at(3);

            // Remove LF if exists
            if (name.right(1) == "\n")
                name = name.left(name.count() - 1);

            // Extract TCP Port
            quint16 tcpPort = dg.at(1).toInt();

            // Extract AppID
            QByteArray *appId = new QByteArray(dg.at(2));

            // Add client if this is not our own broadcast
            if (*appId != this->appId) {
                addIvyClient(host,&tcpPort,&name,appId);
                emit ivyBusTrafficStats(In,datagram->size(),UDP);
            }

        } else failed = true;
    } else failed = true;

    if (!failed) return false;
    else {
        logMessage(QString("Unexpected UDP datagram from %1:%2 %3").arg(host->toString()).arg(QString::number(*udpPort)).arg(QString(*datagram)),1);
        return true;
    }
}

// Find IvyClient based on Host, Port, and AgentName
// Return address of IvyClient match
IvyClient* IvyQt::findClient(QHostAddress *host, quint16 *port, QString *name)
{
    // Iterate QList of IvyClients and return IvyClient*
    // on first match
    for(int i = 0; i < clients.count(); i++)
    {
        if (clients.at(i)->port != *port) break;
        if (*clients.at(i)->hostAddress != *host) break;
        if (clients.at(i)->name != *name) break;

        return clients.at(i);
    }

    // Did not locate client
    return NULL;
}

int IvyQt::addIvyClient(QHostAddress* host, quint16* port, QString* name, QByteArray* appId)
{
    // TODO: DEBUG
    if (findClient(host,port,name) != NULL)
        qDebug() << qPrintable(QString("addIvyClient(..) -> Client %1 at %2:%3 already exists!")
                               .arg(*name)
                               .arg(host->toString())
                               .arg(QString::number(*port)));

    // Return TRUE if client exists
    if (findClient(host,port,name) != NULL) return true;

    // Create a new IvyClient, populate it, and
    // add it to QList<IvyClient*>
    IvyClient* client = new IvyClient(this,host,port,name,appId,this);
    addIvyClient(client);

    // No Errors Detected
    return false;
}

void IvyQt::addIvyClient(IvyClient *client)
{
    connect(client, SIGNAL(ivyClientReady(IvyClient*)),
            this, SLOT(onIvyClientReady(IvyClient*)));

    connect(client, SIGNAL(ivyClientBye(IvyClient*)),
            this, SLOT(onIvyClientBye(IvyClient*)));

    connect(client, SIGNAL(ivyMessageReceived(IvyMessage*)),
            this, SLOT(on_ivyMessageReceived(IvyMessage*)));

    connect(client, SIGNAL(ivyPongReceived(IvyClient*,qint16,qint64)),
            this, SLOT(onIvyClientPong(IvyClient*,qint16,qint64)));

    connect(client, SIGNAL(ivyBusMessageStats(quint8,BusTrafficDirection,IvyClient*)),
            this, SLOT(on_ivyBusMessageStats(quint8,BusTrafficDirection,IvyClient*)));

    connect(client, SIGNAL(ivyBusTrafficStats(BusTrafficDirection,qint32,BusTrafficProtocol,IvyClient*)),
            this, SLOT(on_ivyBusTrafficStats(BusTrafficDirection,qint32,BusTrafficProtocol,IvyClient*)));

    clients.append(client);
}

// Match intended message against connected clients subscriptions
// Will send more than one message per client depending
// on subscriptions
void IvyQt::IvySendMsg(QByteArray *msg)
{
    quint16 msgCount = 0;
    // Find a match
    // /^ $/
    for(int i = 0; i < clients.count(); i++) {
        for(int j = 0; j < clients.at(i)->subscriptions.count(); j++) {
            // qDebug() << qPrintable(QString("Checking %1 Pattern '%2' for '%3'").arg(clients.at(i)->name).arg(clients.at(i)->subscriptions.at(j)->pattern()).arg(QString(msg->data())));
            QList<QByteArray*> *matches = clients.at(i)->subscriptions.at(j)->match(msg);
            if (matches != NULL) {
                // qDebug() << "Match!" << matches->count();
                msgCount++;
                clients.at(i)->sendTextMessage(clients.at(i)->subscriptions.at(j)->identifier,matches);
            }
        }
    }

    emit ivyMessagesSent(msgCount);

}

//// Subscribe local IvyQt client
//// 1) Manage local subscription
//// 2) Communicate subscription to bus
//quint16 IvyQt::IvyBind(QByteArray *pattern)
//{
//    // No errors
//    return bind(pattern);
//}

//quint16 IvyQt::IvyBind(const char *pattern)
//{
//    QByteArray *pat = new QByteArray(pattern);
//    return bind(pat);
//}

//quint16 IvyQt::bind(QByteArray *pattern)
//{
//    // Create new Subscription
//    // Create incrementing identifier
//    // Add Subscription to local list of subscriptions
//    Subscription *sub = new Subscription(pattern);
//    sub->setIdentifier(this->subscriptions.count());
//    this->subscriptions.append(sub);

//    // Update to connected clients
//    // this will of course do nothing if there are
//    // no connnected clients
//    for (int i = 0; i < clients.count(); i++)
//        clients.at(i)->updateSubscription(sub);

//    // Return Identifier
//    return sub->identifier;
//}

//int IvyQt::IvyDelBind(quint16 identifier)
//{
//    for(int i = 0; i < subscriptions.count(); i++)
//    {
//        if (subscriptions.at(i)->identifier == identifier) {
//            // Send all connected clients message type 4
//            // Subscription Deletion
//            for(int j = 0; j < clients.count(); j++)
//                clients.at(j)->sendSubscriptionDeletion(identifier);

//            // Delete local subscription
//            Subscription *subscription = subscriptions.at(i);
//            subscriptions.removeAt(i);
//            delete subscription;
//        }
//    }
//    // TODO!
//}

// Send Subscriptons to Clients
// This must only be called during a
// bus join as the Message Type 5 following
// the list informs the remote client that
// this client is now Ready
void IvyQt::sendSubscriptions()
{
    for(int i = 0; i < clients.count(); i++)
        clients.at(i)->sendSubscriptions();
}

void IvyQt::onIvyClientReady(IvyClient *ivyClient)
{
    emit ivyClientReady(ivyClient);
    logMessage(QString("IvyClient %1 READY").arg(ivyClient->name),1);
}

// Ivy Client is disconnected
// Possible causes:
// 1) Remote client sent bye
// 2) Remote client disconnected
// 3) Local disconnected peer
void IvyQt::onIvyClientBye(IvyClient *ivyClient)
{
    // Remove signal and slot mapping
    disconnect(ivyClient, SIGNAL(ivyClientReady(IvyClient*)),
            this, SLOT(onIvyClientReady(IvyClient*)));
    disconnect(ivyClient, SIGNAL(ivyClientBye(IvyClient*)),
            this, SLOT(onIvyClientBye(IvyClient*)));
    disconnect(ivyClient, SIGNAL(ivyMessageReceived(IvyMessage*)),
            this, SLOT(on_ivyMessageReceived(IvyMessage*)));

    logMessage(QString("IvyClient %1 BYE").arg(ivyClient->name),1);

    emit ivyClientBye(ivyClient);

    // Clean up client
    clients.removeAll(ivyClient);

    // TODO: too dangerous to delete as-is at this point
    // need a more elegant solution
    ivyClient->deleteLater();
}

// void IvyQt::on_ivyMessageReceived(quint16 identifier, QList<QByteArray> *args, IvyClient *client)
void IvyQt::on_ivyMessageReceived(IvyMessage *ivymsg)
{

    if ((ivymsg->type == Msg) && ivymsg->isValid())
    {
        Subscription* subscription = subscriptionByIdentifier(ivymsg->identifier);
        // Invoke slot if designated for this subscription
        if (!subscription->slotMember.isEmpty() && subscription->slotReceiver)
        {
            // qRegisterMetaType<QList<QByteArray>*>("QList<QByteArray>*");
            // slot(IvyMessage*)
            if (subscription->slotParameters == "IvyMessage*") {
                qRegisterMetaType<IvyMessage*>("IvyMessage*");
                QMetaObject::invokeMethod(subscription->slotReceiver, subscription->slotMember.constData(), Qt::AutoConnection, Q_ARG(IvyMessage*, ivymsg));
            }

            // slot()
            if (!subscription->slotParameters.count())
                QMetaObject::invokeMethod(subscription->slotReceiver, subscription->slotMember.constData(), Qt::AutoConnection);
        }

    }

    emit ivyMessageReceived(ivymsg);

//    QString prms;
//    for (int i = 0; i < args->count(); i++) {
//        prms.append(args->at(i));
//        if (i < (args->count()-1)) prms.append(",");
//    }

//    logMessage(QString("Message from %1 - %2: %3").arg(client->name).arg(QString::number(identifier)).arg(prms),1);

}

// Client has received a PONG response to a PING
void IvyQt::onIvyClientPong(IvyClient *client, qint16 id, qint64 roundtrip)
{
    emit ivyClientPong(client,id,roundtrip);
}

void IvyQt::on_ivyBusTrafficStats(BusTrafficDirection direction, qint32 bytes, BusTrafficProtocol protocol, IvyClient* client)
{
    switch (protocol) {

    case TCP:
        if (direction == In) statsTcpBytesIn += bytes;
        if (direction == Out) statsTcpBytesOut += bytes;
        break;

    case UDP:
        if (direction == In) statsUdpBytesIn += bytes;
        if (direction == Out) statsUdpBytesOut += bytes;
        break;
    }

    // re-transmit event
    emit ivyBusTrafficStats(direction,bytes,protocol,client);
}

// Return address of Subscription from Identifier, or 0 if not found
Subscription* IvyQt::subscriptionByIdentifier(quint16 identifier)
{
    Subscription* result = 0;
    for(int i = 0; i < subscriptions.count(); i++)
        if (subscriptions.at(i)->identifier == identifier) {
            result = subscriptions.at(i);
            break;
        }
    return result;
}

void IvyQt::on_ivyBusMessageStats(quint8 type, BusTrafficDirection direction, IvyClient* client)
{
    switch (direction) {
    case In:
        messageCountStatsIn[type]++;
        break;

    case Out:
        messageCountStatsOut[type]++;
        break;
    }

    emit ivyBusMessageStats(type,direction,client);
}

