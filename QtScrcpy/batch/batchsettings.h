#ifndef BATCH_BATCHSETTINGS_H
#define BATCH_BATCHSETTINGS_H

#include <QString>

namespace batch {

class BatchSettings
{
public:
    explicit BatchSettings(const QString &dir);

    QString apkPath() const;
    void setApkPath(const QString &path);

    int concurrency() const;
    void setConcurrency(int value);

private:
    static int clampConcurrency(int value);

    QString m_settingsPath;
};

} // namespace batch

#endif // BATCH_BATCHSETTINGS_H
