#ifndef BATCH_DEVICEROW_H
#define BATCH_DEVICEROW_H

#include <QMetaType>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace batch {

enum class ConnState {
    Unknown,
    Checking,
    Connected,
    Unauthorized,
    Offline,
    Unreachable,
    Invalid
};

enum class WechatState {
    Unknown,
    Checking,
    Installed,
    Missing,
    Installing,
    InstallFailed
};

enum class RegState {
    Idle,
    Running,
    Registered,
    AlreadyRegistered,
    NeedsManual,
    Failed,
    Cancelled
};

struct DeviceRow {
    QString ip;
    quint16 port = 5555;
    QString phone;
    ConnState conn = ConnState::Unknown;
    WechatState wechat = WechatState::Unknown;
    RegState reg = RegState::Idle;
    QString wechatVersion;
    QString detail;

    QString serial() const
    {
        return ip + QLatin1Char(':') + QString::number(port);
    }
};

} // namespace batch

Q_DECLARE_METATYPE(batch::DeviceRow)

#endif // BATCH_DEVICEROW_H
