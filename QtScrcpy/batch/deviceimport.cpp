#include "deviceimport.h"

#include "hostcheck.h"

#include <QLatin1Char>
#include <QSet>

namespace batch {

namespace {

QString normalizedHeader(const QString &cell)
{
    return cell.trimmed().toLower();
}

bool headerMatches(const QString &header, const QStringList &aliases)
{
    const QString h = normalizedHeader(header);
    for (const QString &alias : aliases) {
        if (h == alias) {
            return true;
        }
    }
    return false;
}

int findColumn(const QStringList &headerRow, const QStringList &aliases)
{
    for (int i = 0; i < headerRow.size(); ++i) {
        if (headerMatches(headerRow.at(i), aliases)) {
            return i;
        }
    }
    return -1;
}

QString cellAt(const QStringList &row, int col)
{
    if (col < 0 || col >= row.size()) {
        return QString();
    }
    return row.at(col).trimmed();
}

QString normalizePhone(QString raw)
{
    raw = raw.trimmed();
    if (raw.contains(QLatin1Char('E'), Qt::CaseInsensitive)) {
        bool ok = false;
        const double value = raw.toDouble(&ok);
        if (ok) {
            raw = QString::number(value, 'f', 0);
        }
    }

    QString digits;
    bool hasPlus = false;
    for (int i = 0; i < raw.size(); ++i) {
        const QChar ch = raw.at(i);
        if (ch.isDigit()) {
            digits.append(ch);
        } else if (ch == QLatin1Char('+') && digits.isEmpty() && !hasPlus) {
            hasPlus = true;
        }
    }

    if (digits.isEmpty()) {
        return QString();
    }
    const int len = digits.size();
    if (len < 7 || len > 15) {
        return QString();
    }
    if (hasPlus) {
        return QLatin1Char('+') + digits;
    }
    return digits;
}

bool parsePort(const QString &text, quint16 *outPort)
{
    if (text.isEmpty()) {
        if (outPort) {
            *outPort = 5555;
        }
        return true;
    }
    bool ok = false;
    const int port = text.toInt(&ok);
    if (!ok || port < 1 || port > 65535) {
        return false;
    }
    if (outPort) {
        *outPort = static_cast<quint16>(port);
    }
    return true;
}

} // namespace

DeviceImport::DeviceImport()
{
}

ImportResult DeviceImport::fromTable(const SheetTable &table)
{
    ImportResult result;
    if (table.isEmpty()) {
        result.error = QStringLiteral("Sheet is empty");
        return result;
    }

    const QStringList header = table.first();
    static const QStringList ipAliases = {
        QStringLiteral("tailscale_ip"),
        QStringLiteral("ip"),
        QStringLiteral("tailscale ip"),
        QStringLiteral("device ip"),
    };
    static const QStringList phoneAliases = {
        QStringLiteral("phone"),
        QStringLiteral("phone_number"),
        QStringLiteral("number"),
        QStringLiteral("mobile"),
    };
    static const QStringList portAliases = {
        QStringLiteral("port"),
    };

    const int ipCol = findColumn(header, ipAliases);
    const int phoneCol = findColumn(header, phoneAliases);
    const int portCol = findColumn(header, portAliases);

    if (ipCol < 0) {
        result.error = QStringLiteral("Missing required IP column header");
        return result;
    }
    if (phoneCol < 0) {
        result.error = QStringLiteral("Missing required phone column header");
        return result;
    }

    QSet<QString> seenIps;
    QSet<QString> seenPhones;

    for (int rowIdx = 1; rowIdx < table.size(); ++rowIdx) {
        const QStringList &row = table.at(rowIdx);
        const int sheetRow = rowIdx + 1;

        const QString ip = cellAt(row, ipCol);
        const QString phoneRaw = cellAt(row, phoneCol);
        const QString portText = cellAt(row, portCol);

        if (ip.isEmpty() && phoneRaw.isEmpty()) {
            continue;
        }

        if (ip.isEmpty()) {
            result.warnings.append(
                QStringLiteral("row %1: missing IP").arg(sheetRow));
            continue;
        }
        if (!HostCheck::isIPv4Literal(ip)) {
            result.warnings.append(
                QStringLiteral("row %1: invalid IP").arg(sheetRow));
            continue;
        }
        if (HostCheck::isPublicRoutableIPv4(ip)) {
            result.warnings.append(
                QStringLiteral("row %1: public routable IP not allowed").arg(sheetRow));
            continue;
        }

        const QString phone = normalizePhone(phoneRaw);
        if (phone.isEmpty()) {
            result.warnings.append(
                QStringLiteral("row %1: invalid phone").arg(sheetRow));
            continue;
        }

        quint16 port = 5555;
        if (!parsePort(portText, &port)) {
            result.warnings.append(
                QStringLiteral("row %1: invalid port").arg(sheetRow));
            continue;
        }

        if (seenIps.contains(ip)) {
            result.warnings.append(
                QStringLiteral("row %1: duplicate IP").arg(sheetRow));
            continue;
        }
        if (seenPhones.contains(phone)) {
            result.warnings.append(
                QStringLiteral("row %1: duplicate phone").arg(sheetRow));
            continue;
        }

        seenIps.insert(ip);
        seenPhones.insert(phone);

        DeviceRow device;
        device.ip = ip;
        device.phone = phone;
        device.port = port;
        result.rows.append(device);
    }

    return result;
}

} // namespace batch
