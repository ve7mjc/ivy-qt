#include "subscription.h"

Subscription::Subscription(quint16 identifier, QByteArray *pattern, QObject *parent) :
    QObject(parent)
{
    init();

    this->identifier = identifier;
    regexp.setPattern(*pattern);

    // QRegExp::RegExp is the most perl like
    // It is expected that this will be more compatible
    // with Ivy-C implementations relying on PCRE
    regexp.setPatternSyntax(QRegExp::RegExp);

}

Subscription::Subscription(QByteArray *pattern, QObject *parent) :
    QObject(parent)
{
    init();
    regexp.setPattern(QString(*pattern));
}

Subscription::Subscription(const QString *pattern, QObject *parent) :
    QObject(parent)
{
    init();
    regexp.setPattern(*pattern);
}

void Subscription::init()
{
    // Default QMetaObject for future checks
    slotReceiver = 0;
    active = true;
}

void Subscription::setPattern(const QString pattern)
{
    this->regexp.setPattern(pattern);
}

const QString Subscription::pattern()
{
    return this->regexp.pattern();
}

// Return a pointer to a QList of pointers to QByteArray matches
// Return NULL if no matches
QList<QByteArray*>* Subscription::match(QByteArray *message)
{
    QList<QByteArray*> *results = new QList<QByteArray*>;

    // Perform RegExp match of message against
    // subscription pattern
    if (regexp.indexIn(QString(*message)) != -1) {
        for (int i = 0; i < regexp.captureCount(); i++) {
            QByteArray *cap = new QByteArray(regexp.cap(i+1).toUtf8());
            results->append(cap);
        }
        return results;
    }
    else return NULL; // not found
}

void Subscription::setIdentifier(quint16 identifier)
{
    this->identifier = identifier;
}
