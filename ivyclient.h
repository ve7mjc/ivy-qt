#ifndef IVYCLIENT_H
#define IVYCLIENT_H

#include <QObject>
#include <QHostAddress>
#include <QTcpSocket>
#include <QList>

#include "subscription.h"
#include "ivyqt.h"
#include "ivymessage.h"

#include <QTimer>
#include <QElapsedTimer>

class IvyQt;
class IvyMessage;

class IvyClient : public QObject
{
    Q_OBJECT

public:

    static const quint16 pingTimeoutSeconds = 30;

    IvyClient(IvyQt *ivyQt, QHostAddress *host, quint16 *port, QString *name, QByteArray *appId = 0, QObject *parent = 0);
    IvyClient(IvyQt *ivyQt, QTcpSocket* socket, QObject *parent = 0);

    void init();

    void sendPeerId();
    void sendBye();
    void IvySendPing(void);
    void sendPong(qint16 id);
    void IvySendDieMsg(void);
    void processPong(qint16 id);
    void processBye();
    void sendSubscriptions();
    void updateSubscription(Subscription *subscription);
    void sendSubscriptionDeletion(quint16 identifier);

    int sendMessage(MsgType type, quint32 identifier, QByteArray *data = 0);

    int start();
    int sendTextMessage(quint16 ident, QList<QByteArray*> *parameters);
    int sendSubscribeMessage(quint16 ident, QString *expression);

    IvyQt *ivyQt;

    QHostAddress *hostAddress;
    quint16 port;
    QByteArray appId;
    QString name;

    bool ready;
    bool isReady() { return ready; }

    QTcpSocket *socket;
    QByteArray rcvBuffer;

    QList<Subscription*> subscriptions;
    Subscription* subscriptionByIdentifier(quint16 identifier);

    QList<IvyMessage*> messages;

    void processMessage(IvyMessage *msg);

    QElapsedTimer pingElapsedTimer;
    QTimer pingTimeoutTimer;
    quint16 pingId;

    // Statistics
    quint32 statsTcpBytesIn;
    quint32 statsTcpBytesOut;
    quint32 statsUdpBytesIn;
    quint32 statsUdpBytesOut;

    quint16 messageCountStatsIn[10];
    quint16 messageCountStatsOut[10];
    quint16 messageCountTotalIn;
    quint16 messageCountTotalOut;

private:

    void setReady(bool value = true);
    bool receivedByeRequest;

    void logTrafficStats(BusTrafficProtocol type, BusTrafficDirection direction, quint16 bytes);
    void logMessageStats(quint8 type, BusTrafficDirection direction);

    // (qint16 bytes);
    void statsTcpOut(qint16 bytes);
    void statsUdpIn(qint16 bytes);
    void statsUdpOut(qint16 bytes);

signals:

    void ivyClientReady(IvyClient *ivyClient);
    void ivyClientBye(IvyClient *ivyClient, bool graceful = true);

    void ivyMessageReceived(IvyMessage* ivymsg);
    void ivyPongReceived(IvyClient* client, qint16 id, qint64 roundtrip);

    void ivyBusTrafficStats(BusTrafficDirection direction, qint32 bytes, BusTrafficProtocol, IvyClient* client = 0);
    void ivyBusMessageStats(quint8 type, BusTrafficDirection direction, IvyClient* client = 0);

    void ivyClientSubscription(IvyClient *client, Subscription *subscription, bool change);

public slots:

    void onSocketReadyRead();
    void onSocketStateChanged(QAbstractSocket::SocketState state);

    void onSocketBytesWritten(qint64 bytes);
    void onPingTimeoutTimerTimeout();

};

#endif // IVYCLIENT_H
