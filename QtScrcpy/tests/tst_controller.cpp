#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <atomic>

#include <QSet>
#include <QThread>

#include "adbrunner.h"
#include "batchcontroller.h"
#include "batchsettings.h"
#include "registrationstore.h"

class TestBatchController : public QObject
{
    Q_OBJECT

    static QString fixtureKey(const QStringList &adbArgv)
    {
        QString key = adbArgv.join(QLatin1Char(' '));
        key.replace(QLatin1Char(' '), QLatin1Char('_'));
        key.replace(QLatin1Char('/'), QLatin1Char('_'));
        key.replace(QLatin1Char(':'), QLatin1Char('_'));
        return key;
    }

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

    void writeFixtureForArgs(const QString &dirPath, const QStringList &adbArgv, const QByteArray &body) const
    {
        writeFixtureFile(dirPath, fixtureKey(adbArgv) + QStringLiteral(".txt"), body);
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

    QStringList adbArgsWithSerial(const QString &serial, const QStringList &args) const
    {
        QStringList full;
        full << QStringLiteral("-s") << serial << args;
        return full;
    }

    void writeConnectedFixtures(const QString &dirPath,
                                const QString &ip,
                                quint16 port,
                                bool wechatInstalled,
                                const QString &version = QStringLiteral("8.0.45")) const
    {
        const QString serial = ip + QLatin1Char(':') + QString::number(port);
        writeFixtureForArgs(dirPath, QStringList{QStringLiteral("connect"), serial}, QByteArray("connected\n"));
        writeFixtureForArgs(dirPath, adbArgsWithSerial(serial, {QStringLiteral("get-state")}), QByteArray("device\n"));
        writeFixtureForArgs(dirPath,
                            adbArgsWithSerial(serial,
                                              {QStringLiteral("shell"),
                                               QStringLiteral("getprop"),
                                               QStringLiteral("ro.product.model")}),
                            QByteArray("Pixel\n"));
        if (wechatInstalled) {
            writeFixtureForArgs(dirPath,
                                adbArgsWithSerial(serial,
                                                  {QStringLiteral("shell"),
                                                   QStringLiteral("pm"),
                                                   QStringLiteral("path"),
                                                   QStringLiteral("com.tencent.mm")}),
                                QByteArray("package:/data/app/com.tencent.mm/base.apk\n"));
            writeFixtureForArgs(dirPath,
                                adbArgsWithSerial(serial,
                                                  {QStringLiteral("shell"),
                                                   QStringLiteral("dumpsys"),
                                                   QStringLiteral("package"),
                                                   QStringLiteral("com.tencent.mm")}),
                                QByteArray("versionName=" + version.toUtf8() + "\n"));
        } else {
            writeFixtureForArgs(dirPath,
                                adbArgsWithSerial(serial,
                                                  {QStringLiteral("shell"),
                                                   QStringLiteral("pm"),
                                                   QStringLiteral("path"),
                                                   QStringLiteral("com.tencent.mm")}),
                                QByteArray());
        }
    }

    void writeUnreachableFixtures(const QString &dirPath, const QString &ip, quint16 port) const
    {
        const QString serial = ip + QLatin1Char(':') + QString::number(port);
        writeFixtureFile(dirPath,
                         fixtureKey(QStringList{QStringLiteral("connect"), serial}) + QStringLiteral(".sleep"),
                         QByteArray("30"));
    }

    void waitUntilIdle(batch::BatchController &controller) const
    {
        if (!controller.isBusy()) {
            return;
        }
        QSignalSpy spy(&controller, &batch::BatchController::busyChanged);
        for (int i = 0; i < 120 && controller.isBusy(); ++i) {
            if (spy.wait(500)) {
                continue;
            }
            QApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        QVERIFY2(!controller.isBusy(), "controller still busy after wait");
    }

    QString sheetPath() const
    {
        return QFINDTESTDATA("devices_ok.csv");
    }

private slots:
    void initTestCase()
    {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "tst_controller";
            static char *argv[] = { arg0, nullptr };
            new QApplication(argc, argv);
        }
        batch::BatchController::clearRegistrarFactory();
    }

    void cleanup()
    {
        batch::BatchController::clearRegistrarFactory();
    }

    void loadSheetChecksAllDevices()
    {
        QTemporaryDir dataDir;
        QVERIFY(dataDir.isValid());
        QTemporaryDir adbDir;
        QVERIFY(adbDir.isValid());

        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.1"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.2"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.3"), 5555, true);
        useFakeAdb(adbDir.path());

        batch::BatchController controller(dataDir.path());
        QSignalSpy summarySpy(&controller, &batch::BatchController::summaryChanged);

        QString err;
        QVERIFY(controller.loadSheet(sheetPath(), &err));
        QCOMPARE(err, QString());
        waitUntilIdle(controller);

        QCOMPARE(controller.model()->rowCount(), 3);
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(controller.model()->row(i).conn, batch::ConnState::Connected);
        }

        QVERIFY(summarySpy.count() >= 1);
        const QList<QVariant> last = summarySpy.last();
        QCOMPARE(last.at(0).toInt(), 3);
        QCOMPARE(last.at(1).toInt(), 3);
    }

