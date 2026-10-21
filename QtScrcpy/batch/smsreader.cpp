#include "smsreader.h"

#include <QRegularExpression>

namespace batch {

namespace {

const QRegularExpression &contentRowRe()
{
    static const QRegularExpression re(
        QStringLiteral(R"(^Row:\s*\d+\s+address=(.*?),\s*body=(.*),\s*date=(\d+)\s*$)"));
    return re;
}

const QRegularExpression &wechatKeywordRe()
{
    static const QRegularExpression re(QStringLiteral(R"((WeChat|Weixin|微信))"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression &sixDigitCodeRe()
{
    static const QRegularExpression re(QStringLiteral(R"(\b(\d{6})\b)"));
    return re;
}

QString extractCodeFromText(const QString &text)
{
    if (!wechatKeywordRe().match(text).hasMatch()) {
        return QString();
    }
    QRegularExpressionMatchIterator it = sixDigitCodeRe().globalMatch(text);
    if (!it.hasNext()) {
        return QString();
    }
    return it.next().captured(1);
}

} // namespace

QList<SmsMessage> SmsReader::parseContentQuery(const QString &out)
{
    QList<SmsMessage> messages;
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QLatin1String("Row:"))) {
            continue;
        }
        const QRegularExpressionMatch match = contentRowRe().match(trimmed);
        if (!match.hasMatch()) {
            continue;
        }
        SmsMessage msg;
        msg.address = match.captured(1);
        msg.body = match.captured(2);
        msg.dateMs = match.captured(3).toLongLong();
        messages.append(msg);
    }
    return messages;
}

QString SmsReader::extractWechatCode(const QList<SmsMessage> &msgs, qint64 notBeforeMs)
{
    qint64 bestDate = -1;
    QString bestCode;
    for (const SmsMessage &msg : msgs) {
        if (msg.dateMs < notBeforeMs) {
            continue;
        }
        const QString code = extractCodeFromText(msg.body);
        if (code.isEmpty()) {
            continue;
        }
        if (msg.dateMs >= bestDate) {
            bestDate = msg.dateMs;
            bestCode = code;
        }
    }
    return bestCode;
}

QString SmsReader::extractCodeFromNotifications(const QString &dumpsysOut)
{
    return extractCodeFromText(dumpsysOut);
}

} // namespace batch
