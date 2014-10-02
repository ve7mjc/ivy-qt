#ifndef IVYQT_H
#define IVYQT_H

#include <QObject>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTcpSocket> // may not need if using clients!

#include <QList>
#include <QHostAddress>

#include <QRegExp>
#include <QStringList>

#include <QTimer>
#include <QDateTime>

typedef enum {
    Bye = 0,
    AddRegexp = 1,
    Msg = 2,
    Error = 3,
    DelRegexp = 4,
    EndRegexp = 5,
    StartRegexp = 6,
    DirectMsg = 7,
    Die = 8,
    Ping = 9,
    Pong = 10
} MsgType;

typedef struct {
    QString network;
    QString mask;
    quint16 port;
    QString appId;
} Bus;

typedef enum { //not yet in use
    ARG_START = 0x02,
    ARG_END = 0x03
} MsgArgs;

typedef enum {
    Both = 0,
    In = 1,
    Out = 2
} BusTrafficDirection;

typedef enum {
    TCP = 0,
    UDP = 1,
    Either = 2
} BusTrafficProtocol;

//typedef  struct _clnt_lst_dict *RWIvyClientPtr;
//typedef  const struct _clnt_lst_dict *IvyClientPtr;

//typedef enum { IvyApplicationConnected, IvyApplicationDisconnected,
//           IvyApplicationCongestion , IvyApplicationDecongestion,
//           IvyApplicationFifoFull } IvyApplicationEvent;
//typedef enum { IvyAddBind, IvyRemoveBind, IvyFilterBind, IvyChangeBind } IvyBindEvent;

using namespace std;

#include "ivymessage.h"
#include "ivyclient.h"

// my attempt
typedef enum { LogLevelHigh } IvyLogLevel;

class IvyClient;

class IvyQt : public QObject
{
    Q_OBJECT

    static const quint16 maxTcpPort = 65535;
    static const quint16 minTcpPort = 1;
    static const quint8 protocolVersionMajor = 3;
    static const quint8 defaultLogLevel = 9;
    static const QString defaultBusNetwork;
    static const quint16 defaultBusPort = 2010;

public:
    explicit IvyQt(QObject *parent = 0);
    IvyQt(QString name, QObject *parent = 0);

    void IvyInit(QByteArray *appName, QByteArray *readyMsg);
    void IvyInit(char *appName, char *readyMsg);
    void IvyStart(QString network = "");
    void IvyDie();
    void IvyStop(void);

    // Network stored as valid octals without mask
    // e.g. 127 for 127.255.255.255
    void setNetworks(QString bus = "");
    QList<Bus*> getNetworks() { return busNetworks; }

    int IvyBind(const QString *pattern, QObject *receiver = 0, const char *member = 0);
    int IvyBind(const char *pattern, QObject *receiver = 0, const char *member = 0) { return IvyBind(new QString(pattern),receiver,member); }
    int IvyUnBind(quint16 identifier);
    int IvyClearBindings(void);

    void IvySendMsg(QByteArray *msg);
    void IvySendMsg(const char *msg) { IvySendMsg(new QByteArray(msg)); }
    void IvySendMsg(QString msg) { IvySendMsg(msg.toUtf8()); }
    void IvySendMsg(QByteArray msg) { IvySendMsg(&msg); }

    int addIvyClient(QHostAddress* host, quint16* port, QString* name, QByteArray *appId = 0);
    void addIvyClient(IvyClient *client);
    IvyClient* findClient(QHostAddress* host, quint16* port, QString* name);
    QList<IvyClient*> clients;

    Subscription* subscriptionByIdentifier(quint16 identifier);
    QList<Subscription*> subscriptions;

    void logMessage(QString *msg, quint16 level);
    void logMessage(const char *msg, quint16 level) { logMessage(new QString(msg),level); }
    void logMessage(QString msg, quint16 level) { logMessage(&msg, level); }

    void setLogLevel(quint16 level);
    quint16 logLevel();

    // Statistics
    quint32 statsTcpBytesIn;
    quint32 statsTcpBytesOut;
    quint32 statsUdpBytesIn;
    quint32 statsUdpBytesOut;
    quint16 messageCountStatsIn[10];
    quint16 messageCountStatsOut[10];

    //    int IvySendMsg( const char *fmt_message, ... )

    QString agentName;
    quint16 localTcpPort;

    // UDP Socket & Network Address
    QUdpSocket* udpSocket;
    QHostAddress busNetworkAddress;
    quint16 busPort;

    // TCP Server & Interface Address
    QTcpServer* tcpServer;
    QHostAddress localTcpAddress;

private:

    bool active; // should this instead be ready?
    bool obeyDieRequest;

    QList<Bus*> busNetworks;

    // QStringList busNetworks;
    QStringList busNetworkMasks;

    void broadcast();

    int processBroadcastDatagram(QByteArray* datagram, QHostAddress* host = 0, quint16* udpPort = 0);

    void init();

    void sendSubscriptions();

    quint16 _logLevel;

    QByteArray generateAppId(quint16 port);

    QByteArray appId;




signals:

    void joinedIvyBus();
    void leftIvyBus();

    void IvyDie(IvyClient *requester);

    void ivyClientReady(IvyClient *client);
    void ivyClientReady(QString *name, QHostAddress *address, quint16 port);
    void ivyClientBye(IvyClient *client);
    void ivyClientBye(QString *name, QHostAddress *address, quint16 port);
    void ivyClientPong(IvyClient *client, qint16 id, qint64 roundtrip);

    // void ivyBusTraffic(BusTrafficDirection direction = Both, qint32 bytes = 0, BusTrafficProtocol = Either, IvyClient* client = 0);

    void ivyMessagesSent(quint16 msgCount);
    void ivyMessageReceived(IvyMessage* ivymsg);
    void formattedLogMessage(QString* logmsg, quint16 level);

    void ivyBusTrafficStats(BusTrafficDirection direction, qint32 bytes, BusTrafficProtocol protocol, IvyClient* client = 0);
    void ivyBusMessageStats(quint8 type, BusTrafficDirection direction, IvyClient* client = 0);

    // void logMessage(QString* message, quint16 verbosityLevel = 1);
    // void logMessage(const QString *logmsg, quint16 level);

public slots:

    void onIvyClientReady(IvyClient *ivyClient);
    void onIvyClientBye(IvyClient *ivyClient);
    void onIvyClientPong(IvyClient *client, qint16 id, qint64 roundtrip);

    //void on_ivyMessageReceived(quint16 identifier, QList<QByteArray> *args, IvyClient* client);
    void on_ivyMessageReceived(IvyMessage* ivymsg);

    void on_ivyBusTrafficStats(BusTrafficDirection direction, qint32 bytes, BusTrafficProtocol protocol, IvyClient* client = 0);
    void on_ivyBusMessageStats(quint8 type, BusTrafficDirection direction, IvyClient* client = 0);

    void readPendingDatagrams();
    void onTcpServerNewConnection();

};

#endif // IVYQT_H
