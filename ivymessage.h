#ifndef IVYMESSAGE_H
#define IVYMESSAGE_H

#include <QObject>
#include <QDateTime>
#include "ivyqt.h"
#include "ivyclient.h"
#include "subscription.h"

class IvyClient;

class IvyMessage : public QObject
{
    Q_OBJECT

public:
    explicit IvyMessage(IvyClient *client = 0);
    IvyMessage(QByteArray *data, IvyClient *client = 0);

    // Raw Data
    QByteArray *data;

    MsgType type;
    quint16 identifier;
    QList<QByteArray> parameters;

    Subscription *subscription;
    IvyClient *client;

    QDateTime date() { return m_date; }

    bool isValid() { return valid; }
    QString getPeerName();

private:

    bool valid;
    qint16 stxPos;

    QDateTime m_date; // sent or received

signals:

public slots:

};

#endif // IVYMESSAGE_H
