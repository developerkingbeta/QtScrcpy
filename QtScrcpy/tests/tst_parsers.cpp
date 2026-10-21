#include <QtTest>

#include <QFile>

#include "smsreader.h"
#include "uidump.h"

class TestParsers : public QObject
{
    Q_OBJECT

    QString fixturePath(const char *name) const
    {
        return QFINDTESTDATA(name);
    }

    QString readFixture(const char *name) const
    {
        QFile file(fixturePath(name));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }
        return QString::fromUtf8(file.readAll());
    }

private slots:
    void parsesBoundsAndCenter()
    {
        const QString xml = readFixture("ui_bounds.xml");
        QVERIFY2(!xml.isEmpty(), qPrintable(fixturePath("ui_bounds.xml")));
        const QList<batch::UiNode> nodes = batch::UiDump::parse(xml);
        QCOMPARE(nodes.size(), 1);
        const batch::UiNode &n = nodes.first();
        QCOMPARE(n.text, QStringLiteral("Tap me"));
        QCOMPARE(n.resId, QStringLiteral("com.example:id/btn"));
        QCOMPARE(n.cls, QStringLiteral("android.widget.Button"));
        QCOMPARE(n.bounds, QRect(100, 200, 200, 200));
        QVERIFY(n.clickable);
        QVERIFY(n.enabled);
        QCOMPARE(n.center(), QPoint(200, 300));
    }

    void findAnyMatchesChineseAndEnglish()
    {
        const QList<batch::UiNode> nodes = batch::UiDump::parse(readFixture("ui_find.xml"));

        const batch::UiNode *byEnglish = batch::UiDump::findAny(nodes, {QStringLiteral("register")});
        QVERIFY(byEnglish);
        QCOMPARE(byEnglish->text, QStringLiteral("Register"));

        const batch::UiNode *byChineseDesc = batch::UiDump::findAny(nodes, {QStringLiteral("注册")});
        QVERIFY(byChineseDesc);
        QCOMPARE(byChineseDesc->desc, QStringLiteral("注册"));

        const batch::UiNode *partial = batch::UiDump::findAny(nodes, {QStringLiteral("next")});
        QVERIFY(partial);
        QCOMPARE(partial->text, QStringLiteral("NEXT"));

        const batch::UiNode *exactMiss = batch::UiDump::findAny(nodes, {QStringLiteral("Reg")}, true);
        QVERIFY(!exactMiss);

        const batch::UiNode *exactHit = batch::UiDump::findAny(nodes, {QStringLiteral("Register")}, true);
        QVERIFY(exactHit);
    }

    void editTextsSortedByY()
    {
        const QList<batch::UiNode> nodes = batch::UiDump::parse(readFixture("ui_edits.xml"));
        const QList<batch::UiNode> edits = batch::UiDump::editTexts(nodes);
        QCOMPARE(edits.size(), 3);
        QCOMPARE(edits.at(0).resId, QStringLiteral("nick"));
        QCOMPARE(edits.at(1).resId, QStringLiteral("pass"));
        QCOMPARE(edits.at(2).resId, QStringLiteral("phone"));
        QVERIFY(!edits.at(1).enabled);
    }

    void smsBodyWithCommasParsed()
    {
        const QList<batch::SmsMessage> msgs =
            batch::SmsReader::parseContentQuery(readFixture("sms_commas.txt"));
        QCOMPARE(msgs.size(), 2);
        QCOMPARE(msgs.at(0).address, QStringLiteral("1069000000"));
        QCOMPARE(msgs.at(0).body,
                 QStringLiteral("Hello, dear user, your code is 111222"));
        QCOMPARE(msgs.at(0).dateMs, static_cast<qint64>(1697881200000));
    }

    void codeExtractedForWechatMessage()
    {
        const QList<batch::SmsMessage> msgs =
            batch::SmsReader::parseContentQuery(readFixture("sms_wechat.txt"));
        const QString code = batch::SmsReader::extractWechatCode(msgs, 1697881000000);
        QCOMPARE(code, QStringLiteral("482913"));
    }

    void chineseMessageExtracted()
    {
        const QList<batch::SmsMessage> msgs =
            batch::SmsReader::parseContentQuery(readFixture("sms_chinese.txt"));
        const QString code = batch::SmsReader::extractWechatCode(msgs, 1697881000000);
        QCOMPARE(code, QStringLiteral("135790"));
    }

    void staleCodeIgnored()
    {
        const QList<batch::SmsMessage> msgs =
            batch::SmsReader::parseContentQuery(readFixture("sms_stale.txt"));
        const QString code = batch::SmsReader::extractWechatCode(msgs, 1697881000000);
        QVERIFY(code.isEmpty());
    }

    void nonWechatSixDigitsIgnored()
    {
        const QList<batch::SmsMessage> msgs =
            batch::SmsReader::parseContentQuery(readFixture("sms_commas.txt"));
        const QString code = batch::SmsReader::extractWechatCode(msgs, 1697881000000);
        QVERIFY(code.isEmpty());
    }

    void notificationFallback()
    {
        const QString code =
            batch::SmsReader::extractCodeFromNotifications(readFixture("notifications_wechat.txt"));
        QCOMPARE(code, QStringLiteral("654321"));
    }
};

QTEST_APPLESS_MAIN(TestParsers)
#include "tst_parsers.moc"
