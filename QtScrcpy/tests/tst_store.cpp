#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "batchsettings.h"
#include "registrationstore.h"

class TestStore : public QObject
{
    Q_OBJECT

private slots:
    void appendThenIsRegistered()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        batch::RegistrationStore store(dir.path());
        batch::RegistrationRecord rec;
        rec.time = QDateTime::fromString(QStringLiteral("2026-10-21T12:00:00"), Qt::ISODate);
        rec.ip = QStringLiteral("203.0.113.10");
        rec.phone = QStringLiteral("13800138000");
        rec.state = batch::RegState::Registered;
        rec.detail = QStringLiteral("ok");

        store.append(rec);
        QVERIFY(store.isRegistered(rec.ip, rec.phone));
        QVERIFY(!store.isRegistered(rec.ip, QStringLiteral("13900139000")));
    }

    void latestRecordWins()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        batch::RegistrationStore store(dir.path());
        const QString ip = QStringLiteral("203.0.113.11");
        const QString phone = QStringLiteral("13800138001");

        batch::RegistrationRecord ok;
        ok.time = QDateTime::fromString(QStringLiteral("2026-10-21T12:00:00"), Qt::ISODate);
        ok.ip = ip;
        ok.phone = phone;
        ok.state = batch::RegState::Registered;
        ok.detail = QStringLiteral("first");
        store.append(ok);

        batch::RegistrationRecord fail;
        fail.time = QDateTime::fromString(QStringLiteral("2026-10-21T13:00:00"), Qt::ISODate);
        fail.ip = ip;
        fail.phone = phone;
        fail.state = batch::RegState::Failed;
        fail.detail = QStringLiteral("retry failed");
        store.append(fail);

        QVERIFY(!store.isRegistered(ip, phone));
    }

    void csvEscapesCommasAndQuotes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        batch::RegistrationStore store(dir.path());
        batch::RegistrationRecord rec;
        rec.time = QDateTime::fromString(QStringLiteral("2026-10-21T12:00:00"), Qt::ISODate);
        rec.ip = QStringLiteral("203.0.113.12");
        rec.phone = QStringLiteral("13800138002");
        rec.state = batch::RegState::AlreadyRegistered;
        rec.detail = QStringLiteral("said \"hello\", world");

        store.append(rec);

        QFile file(store.csvPath());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray raw = file.readAll();
        file.close();

        QVERIFY(raw.contains("said \"\"hello\"\", world"));
        QVERIFY(!raw.contains("said \"hello\", world"));
    }

    void fileIsOwnerOnly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        batch::RegistrationStore store(dir.path());
        batch::RegistrationRecord rec;
        rec.time = QDateTime::currentDateTimeUtc();
        rec.ip = QStringLiteral("203.0.113.13");
        rec.phone = QStringLiteral("13800138003");
        rec.state = batch::RegState::Registered;
        rec.detail = QStringLiteral("perm");
        store.append(rec);

        const QFile::Permissions perms = QFile::permissions(store.csvPath());
        QVERIFY(perms & QFile::ReadOwner);
        QVERIFY(perms & QFile::WriteOwner);
        QVERIFY(!(perms & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup
                           | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther)));
    }

    void survivesReopen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString ip = QStringLiteral("203.0.113.14");
        const QString phone = QStringLiteral("13800138004");

        {
            batch::RegistrationStore store(dir.path());
            batch::RegistrationRecord rec;
            rec.time = QDateTime::fromString(QStringLiteral("2026-10-21T14:00:00"), Qt::ISODate);
            rec.ip = ip;
            rec.phone = phone;
            rec.state = batch::RegState::AlreadyRegistered;
            rec.detail = QStringLiteral("persisted");
            store.append(rec);
        }

        batch::RegistrationStore reopened(dir.path());
        QVERIFY(reopened.isRegistered(ip, phone));
    }

    void concurrencyClamped()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        batch::BatchSettings settings(dir.path());
        QCOMPARE(settings.concurrency(), 3);

        settings.setConcurrency(0);
        QCOMPARE(settings.concurrency(), 1);

        settings.setConcurrency(99);
        QCOMPARE(settings.concurrency(), 8);

        batch::BatchSettings reloaded(dir.path());
        QCOMPARE(reloaded.concurrency(), 8);
    }
};

QTEST_MAIN(TestStore)
#include "tst_store.moc"
