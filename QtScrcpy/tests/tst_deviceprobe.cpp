#include <QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "adbrunner.h"
#include "deviceprobe.h"

class TestDeviceProbe : public QObject
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

    batch::DeviceRow sampleRow() const
    {
        batch::DeviceRow row;
        row.ip = QStringLiteral("10.0.0.5");
        row.port = 5555;
        row.phone = QStringLiteral("+919876543210");
        return row;
    }

    QStringList adbArgsWithSerial(const QString &serial, const QStringList &args) const
    {
        QStringList full;
        full << QStringLiteral("-s") << serial << args;
        return full;
    }

private slots:
    void initTestCase()
    {
        if (!QCoreApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "tst_deviceprobe";
            static char *argv[] = { arg0, nullptr };
            new QCoreApplication(argc, argv);
        }
    }

    void connectedDevice()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString serial = QStringLiteral("10.0.0.5:5555");
        writeFixtureForArgs(temp.path(), QStringList{QStringLiteral("connect"), serial}, QByteArray("connected\n"));
        writeFixtureForArgs(temp.path(), adbArgsWithSerial(serial, {QStringLiteral("get-state")}), QByteArray("device\n"));
        writeFixtureForArgs(temp.path(),
                            adbArgsWithSerial(serial, {QStringLiteral("shell"), QStringLiteral("getprop"), QStringLiteral("ro.product.model")}),
                            QByteArray("Pixel 6\n"));
        useFakeAdb(temp.path());

        batch::DeviceRow row = sampleRow();
        QCOMPARE(batch::DeviceProbe::probeConnection(row), batch::ConnState::Connected);
        QCOMPARE(row.detail, QStringLiteral("Pixel 6"));
    }

    void unauthorizedDevice()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString serial = QStringLiteral("10.0.0.5:5555");
        writeFixtureForArgs(temp.path(), QStringList{QStringLiteral("connect"), serial}, QByteArray());
        writeFixtureForArgs(temp.path(), adbArgsWithSerial(serial, {QStringLiteral("get-state")}), QByteArray("unauthorized\n"));
        useFakeAdb(temp.path());

        batch::DeviceRow row = sampleRow();
        QCOMPARE(batch::DeviceProbe::probeConnection(row), batch::ConnState::Unauthorized);
    }

    void offlineDevice()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString serial = QStringLiteral("10.0.0.5:5555");
        writeFixtureForArgs(temp.path(), QStringList{QStringLiteral("connect"), serial}, QByteArray());
        writeFixtureForArgs(temp.path(), adbArgsWithSerial(serial, {QStringLiteral("get-state")}), QByteArray("offline\n"));
        useFakeAdb(temp.path());

        batch::DeviceRow row = sampleRow();
        QCOMPARE(batch::DeviceProbe::probeConnection(row), batch::ConnState::Offline);
    }

    void unreachableTimesOutWithin15s()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString serial = QStringLiteral("10.0.0.5:5555");
        writeFixtureFile(temp.path(), fixtureKey(QStringList{QStringLiteral("connect"), serial}) + QStringLiteral(".sleep"), QByteArray("30"));
        useFakeAdb(temp.path());

        batch::DeviceRow row = sampleRow();
        QElapsedTimer elapsed;
        elapsed.start();
        QCOMPARE(batch::DeviceProbe::probeConnection(row), batch::ConnState::Unreachable);
        QVERIFY(elapsed.elapsed() < 15000);
    }

    void wechatInstalledWithVersion()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString serial = QStringLiteral("10.0.0.5:5555");
        writeFixtureForArgs(temp.path(),
                            adbArgsWithSerial(serial, {QStringLiteral("shell"), QStringLiteral("pm"), QStringLiteral("path"), QStringLiteral("com.tencent.mm")}),
                            QByteArray("package:/data/app/com.tencent.mm/base.apk\n"));
        writeFixtureForArgs(temp.path(),
                            adbArgsWithSerial(serial,
                                              {QStringLiteral("shell"),
                                               QStringLiteral("dumpsys"),
                                               QStringLiteral("package"),
                                               QStringLiteral("com.tencent.mm")}),
                            QByteArray("versionName=8.0.45\n"));
        useFakeAdb(temp.path());

        batch::DeviceRow row = sampleRow();
        QCOMPARE(batch::DeviceProbe::probeWechat(row), batch::WechatState::Installed);
        QCOMPARE(row.wechatVersion, QStringLiteral("8.0.45"));
    }

    void wechatMissing()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString serial = QStringLiteral("10.0.0.5:5555");
        writeFixtureForArgs(temp.path(),
                            adbArgsWithSerial(serial, {QStringLiteral("shell"), QStringLiteral("pm"), QStringLiteral("path"), QStringLiteral("com.tencent.mm")}),
                            QByteArray());
        useFakeAdb(temp.path());

        batch::DeviceRow row = sampleRow();
        QCOMPARE(batch::DeviceProbe::probeWechat(row), batch::WechatState::Missing);
    }

    void installSuccess()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QTemporaryFile apk(temp.path() + QStringLiteral("/wechat.apk"));
        QVERIFY(apk.open());
        apk.write("PK", 2);
        apk.close();

        const QString serial = QStringLiteral("10.0.0.5:5555");
        const QStringList installArgs = {QStringLiteral("install"), QStringLiteral("-r"), apk.fileName()};
        writeFixtureForArgs(temp.path(), adbArgsWithSerial(serial, installArgs), QByteArray("Success\n"));
        useFakeAdb(temp.path());

        batch::DeviceRow row = sampleRow();
        QString err;
        QVERIFY(batch::DeviceProbe::installWechat(row, apk.fileName(), &err));
        QVERIFY(err.isEmpty());
        QCOMPARE(row.wechat, batch::WechatState::Installed);
    }

    void installNoMatchingAbis()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QTemporaryFile apk(temp.path() + QStringLiteral("/wechat.apk"));
        QVERIFY(apk.open());
        apk.write("PK", 2);
        apk.close();

        const QString serial = QStringLiteral("10.0.0.5:5555");
        const QStringList installArgs = {QStringLiteral("install"), QStringLiteral("-r"), apk.fileName()};
        writeFixtureForArgs(temp.path(),
                            adbArgsWithSerial(serial, installArgs),
                            QByteArray("Failure [INSTALL_FAILED_NO_MATCHING_ABIS]\n"));
        writeFixtureFile(temp.path(),
                         fixtureKey(adbArgsWithSerial(serial, installArgs)) + QStringLiteral(".code"),
                         QByteArray("1"));
        useFakeAdb(temp.path());

        batch::DeviceRow row = sampleRow();
        QString err;
        QVERIFY(!batch::DeviceProbe::installWechat(row, apk.fileName(), &err));
        QVERIFY(err.contains(QStringLiteral("ABI")));
        QCOMPARE(row.wechat, batch::WechatState::InstallFailed);
    }

    void installRejectsMissingApkFile()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());

        const QString markerScript = temp.path() + QStringLiteral("/marker_adb.sh");
        {
            QFile script(markerScript);
            QVERIFY(script.open(QIODevice::WriteOnly));
            const QByteArray body = QByteArray(
                "#!/usr/bin/env bash\n"
                "echo invoked > \"${MARKER_FILE}\"\n"
                "exec \"${REAL_FAKE}\"\n");
            QCOMPARE(script.write(body), body.size());
            script.close();
        }
        QFile::setPermissions(markerScript,
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                  | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);

        const QString markerFile = temp.path() + QStringLiteral("/adb_invoked");
        qputenv("MARKER_FILE", markerFile.toLocal8Bit());
        qputenv("REAL_FAKE", QFileInfo(fakeAdbScript()).absoluteFilePath().toLocal8Bit());
        qputenv("QTSCRCPY_ADB_PATH", markerScript.toLocal8Bit());
        qputenv("FAKE_ADB_DIR", temp.path().toLocal8Bit());
        batch::AdbRunner::setConfiguredAdbPath(QString());

        batch::DeviceRow row = sampleRow();
        QString err;
        const QString missingApk = temp.path() + QStringLiteral("/does-not-exist.apk");
        QVERIFY(!batch::DeviceProbe::installWechat(row, missingApk, &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(!QFile::exists(markerFile));
    }
};

QTEST_MAIN(TestDeviceProbe)
#include "tst_deviceprobe.moc"
