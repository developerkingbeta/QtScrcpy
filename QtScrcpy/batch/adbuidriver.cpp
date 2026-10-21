#include "adbuidriver.h"

#include "adbrunner.h"
#include "smsreader.h"
#include "uidump.h"

#include <QThread>

namespace batch {

namespace {

const int kShellTimeoutMs = 30000;
const char kUiDumpPath[] = "/sdcard/qsc_ui.xml";

QString escapeInputText(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char(' '), QStringLiteral("%s"));
    escaped.replace(QLatin1Char('&'), QStringLiteral("\\&"));
    escaped.replace(QLatin1Char('<'), QStringLiteral("\\<"));
    escaped.replace(QLatin1Char('>'), QStringLiteral("\\>"));
    escaped.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    escaped.replace(QLatin1Char(';'), QStringLiteral("\\;"));
    escaped.replace(QLatin1Char('('), QStringLiteral("\\("));
    escaped.replace(QLatin1Char(')'), QStringLiteral("\\)"));
    return escaped;
}

} // namespace

AdbUiDriver::AdbUiDriver(const QString &serial, const std::atomic<bool> *cancel)
    : m_serial(serial)
    , m_cancel(cancel)
{
}

QList<UiNode> AdbUiDriver::dump()
{
    AdbRunner::run(m_serial,
                   {QStringLiteral("shell"),
                    QStringLiteral("uiautomator"),
                    QStringLiteral("dump"),
                    QString::fromUtf8(kUiDumpPath)},
                   kShellTimeoutMs,
                   m_cancel);
    const AdbResult cat =
        AdbRunner::run(m_serial,
                       {QStringLiteral("exec-out"),
                        QStringLiteral("cat"),
                        QString::fromUtf8(kUiDumpPath)},
                       kShellTimeoutMs,
                       m_cancel);
    if (!cat.ok()) {
        return QList<UiNode>();
    }
    return UiDump::parse(cat.out);
}

bool AdbUiDriver::tap(int x, int y)
{
    const AdbResult result =
        AdbRunner::run(m_serial,
                       {QStringLiteral("shell"),
                        QStringLiteral("input"),
                        QStringLiteral("tap"),
                        QString::number(x),
                        QString::number(y)},
                       kShellTimeoutMs,
                       m_cancel);
    return result.ok();
}

bool AdbUiDriver::typeText(const QString &asciiText)
{
    const AdbResult result =
        AdbRunner::run(m_serial,
                       {QStringLiteral("shell"),
                        QStringLiteral("input"),
                        QStringLiteral("text"),
                        escapeInputText(asciiText)},
                       kShellTimeoutMs,
                       m_cancel);
    return result.ok();
}

bool AdbUiDriver::pressBack()
{
    const AdbResult result =
        AdbRunner::run(m_serial,
                       {QStringLiteral("shell"),
                        QStringLiteral("input"),
                        QStringLiteral("keyevent"),
                        QStringLiteral("4")},
                       kShellTimeoutMs,
                       m_cancel);
    return result.ok();
}

bool AdbUiDriver::launchWechat()
{
    const AdbResult result = AdbRunner::run(
        m_serial,
        {QStringLiteral("shell"),
         QStringLiteral("am"),
         QStringLiteral("start"),
         QStringLiteral("-n"),
         QStringLiteral("com.tencent.mm/.ui.LauncherUI")},
        kShellTimeoutMs,
        m_cancel);
    return result.ok();
}

QList<SmsMessage> AdbUiDriver::readSms()
{
    const AdbResult result = AdbRunner::run(
        m_serial,
        {QStringLiteral("shell"),
         QStringLiteral("content"),
         QStringLiteral("query"),
         QStringLiteral("--uri"),
         QStringLiteral("content://sms/inbox"),
         QStringLiteral("--projection"),
         QStringLiteral("address:body:date"),
         QStringLiteral("--sort"),
         QStringLiteral("\"date DESC\"")},
        kShellTimeoutMs,
        m_cancel);
    if (!result.ok()) {
        return QList<SmsMessage>();
    }
    return SmsReader::parseContentQuery(result.out);
}

QString AdbUiDriver::notificationDump()
{
    const AdbResult result = AdbRunner::run(
        m_serial,
        {QStringLiteral("shell"),
         QStringLiteral("dumpsys"),
         QStringLiteral("notification")},
        kShellTimeoutMs,
        m_cancel);
    if (!result.ok()) {
        return QString();
    }
    return result.out;
}

qint64 AdbUiDriver::deviceNowMs()
{
    const AdbResult result = AdbRunner::run(
        m_serial,
        {QStringLiteral("shell"), QStringLiteral("date"), QStringLiteral("+%s")},
        kShellTimeoutMs,
        m_cancel);
    if (!result.ok()) {
        return 0;
    }
    bool ok = false;
    const qint64 seconds = result.out.trimmed().toLongLong(&ok);
    if (!ok) {
        return 0;
    }
    return seconds * 1000;
}

void AdbUiDriver::sleepMs(int ms)
{
    if (ms <= 0) {
        return;
    }
    QThread::msleep(static_cast<unsigned long>(ms));
}

bool AdbUiDriver::cancelled() const
{
    return m_cancel && m_cancel->load();
}

} // namespace batch
