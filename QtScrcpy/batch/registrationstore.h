#ifndef BATCH_REGISTRATIONSTORE_H
#define BATCH_REGISTRATIONSTORE_H

#include <QDateTime>
#include <QHash>
#include <QString>

#include "devicerow.h"

namespace batch {

struct RegistrationRecord {
    QDateTime time;
    QString ip;
    QString phone;
    RegState state = RegState::Idle;
    QString detail;
};

class RegistrationStore
{
public:
    explicit RegistrationStore(const QString &dir);

    void append(const RegistrationRecord &record);
    bool isRegistered(const QString &ip, const QString &phone) const;
    QString csvPath() const;

private:
    QString pairKey(const QString &ip, const QString &phone) const;
    void reloadFromDisk();
    static QString regStateToString(RegState state);
    static RegState regStateFromString(const QString &text);
    static QString csvEscapeField(const QString &field);
    static QStringList parseCsvLine(const QString &line);

    QString m_dir;
    QString m_csvPath;
    QHash<QString, RegistrationRecord> m_latest;
};

} // namespace batch

#endif // BATCH_REGISTRATIONSTORE_H
