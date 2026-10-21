#include "adbrunner.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>

namespace batch {

namespace {

QString g_configuredAdbPath;

QString resolveAdbPath()
{
    QStringList potentialPaths;
    potentialPaths << QString::fromLocal8Bit(qgetenv("QTSCRCPY_ADB_PATH")) << g_configuredAdbPath;
#ifdef Q_OS_WIN
    potentialPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/adb.exe");
#else
    potentialPaths << QCoreApplication::applicationDirPath() + QStringLiteral("/adb");
#endif

    for (const QString &path : potentialPaths) {
        if (path.isEmpty()) {
            continue;
        }
        const QFileInfo fileInfo(path);
        if (fileInfo.isFile()) {
            return fileInfo.absoluteFilePath();
        }
    }
    return QString();
}

} // namespace

void AdbRunner::setConfiguredAdbPath(const QString &path)
{
    g_configuredAdbPath = path;
}

QString AdbRunner::adbPath()
{
    return resolveAdbPath();
}

AdbResult AdbRunner::run(const QString &serial,
                         const QStringList &args,
                         int timeoutMs,
                         const std::atomic<bool> *cancel)
{
    AdbResult result;

    const QString program = resolveAdbPath();
    if (program.isEmpty()) {
        return result;
    }

    QStringList adbArgs;
    if (!serial.isEmpty()) {
        adbArgs << QStringLiteral("-s") << serial;
    }
    adbArgs << args;

    QProcess process;
    process.setProgram(program);
    process.setArguments(adbArgs);
    process.start();
    if (!process.waitForStarted()) {
        return result;
    }
    result.started = true;

    QElapsedTimer deadline;
    deadline.start();
    const qint64 limitMs = timeoutMs > 0 ? static_cast<qint64>(timeoutMs) : -1;

    while (!process.waitForFinished(100)) {
        if (cancel && cancel->load()) {
            process.kill();
            process.waitForFinished(1000);
            result.cancelled = true;
            result.exitCode = process.exitCode();
            result.out = QString::fromLocal8Bit(process.readAllStandardOutput());
            result.err = QString::fromLocal8Bit(process.readAllStandardError());
            return result;
        }
        if (limitMs >= 0 && deadline.elapsed() >= limitMs) {
            process.kill();
            process.waitForFinished(1000);
            result.timedOut = true;
            result.exitCode = process.exitCode();
            result.out = QString::fromLocal8Bit(process.readAllStandardOutput());
            result.err = QString::fromLocal8Bit(process.readAllStandardError());
            return result;
        }
    }

    result.exitCode = process.exitCode();
    result.out = QString::fromLocal8Bit(process.readAllStandardOutput());
    result.err = QString::fromLocal8Bit(process.readAllStandardError());
    return result;
}

} // namespace batch
