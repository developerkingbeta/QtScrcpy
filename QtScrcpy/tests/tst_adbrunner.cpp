#include <QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTimer>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <thread>

#include "adbrunner.h"

class TestAdbRunner : public QObject
{
    Q_OBJECT

    QString fakeAdbScript() const
    {
        return QFINDTESTDATA("fake_adb.sh");
    }

    void writeFixtureFile(const QString &dirPath, const QString &name, const QByteArray &body) const
    {
        QFile file(dirPath + QLatin1Char('/') + name);
        QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
        QCOMPARE(file.write(body), body.size());
        file.close();
    }

    void useFakeAdb(const QString &fixtureDirPath) const
    {
        const QString script = fakeAdbScript();
        QVERIFY(QFile::exists(script));
        QFile::setPermissions(script,
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                  | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
        qputenv("QTSCRCPY_ADB_PATH", QFileInfo(script).absoluteFilePath().toLocal8Bit());
        qputenv("FAKE_ADB_DIR", QFileInfo(fixtureDirPath).absoluteFilePath().toLocal8Bit());
        batch::AdbRunner::setConfiguredAdbPath(QString());
    }

private slots:
    void runsAndCapturesStdout()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        writeFixtureFile(temp.path(), QStringLiteral("hello.txt"), QByteArray("adb-output-line\n"));
        useFakeAdb(temp.path());

        const batch::AdbResult result = batch::AdbRunner::run(QString(), QStringList{QStringLiteral("hello")}, 5000);
        QVERIFY(result.started);
        QCOMPARE(result.out.trimmed(), QStringLiteral("adb-output-line"));
        QVERIFY(result.ok());
    }

    void nonZeroExitIsNotOk()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        writeFixtureFile(temp.path(), QStringLiteral("fail.txt"), QByteArray("ignored\n"));
        writeFixtureFile(temp.path(), QStringLiteral("fail.code"), QByteArray("7"));
        useFakeAdb(temp.path());

        const batch::AdbResult result = batch::AdbRunner::run(QString(), QStringList{QStringLiteral("fail")}, 5000);
        QVERIFY(result.started);
        QCOMPARE(result.exitCode, 7);
        QVERIFY(!result.ok());
    }

    void timeoutKillsProcess()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        writeFixtureFile(temp.path(), QStringLiteral("slow.sleep"), QByteArray("5"));
        useFakeAdb(temp.path());

        QElapsedTimer elapsed;
        elapsed.start();
        const batch::AdbResult result = batch::AdbRunner::run(QString(), QStringList{QStringLiteral("slow")}, 300);
        QVERIFY(elapsed.elapsed() < 2000);
        QVERIFY(result.started);
        QVERIFY(result.timedOut);
        QVERIFY(!result.cancelled);
        QVERIFY(!result.ok());
    }

    void cancelFlagKillsProcess()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        writeFixtureFile(temp.path(), QStringLiteral("long.sleep"), QByteArray("30"));
        useFakeAdb(temp.path());

        std::atomic<bool> cancel(false);
        batch::AdbResult result;
        std::thread runner([&]() { result = batch::AdbRunner::run(QString(), QStringList{QStringLiteral("long")}, 120000, &cancel); });

        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, [&]() { cancel.store(true); });
        timer.start(200);

        QElapsedTimer wait;
        wait.start();
        while (runner.joinable() && wait.elapsed() < 10000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(10);
        }
        runner.join();

        QVERIFY(result.started);
        QVERIFY(result.cancelled);
        QVERIFY(!result.timedOut);
        QVERIFY(!result.ok());
    }

    void missingBinaryReported()
    {
        qunsetenv("QTSCRCPY_ADB_PATH");
        batch::AdbRunner::setConfiguredAdbPath(QStringLiteral("/no/such/adb/binary"));
        const batch::AdbResult result = batch::AdbRunner::run(QString(), QStringList{QStringLiteral("x")}, 5000);
        QCOMPARE(result.started, false);
    }
};

QTEST_MAIN(TestAdbRunner)
#include "tst_adbrunner.moc"