    void partialConnectionBlocksOnlyThoseRows()
    {
        QTemporaryDir dataDir;
        QTemporaryDir adbDir;
        QVERIFY(dataDir.isValid() && adbDir.isValid());

        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.1"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.2"), 5555, true);
        writeUnreachableFixtures(adbDir.path(), QStringLiteral("100.64.1.3"), 5555);
        useFakeAdb(adbDir.path());

        QSet<QString> registrarSerials;
        batch::BatchController::setRegistrarFactory(
            [&registrarSerials](const batch::DeviceRow &row, const std::atomic<bool> *) {
                registrarSerials.insert(row.serial());
                batch::RegisterOutcome out;
                out.state = batch::RegState::Registered;
                return out;
            });

        batch::BatchController controller(dataDir.path());
        QString err;
        QVERIFY(controller.loadSheet(sheetPath(), &err));
        waitUntilIdle(controller);

        QTemporaryFile apk(dataDir.path() + QStringLiteral("/wechat.apk"));
        QVERIFY(apk.open());
        apk.write("PK", 2);
        apk.close();

        controller.ensureWechat(apk.fileName());
        waitUntilIdle(controller);

        controller.registerRows({0, 1, 2});
        waitUntilIdle(controller);

        QVERIFY(!registrarSerials.contains(QStringLiteral("100.64.1.3:5555")));
        QCOMPARE(registrarSerials.size(), 2);
        QCOMPARE(controller.model()->row(2).conn, batch::ConnState::Unreachable);
    }

    void ensureWechatInstallsOnlyMissing()
    {
        QTemporaryDir dataDir;
        QTemporaryDir adbDir;
        QVERIFY(dataDir.isValid() && adbDir.isValid());

        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.1"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.2"), 5555, false);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.3"), 5555, false);

        QTemporaryFile apk(dataDir.path() + QStringLiteral("/wechat.apk"));
        QVERIFY(apk.open());
        apk.write("PK", 2);
        apk.close();

        const QString serial2 = QStringLiteral("100.64.1.2:5555");
        const QString serial3 = QStringLiteral("100.64.1.3:5555");
        const QStringList installArgs2 = {QStringLiteral("install"), QStringLiteral("-r"), apk.fileName()};
        writeFixtureForArgs(adbDir.path(), adbArgsWithSerial(serial2, installArgs2), QByteArray("Success\n"));
        writeFixtureForArgs(adbDir.path(), adbArgsWithSerial(serial3, installArgs2), QByteArray("Success\n"));
        useFakeAdb(adbDir.path());

        batch::BatchController controller(dataDir.path());
        QVERIFY(controller.loadSheet(sheetPath(), nullptr));
        waitUntilIdle(controller);

        controller.ensureWechat(apk.fileName());
        waitUntilIdle(controller);

        QCOMPARE(controller.model()->row(0).wechat, batch::WechatState::Installed);
        QCOMPARE(controller.model()->row(1).wechat, batch::WechatState::Installed);
        QCOMPARE(controller.model()->row(2).wechat, batch::WechatState::Installed);
    }

    void registerSkipsNotInstalledAndAlreadyRegistered()
    {
        QTemporaryDir dataDir;
        QTemporaryDir adbDir;
        QVERIFY(dataDir.isValid() && adbDir.isValid());

        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.1"), 5555, false);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.2"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.3"), 5555, true);
        useFakeAdb(adbDir.path());

        batch::RegistrationStore store(dataDir.path());
        batch::RegistrationRecord existing;
        existing.time = QDateTime::currentDateTimeUtc();
        existing.ip = QStringLiteral("100.64.1.2");
        existing.phone = QStringLiteral("9222222222");
        existing.state = batch::RegState::Registered;
        store.append(existing);

        QSet<QString> touched;
        batch::BatchController::setRegistrarFactory(
            [&touched](const batch::DeviceRow &row, const std::atomic<bool> *) {
                touched.insert(row.serial());
                batch::RegisterOutcome out;
                out.state = batch::RegState::Registered;
                return out;
            });

        batch::BatchController controller(dataDir.path());
        QSignalSpy logSpy(&controller, &batch::BatchController::logLine);
        QVERIFY(controller.loadSheet(sheetPath(), nullptr));
        waitUntilIdle(controller);

        controller.registerRows({0, 1, 2});
        waitUntilIdle(controller);

        QVERIFY(touched.contains(QStringLiteral("100.64.1.3:5555")));
        QVERIFY(!touched.contains(QStringLiteral("100.64.1.1:5555")));
        QVERIFY(!touched.contains(QStringLiteral("100.64.1.2:5555")));

        bool sawSkipMissing = false;
        bool sawSkipRegistered = false;
        for (int i = 0; i < logSpy.count(); ++i) {
            const QString line = logSpy.at(i).at(0).toString();
            if (line.contains(QStringLiteral("100.64.1.1")) && line.contains(QStringLiteral("skipped"))) {
                sawSkipMissing = true;
            }
            if (line.contains(QStringLiteral("100.64.1.2")) && line.contains(QStringLiteral("skipped"))) {
                sawSkipRegistered = true;
            }
        }
        QVERIFY(sawSkipMissing);
        QVERIFY(sawSkipRegistered);
    }

    void registeredRowWritesLogAndStore()
    {
        QTemporaryDir dataDir;
        QTemporaryDir adbDir;
        QVERIFY(dataDir.isValid() && adbDir.isValid());

        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.1"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.2"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.3"), 5555, true);
        useFakeAdb(adbDir.path());

        batch::BatchController::setRegistrarFactory([](const batch::DeviceRow &row, const std::atomic<bool> *) {
            batch::RegisterOutcome out;
            if (row.ip == QStringLiteral("100.64.1.1")) {
                out.state = batch::RegState::Registered;
            } else {
                out.state = batch::RegState::Failed;
            }
            return out;
        });

        batch::BatchController controller(dataDir.path());
        QSignalSpy logSpy(&controller, &batch::BatchController::logLine);
        QVERIFY(controller.loadSheet(sheetPath(), nullptr));
        waitUntilIdle(controller);

        controller.registerRows({0});
        waitUntilIdle(controller);

        bool sawRegisteredLog = false;
        for (int i = 0; i < logSpy.count(); ++i) {
            const QString line = logSpy.at(i).at(0).toString();
            if (line.startsWith(QStringLiteral("[REGISTERED] 100.64.1.1:5555 9111111111"))) {
                sawRegisteredLog = true;
            }
        }
        QVERIFY(sawRegisteredLog);

        batch::RegistrationStore store(dataDir.path());
        QVERIFY(store.isRegistered(QStringLiteral("100.64.1.1"), QStringLiteral("9111111111")));
    }

    void cancelLeavesNoRunningRows()
    {
        QTemporaryDir dataDir;
        QTemporaryDir adbDir;
        QVERIFY(dataDir.isValid() && adbDir.isValid());

        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.1"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.2"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.3"), 5555, true);
        useFakeAdb(adbDir.path());

        batch::BatchController::setRegistrarFactory([](const batch::DeviceRow &, const std::atomic<bool> *cancel) {
            for (int i = 0; i < 200 && (!cancel || !cancel->load()); ++i) {
                QThread::msleep(25);
            }
            batch::RegisterOutcome out;
            out.state = batch::RegState::Cancelled;
            return out;
        });

        batch::BatchController controller(dataDir.path());
        QVERIFY(controller.loadSheet(sheetPath(), nullptr));
        waitUntilIdle(controller);

        controller.registerRows({0, 1, 2});
        QTRY_VERIFY_WITH_TIMEOUT(controller.isBusy(), 5000);
        controller.cancel();
        waitUntilIdle(controller);

        for (int i = 0; i < controller.model()->rowCount(); ++i) {
            QVERIFY(controller.model()->row(i).reg != batch::RegState::Running);
        }
    }

    void secondRunRejectedWhileBusy()
    {
        QTemporaryDir dataDir;
        QTemporaryDir adbDir;
        QVERIFY(dataDir.isValid() && adbDir.isValid());

        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.1"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.2"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.3"), 5555, true);
        useFakeAdb(adbDir.path());

        int probePasses = 0;
        batch::BatchController::setRegistrarFactory([&probePasses](const batch::DeviceRow &, const std::atomic<bool> *) {
            ++probePasses;
            batch::RegisterOutcome out;
            out.state = batch::RegState::Registered;
            return out;
        });

        batch::BatchController controller(dataDir.path());
        QVERIFY(controller.loadSheet(sheetPath(), nullptr));
        QTRY_VERIFY_WITH_TIMEOUT(controller.isBusy(), 5000);

        controller.checkAll();
        controller.registerRows({0, 1, 2});

        waitUntilIdle(controller);
        QCOMPARE(probePasses, 0);
    }

    void concurrencyLimitRespected()
    {
        QTemporaryDir dataDir;
        QTemporaryDir adbDir;
        QVERIFY(dataDir.isValid() && adbDir.isValid());

        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.1"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.2"), 5555, true);
        writeConnectedFixtures(adbDir.path(), QStringLiteral("100.64.1.3"), 5555, true);
        useFakeAdb(adbDir.path());

        batch::BatchSettings settings(dataDir.path());
        settings.setConcurrency(2);

        std::atomic<int> inFlight(0);
        std::atomic<int> maxInFlight(0);

        batch::BatchController::setRegistrarFactory(
            [&inFlight, &maxInFlight](const batch::DeviceRow &, const std::atomic<bool> *cancel) {
                const int now = ++inFlight;
                int observed = maxInFlight.load();
                while (observed < now && !maxInFlight.compare_exchange_weak(observed, now)) {
                }
                for (int i = 0; i < 40 && (!cancel || !cancel->load()); ++i) {
                    QThread::msleep(25);
                }
                --inFlight;
                batch::RegisterOutcome out;
                out.state = batch::RegState::Registered;
                return out;
            });

        batch::BatchController controller(dataDir.path());
        QVERIFY(controller.loadSheet(sheetPath(), nullptr));
        waitUntilIdle(controller);

        controller.registerRows({0, 1, 2});
        waitUntilIdle(controller);

        QVERIFY(maxInFlight.load() <= 2);
    }
};

QTEST_MAIN(TestBatchController)
#include "tst_controller.moc"
