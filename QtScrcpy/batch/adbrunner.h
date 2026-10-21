#ifndef BATCH_ADBRUNNER_H
#define BATCH_ADBRUNNER_H

#include <atomic>

#include <QString>
#include <QStringList>

namespace batch {

struct AdbResult
{
    bool started = false;
    bool timedOut = false;
    bool cancelled = false;
    int exitCode = -1;
    QString out;
    QString err;

    bool ok() const
    {
        return started && !timedOut && !cancelled && exitCode == 0;
    }
};

class AdbRunner
{
public:
    static void setConfiguredAdbPath(const QString &path);
    static QString adbPath();
    static AdbResult run(const QString &serial,
                         const QStringList &args,
                         int timeoutMs,
                         const std::atomic<bool> *cancel = nullptr);

private:
    AdbRunner();
};

} // namespace batch

#endif // BATCH_ADBRUNNER_H
