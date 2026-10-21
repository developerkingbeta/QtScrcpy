#include "registrationstore.h"

#include <QDir>
#include <QFile>

namespace batch {

namespace {

const char kCsvFileName[] = "registrations.csv";

bool isRegisteredState(RegState state)
{
    return state == RegState::Registered || state == RegState::AlreadyRegistered;
}

void applyOwnerOnlyPermissions(const QString &path)
{
    QFile::setPermissions(path,
                          QFile::ReadOwner | QFile::WriteOwner);
}

} // namespace

RegistrationStore::RegistrationStore(const QString &dir)
    : m_dir(dir)
    , m_csvPath(QDir(dir).filePath(QString::fromLatin1(kCsvFileName)))
{
    QDir().mkpath(dir);
    reloadFromDisk();
}

QString RegistrationStore::pairKey(const QString &ip, const QString &phone) const
{
    return ip + QChar(0x1e) + phone;
}

QString RegistrationStore::regStateToString(RegState state)
{
    switch (state) {
    case RegState::Idle:
        return QStringLiteral("Idle");
    case RegState::Running:
        return QStringLiteral("Running");
    case RegState::Registered:
        return QStringLiteral("Registered");
    case RegState::AlreadyRegistered:
        return QStringLiteral("AlreadyRegistered");
    case RegState::NeedsManual:
        return QStringLiteral("NeedsManual");
    case RegState::Failed:
        return QStringLiteral("Failed");
    case RegState::Cancelled:
        return QStringLiteral("Cancelled");
    }
    return QStringLiteral("Idle");
}

RegState RegistrationStore::regStateFromString(const QString &text)
{
    if (text == QLatin1String("Running"))
        return RegState::Running;
    if (text == QLatin1String("Registered"))
        return RegState::Registered;
    if (text == QLatin1String("AlreadyRegistered"))
        return RegState::AlreadyRegistered;
    if (text == QLatin1String("NeedsManual"))
        return RegState::NeedsManual;
    if (text == QLatin1String("Failed"))
        return RegState::Failed;
    if (text == QLatin1String("Cancelled"))
        return RegState::Cancelled;
    if (text == QLatin1String("Idle"))
        return RegState::Idle;
    return RegState::Idle;
}

QString RegistrationStore::csvEscapeField(const QString &field)
{
    const bool needsQuotes = field.contains(QLatin1Char(',')) || field.contains(QLatin1Char('"'))
        || field.contains(QLatin1Char('\n')) || field.contains(QLatin1Char('\r'));
    if (!needsQuotes)
        return field;

    QString escaped = field;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QStringList RegistrationStore::parseCsvLine(const QString &line)
{
    QStringList fields;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (inQuotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    current.append(QLatin1Char('"'));
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current.append(ch);
            }
        } else if (ch == QLatin1Char('"')) {
            inQuotes = true;
        } else if (ch == QLatin1Char(',')) {
            fields.append(current);
            current.clear();
        } else {
            current.append(ch);
        }
    }
    fields.append(current);
    return fields;
}

void RegistrationStore::reloadFromDisk()
{
    m_latest.clear();

    QFile file(m_csvPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty())
            continue;

        const QStringList cols = parseCsvLine(line);
        if (cols.size() < 5)
            continue;

        RegistrationRecord rec;
        rec.time = QDateTime::fromString(cols.at(0), Qt::ISODate);
        if (!rec.time.isValid())
            rec.time = QDateTime::fromString(cols.at(0), Qt::ISODateWithMs);
        rec.ip = cols.at(1);
        rec.phone = cols.at(2);
        rec.state = regStateFromString(cols.at(3));
        rec.detail = cols.at(4);

        const QString key = pairKey(rec.ip, rec.phone);
        const auto it = m_latest.constFind(key);
        if (it == m_latest.constEnd() || rec.time >= it->time)
            m_latest.insert(key, rec);
    }
}

void RegistrationStore::append(const RegistrationRecord &record)
{
    const bool existed = QFile::exists(m_csvPath);
    QFile file(m_csvPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    const QString timestamp = record.time.toString(Qt::ISODateWithMs);
    const QString line = csvEscapeField(timestamp) + QLatin1Char(',') + csvEscapeField(record.ip)
        + QLatin1Char(',') + csvEscapeField(record.phone) + QLatin1Char(',')
        + csvEscapeField(regStateToString(record.state)) + QLatin1Char(',')
        + csvEscapeField(record.detail) + QLatin1Char('\n');

    file.write(line.toUtf8());
    file.close();

    if (!existed)
        applyOwnerOnlyPermissions(m_csvPath);

    const QString key = pairKey(record.ip, record.phone);
    const auto it = m_latest.constFind(key);
    if (it == m_latest.constEnd() || record.time >= it->time)
        m_latest.insert(key, record);
}

bool RegistrationStore::isRegistered(const QString &ip, const QString &phone) const
{
    const auto it = m_latest.constFind(pairKey(ip, phone));
    if (it == m_latest.constEnd())
        return false;
    return isRegisteredState(it->state);
}

QString RegistrationStore::csvPath() const
{
    return m_csvPath;
}

} // namespace batch
