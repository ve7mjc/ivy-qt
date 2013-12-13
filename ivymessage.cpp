#include "ivymessage.h"

IvyMessage::IvyMessage(IvyClient *client) :
    QObject(client)
{
    this->client = client;
    this->subscription = 0;
    this->identifier = -1;

    valid = false;
}

IvyMessage::IvyMessage(QByteArray *data, IvyClient *client) :
    QObject(client)
{
    this->subscription = 0;
    this->data = data;
    this->client = client;
    this->m_date = this->m_date.currentDateTime();
    this->identifier -1;

    // Trim message of EOL char if exists
    if (data->right(1).data() == "\n") data->remove(data->count()-1,1);

    // Process Message Type
    this->type = (MsgType)data->left(2).trimmed().toInt();

    // Recover STX location if present
    stxPos = data->indexOf(2);

    // Locate Identifier
    // TODO: Is this OK? Concerned it doesnt look wide enough
    if(stxPos>=0)
        identifier = data->mid(2,stxPos-2).toInt();

    // Message Type 1: Subscription
    if (type == AddRegexp) {
        QByteArray pattern = data->mid(stxPos+1,data->length()-stxPos-1);
        subscription = new Subscription(identifier,&pattern,client);
    }

    // Message Type 2: Message (with regexp response)
    if (type == Msg) {
        parameters = data->mid(stxPos+1,data->length()-stxPos-1).split(ARG_END);
        parameters.removeLast();
    }

    // Message Type 6: Start Regexp
    if (type == StartRegexp)
    {
        // Split message by ETX character (0x03)
        this->parameters = data->mid(stxPos+1,data->size()-stxPos+1).split(0x03);
        if (!this->parameters.last().size()) this->parameters.removeLast(); // remove trailing empty
    }

    // TODO: increase checking
    valid = true;

}

QString IvyMessage::getPeerName()
{
    if (data->length() > 4 && stxPos)
        return QString(data->mid(stxPos+1,data->length()-stxPos+1));
    else
        return QString();
}

// Return verbose QString with match/parameters
// Add comma and spaces seperation
// Example "Match2, Match2"
QString IvyMessage::content()
{
    QString content;
    if (type == Msg) {
        for (int i = 0; i < parameters.count(); i++) {
            content.append(parameters.at(i));
            if (i < parameters.count() - 1) content.append(", ");
        }
    }
    return content;
}
