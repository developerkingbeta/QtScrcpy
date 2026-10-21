#ifndef BATCH_SMSREADER_H
#define BATCH_SMSREADER_H

#include <QList>
#include <QString>

namespace batch {

struct SmsMessage
{
    qint64 dateMs = 0;
    QString address;
    QString body;
};

class SmsReader
{
public:
    static QList<SmsMessage> parseContentQuery(const QString &out);
    static QString extractWechatCode(const QList<SmsMessage> &msgs, qint64 notBeforeMs);
    static QString extractCodeFromNotifications(const QString &dumpsysOut);

private:
    SmsReader();
};

} // namespace batch

#endif // BATCH_SMSREADER_H
