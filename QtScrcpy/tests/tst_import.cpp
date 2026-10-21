#include <QtTest>

#include "deviceimport.h"
#include "hostcheck.h"
#include "sheetreader.h"

class TestImport : public QObject
{
    Q_OBJECT

    QString fixturePath(const char *name) const
    {
        return QFINDTESTDATA(name);
    }

private slots:
    void readsXlsxBasic()
    {
        QString err;
        const batch::SheetTable table = batch::SheetReader::read(fixturePath("devices_ok.xlsx"), &err);
        QCOMPARE(err, QString());
        QVERIFY(!table.isEmpty());

        const batch::ImportResult result = batch::DeviceImport::fromTable(table);
        QCOMPARE(result.error, QString());
        QCOMPARE(result.rows.size(), 3);
        for (const batch::DeviceRow &row : result.rows) {
            QCOMPARE(row.port, quint16(5555));
            QVERIFY(batch::HostCheck::isIPv4Literal(row.ip));
            QVERIFY(!row.phone.isEmpty());
        }
    }

    void readsCsvSameAsXlsx()
    {
        QString xlsxErr;
        QString csvErr;
        const batch::SheetTable xlsxTable =
            batch::SheetReader::read(fixturePath("devices_ok.xlsx"), &xlsxErr);
        const batch::SheetTable csvTable =
            batch::SheetReader::read(fixturePath("devices_ok.csv"), &csvErr);
        QCOMPARE(xlsxErr, QString());
        QCOMPARE(csvErr, QString());

        const batch::ImportResult xlsxResult = batch::DeviceImport::fromTable(xlsxTable);
        const batch::ImportResult csvResult = batch::DeviceImport::fromTable(csvTable);
        QCOMPARE(xlsxResult.error, QString());
        QCOMPARE(csvResult.error, QString());
        QCOMPARE(csvResult.rows.size(), xlsxResult.rows.size());
        for (int i = 0; i < xlsxResult.rows.size(); ++i) {
            QCOMPARE(csvResult.rows.at(i).ip, xlsxResult.rows.at(i).ip);
            QCOMPARE(csvResult.rows.at(i).phone, xlsxResult.rows.at(i).phone);
            QCOMPARE(csvResult.rows.at(i).port, xlsxResult.rows.at(i).port);
        }
    }

    void numericPhoneCellsNormalised()
    {
        QString err;
        const batch::SheetTable table =
            batch::SheetReader::read(fixturePath("devices_numeric_phone.xlsx"), &err);
        QCOMPARE(err, QString());

        const batch::ImportResult result = batch::DeviceImport::fromTable(table);
        QCOMPARE(result.error, QString());
        QCOMPARE(result.rows.size(), 2);
        QCOMPARE(result.rows.at(0).phone, QStringLiteral("9876500000"));
        QCOMPARE(result.rows.at(1).phone, QStringLiteral("+919876543210"));
    }

    void duplicateIpSkippedWithWarning()
    {
        batch::SheetTable table;
        table << (QStringList() << QStringLiteral("tailscale_ip") << QStringLiteral("phone"));
        table << (QStringList() << QStringLiteral("100.64.0.1") << QStringLiteral("1111111111"));
        table << (QStringList() << QStringLiteral("100.64.0.1") << QStringLiteral("3333333333"));
        table << (QStringList() << QStringLiteral("100.64.0.2") << QStringLiteral("2222222222"));

        const batch::ImportResult result = batch::DeviceImport::fromTable(table);
        QCOMPARE(result.error, QString());
        QCOMPARE(result.rows.size(), 2);
        bool foundRow3 = false;
        for (const QString &w : result.warnings) {
            if (w.contains(QStringLiteral("row 3"))) {
                foundRow3 = true;
                break;
            }
        }
        QVERIFY(foundRow3);
    }

    void duplicatePhoneSkippedWithWarning()
    {
        batch::SheetTable table;
        table << (QStringList() << QStringLiteral("ip") << QStringLiteral("mobile"));
        table << (QStringList() << QStringLiteral("100.64.0.10") << QStringLiteral("4444444444"));
        table << (QStringList() << QStringLiteral("100.64.0.11") << QStringLiteral("5555555555"));
        table << (QStringList() << QStringLiteral("100.64.0.12") << QStringLiteral("4444444444"));

        const batch::ImportResult result = batch::DeviceImport::fromTable(table);
        QCOMPARE(result.error, QString());
        QCOMPARE(result.rows.size(), 2);
        bool foundDuplicatePhone = false;
        for (const QString &w : result.warnings) {
            if (w.contains(QStringLiteral("row 4"))) {
                foundDuplicatePhone = true;
                break;
            }
        }
        QVERIFY(foundDuplicatePhone);
    }

    void publicIpRowRejected()
    {
        batch::SheetTable table;
        table << (QStringList() << QStringLiteral("device ip") << QStringLiteral("phone"));
        table << (QStringList() << QStringLiteral("100.64.0.20") << QStringLiteral("6666666666"));
        table << (QStringList() << QStringLiteral("8.8.8.8") << QStringLiteral("7777777777"));

        const batch::ImportResult result = batch::DeviceImport::fromTable(table);
        QCOMPARE(result.error, QString());
        QCOMPARE(result.rows.size(), 1);
        QCOMPARE(result.rows.at(0).ip, QStringLiteral("100.64.0.20"));
        QVERIFY(!result.warnings.isEmpty());
    }

    void missingPhoneHeaderErrors()
    {
        batch::SheetTable table;
        table << (QStringList() << QStringLiteral("tailscale_ip") << QStringLiteral("port"));
        table << (QStringList() << QStringLiteral("100.64.0.30") << QStringLiteral("5555"));

        const batch::ImportResult result = batch::DeviceImport::fromTable(table);
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(result.rows.size(), 0);
    }

    void xlsRejectedWithMessage()
    {
        QTemporaryFile xlsFile;
        QVERIFY(xlsFile.open());
        xlsFile.write("dummy xls content");
        xlsFile.close();

        const QString path = xlsFile.fileName();
        const QString renamed = path + QStringLiteral(".xls");
        QVERIFY(QFile::rename(path, renamed));

        QString err;
        const batch::SheetTable table = batch::SheetReader::read(renamed, &err);
        Q_UNUSED(table);
        QVERIFY2(!err.isEmpty(), "expected error for .xls");
        QVERIFY2(err.contains(QStringLiteral("Save as .xlsx")), qPrintable(err));
        QFile::remove(renamed);
    }
};

QTEST_MAIN(TestImport)
#include "tst_import.moc"
