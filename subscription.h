#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include <QObject>
#include <QRegExp>
#include <QDebug>

class Subscription : public QObject
{
    Q_OBJECT
public:
    Subscription(QByteArray *pattern, QObject *parent = 0);
    Subscription(const QString *pattern, QObject *parent = 0);
    Subscription(quint16 identifier, QByteArray *pattern, QObject *parent = 0);

    void init();

    quint16 identifier;
    // QString string;

    void setPattern(const QString pattern);
    void setPattern(QByteArray *pattern) { regexp.setPattern(QString(*pattern)); }
    const QString pattern();

    void setIdentifier(quint16 identifier);

    QList<QByteArray*>* match(QByteArray *message);

    // Callback
    QObject *receiver;
    QByteArray member;

private:

    QRegExp regexp;


};

#endif // SUBSCRIPTION_H
