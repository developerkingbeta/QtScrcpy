#include "hostcheck.h"

#include <QLatin1Char>
#include <QStringList>

namespace batch {

namespace {

bool parseIPv4Literal(const QString &host, quint32 *outIp)
{
    const QString trimmed = host.trimmed();
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    const QStringList parts = trimmed.split(QLatin1Char('.'), Qt::SkipEmptyParts);
#else
    const QStringList parts = trimmed.split(QLatin1Char('.'), QString::SkipEmptyParts);
#endif
    if (parts.size() != 4) {
        return false;
    }

    quint32 ip = 0;
    for (int i = 0; i < 4; ++i) {
        const QString &part = parts.at(i);
        if (part.isEmpty()) {
            return false;
        }
        for (int j = 0; j < part.size(); ++j) {
            if (!part.at(j).isDigit()) {
                return false;
            }
        }
        bool ok = false;
        const int octet = part.toInt(&ok);
        if (!ok || octet < 0 || octet > 255) {
            return false;
        }
        ip = (ip << 8) | static_cast<quint32>(octet);
    }

    if (outIp) {
        *outIp = ip;
    }
    return true;
}

bool isNonPublicRoutableIPv4(quint32 ip)
{
    // 10.0.0.0/8
    if ((ip & 0xFF000000u) == 0x0A000000u) {
        return true;
    }
    // 172.16.0.0/12
    if ((ip & 0xFFF00000u) == 0xAC100000u) {
        return true;
    }
    // 192.168.0.0/16
    if ((ip & 0xFFFF0000u) == 0xC0A80000u) {
        return true;
    }
    // 127.0.0.0/8
    if ((ip & 0xFF000000u) == 0x7F000000u) {
        return true;
    }
    // 100.64.0.0/10 (CGNAT / Tailscale)
    if ((ip & 0xFFC00000u) == 0x64400000u) {
        return true;
    }
    // 169.254.0.0/16 link-local
    if ((ip & 0xFFFF0000u) == 0xA9FE0000u) {
        return true;
    }
    return false;
}

} // namespace

HostCheck::HostCheck()
{
}

bool HostCheck::isIPv4Literal(const QString &host)
{
    quint32 ip = 0;
    return parseIPv4Literal(host, &ip);
}

bool HostCheck::isPublicRoutableIPv4(const QString &host)
{
    quint32 ip = 0;
    if (!parseIPv4Literal(host, &ip)) {
        return false;
    }
    return !isNonPublicRoutableIPv4(ip);
}

} // namespace batch
