#include "deviceprobe.h"

#include <QFileInfo>
#include <QRegularExpression>

#include "adbrunner.h"

namespace batch {

namespace {

QString mapInstallFailure(const QString &output)
{
    if (output.contains(QStringLiteral("INSTALL_FAILED_NO_MATCHING_ABIS"))) {
        return QStringLiteral("NO_MATCHING_ABIS");
    }
    if (output.contains(QStringLiteral("INSTALL_FAILED_USER_RESTRICTED"))) {
        return QStringLiteral("USER_RESTRICTED");
    }
    if (output.contains(QStringLiteral("INSTALL_FAILED_INSUFFICIENT_STORAGE"))) {
        return QStringLiteral("INSUFFICIENT_STORAGE");
    }
    if (output.contains(QStringLiteral("INSTALL_FAILED_VERSION_DOWNGRADE"))) {
        return QStringLiteral("VERSION_DOWNGRADE");
    }

    static const QRegularExpression failurePattern(QStringLiteral("INSTALL_FAILED_([^\\]\\s]+)"));
    const QRegularExpressionMatch match = failurePattern.match(output);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return output.trimmed();
}

QString parseVersionName(const QString &dumpsysOut)
{
    static const QRegularExpression versionPattern(QStringLiteral("versionName=([^\\s]+)"));
    const QRegularExpressionMatch match = versionPattern.match(dumpsysOut);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

ConnState classifyConnectionState(const AdbResult &stateResult)
{
    if (!stateResult.started || stateResult.timedOut || stateResult.cancelled) {
        return ConnState::Unreachable;
    }

    const QString state = stateResult.out.trimmed().toLower();
    if (state.contains(QStringLiteral("unauthorized"))) {
        return ConnState::Unauthorized;
    }
    if (state.contains(QStringLiteral("offline"))) {
        return ConnState::Offline;
    }
    if (state == QStringLiteral("device")) {
        return ConnState::Connected;
    }
    return ConnState::Unreachable;
}

} // namespace

ConnState DeviceProbe::probeConnection(DeviceRow &row, const std::atomic<bool> *cancel)
{
    const QString serial = row.serial();

    const AdbResult connectResult = AdbRunner::run(QString(),
                                                     QStringList{QStringLiteral("connect"), serial},
                                                     8000,
                                                     cancel);
    if (connectResult.cancelled) {
        row.conn = ConnState::Unreachable;
        return row.conn;
    }
    if (connectResult.timedOut) {
        row.conn = ConnState::Unreachable;
        return row.conn;
    }

    const AdbResult stateResult = AdbRunner::run(serial, QStringList{QStringLiteral("get-state")}, 5000, cancel);
    row.conn = classifyConnectionState(stateResult);
    if (row.conn != ConnState::Connected) {
        return row.conn;
    }

    const AdbResult modelResult = AdbRunner::run(serial,
                                                 QStringList{QStringLiteral("shell"),
                                                             QStringLiteral("getprop"),
                                                             QStringLiteral("ro.product.model")},
                                                 5000,
                                                 cancel);
    if (modelResult.ok()) {
        row.detail = modelResult.out.trimmed();
    }
    return row.conn;
}

WechatState DeviceProbe::probeWechat(DeviceRow &row, const std::atomic<bool> *cancel)
{
    const QString serial = row.serial();
    const AdbResult pathResult = AdbRunner::run(serial,
                                                QStringList{QStringLiteral("shell"),
                                                            QStringLiteral("pm"),
                                                            QStringLiteral("path"),
                                                            QStringLiteral("com.tencent.mm")},
                                                5000,
                                                cancel);

    if (!pathResult.out.trimmed().startsWith(QStringLiteral("package:"))) {
        row.wechat = WechatState::Missing;
        return row.wechat;
    }

    row.wechat = WechatState::Installed;
    const AdbResult dumpsysResult = AdbRunner::run(serial,
                                                   QStringList{QStringLiteral("shell"),
                                                               QStringLiteral("dumpsys"),
                                                               QStringLiteral("package"),
                                                               QStringLiteral("com.tencent.mm")},
                                                   30000,
                                                   cancel);
    const QString version = parseVersionName(dumpsysResult.out);
    if (!version.isEmpty()) {
        row.wechatVersion = version;
    }
    return row.wechat;
}

bool DeviceProbe::installWechat(DeviceRow &row,
                                const QString &apkPath,
                                QString *error,
                                const std::atomic<bool> *cancel)
{
    if (!QFileInfo(apkPath).isFile()) {
        if (error) {
            *error = QStringLiteral("APK file not found");
        }
        return false;
    }

    const QString serial = row.serial();
    const AdbResult result = AdbRunner::run(serial,
                                            QStringList{QStringLiteral("install"), QStringLiteral("-r"), apkPath},
                                            600000,
                                            cancel);

    const QString combined = result.out + result.err;
    if (combined.contains(QStringLiteral("Success"))) {
        row.wechat = WechatState::Installed;
        if (error) {
            error->clear();
        }
        return true;
    }

    row.wechat = WechatState::InstallFailed;
    const QString message = mapInstallFailure(combined);
    if (error) {
        *error = message.isEmpty() ? QStringLiteral("Install failed") : message;
    }
    return false;
}

} // namespace batch
