#include "batchsettings.h"

#include <QDir>
#include <QSettings>

namespace batch {

namespace {

const char kSettingsFileName[] = "settings.ini";
const char kApkPathKey[] = "apkPath";
const char kConcurrencyKey[] = "concurrency";
const int kDefaultConcurrency = 3;
const int kMinConcurrency = 1;
const int kMaxConcurrency = 8;

} // namespace

BatchSettings::BatchSettings(const QString &dir)
    : m_settingsPath(QDir(dir).filePath(QString::fromLatin1(kSettingsFileName)))
{
    QDir().mkpath(dir);
}

int BatchSettings::clampConcurrency(int value)
{
    if (value < kMinConcurrency)
        return kMinConcurrency;
    if (value > kMaxConcurrency)
        return kMaxConcurrency;
    return value;
}

QString BatchSettings::apkPath() const
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    return settings.value(QString::fromLatin1(kApkPathKey)).toString();
}

void BatchSettings::setApkPath(const QString &path)
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.setValue(QString::fromLatin1(kApkPathKey), path);
    settings.sync();
}

int BatchSettings::concurrency() const
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    const QVariant v = settings.value(QString::fromLatin1(kConcurrencyKey), kDefaultConcurrency);
    return clampConcurrency(v.toInt());
}

void BatchSettings::setConcurrency(int value)
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.setValue(QString::fromLatin1(kConcurrencyKey), clampConcurrency(value));
    settings.sync();
}

} // namespace batch
