#include "adbuidriver.h"

#include "adbrunner.h"
#include "smsreader.h"
#include "uidump.h"

#include <QDateTime>
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
    // Plain `uiautomator dump` often fails with "could not get idle state" on
    // WeChat's animated Welcome splash (MIUI). `--compressed` still returns a
    // usable hierarchy on that screen; fall back to plain dump if needed.
    const QStringList attempts[] = {
        {QStringLiteral("shell"),
         QStringLiteral("uiautomator"),
         QStringLiteral("dump"),
         QStringLiteral("--compressed"),
         QString::fromUtf8(kUiDumpPath)},
        {QStringLiteral("shell"),
         QStringLiteral("uiautomator"),
         QStringLiteral("dump"),
         QString::fromUtf8(kUiDumpPath)},
    };

    for (const QStringList &args : attempts) {
        if (m_cancel && m_cancel->load()) {
            return QList<UiNode>();
        }
        AdbRunner::run(m_serial, args, kShellTimeoutMs, m_cancel);
        const AdbResult cat =
            AdbRunner::run(m_serial,
                           {QStringLiteral("exec-out"),
                            QStringLiteral("cat"),
                            QString::fromUtf8(kUiDumpPath)},
                           kShellTimeoutMs,
                           m_cancel);
        if (!cat.ok() || cat.out.trimmed().isEmpty()) {
            continue;
        }
        const QList<UiNode> nodes = UiDump::parse(cat.out);
        if (!nodes.isEmpty()) {
            return nodes;
        }
    }
    return QList<UiNode>();
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
    // Device clock for SMS notBefore / step budgets. Never return 0 on failure
    // (that disabled timeouts). Fall back to host wall if adb date fails.
    const AdbResult result = AdbRunner::run(
        m_serial,
        {QStringLiteral("shell"), QStringLiteral("date"), QStringLiteral("+%s")},
        kShellTimeoutMs,
        m_cancel);
    if (result.ok()) {
        bool ok = false;
        const qint64 seconds = result.out.trimmed().toLongLong(&ok);
        if (ok && seconds > 0) {
            return seconds * 1000;
        }
    }
    return QDateTime::currentMSecsSinceEpoch();
}

qint64 AdbUiDriver::steadyNowMs()
{
    // Host wall clock — captchaTimeoutSec is real wall time regardless of adb.
    return QDateTime::currentMSecsSinceEpoch();
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
