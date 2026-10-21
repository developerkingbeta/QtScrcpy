#include <QtTest>

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include "wechatregistrar.h"

namespace {

class FakeDriver : public batch::IUiDriver
{
public:
    explicit FakeDriver(const QString &flowDir)
        : m_cancel(false)
        , m_nowMs(1697881200000LL)
        , m_tapCount(0)
        , m_index(-1)
    {
        QDir dir(flowDir);
        const QStringList entries = dir.entryList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
        for (const QString &name : entries) {
            QFile file(dir.filePath(name));
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_screens.append(QString::fromUtf8(file.readAll()));
            }
        }
    }

    QList<batch::UiNode> dump() override
    {
        return batch::UiDump::parse(currentXml());
    }

    bool tap(int, int) override
    {
        ++m_tapCount;
        advance();
        return true;
    }

    bool typeText(const QString &asciiText) override
    {
        m_typedTexts.append(asciiText);
        advance();
        return true;
    }

    bool pressBack() override
    {
        advance();
        return true;
    }

    bool launchWechat() override
    {
        advance();
        return true;
    }

    QList<batch::SmsMessage> readSms() override { return m_sms; }

    QString notificationDump() override { return m_notifications; }

    qint64 deviceNowMs() override { return m_nowMs; }

    void sleepMs(int ms) override { m_nowMs += ms; }

    bool cancelled() const override { return m_cancel.load(); }

    void setCancelled(bool value) { m_cancel.store(value); }

    void setSms(const QList<batch::SmsMessage> &sms) { m_sms = sms; }

    int tapCount() const { return m_tapCount; }

    QStringList typedTexts() const { return m_typedTexts; }

    void setNowMs(qint64 ms) { m_nowMs = ms; }

private:
    QString currentXml() const
    {
        if (m_screens.isEmpty()) {
            return QString();
        }
        const int idx = qBound(0, m_index, m_screens.size() - 1);
        return m_screens.at(idx);
    }

    void advance()
    {
        if (m_index + 1 < m_screens.size()) {
            ++m_index;
        }
    }

    QStringList m_screens;
    QStringList m_typedTexts;
    QList<batch::SmsMessage> m_sms;
    QString m_notifications;
    std::atomic<bool> m_cancel;
    qint64 m_nowMs;
    int m_tapCount;
    int m_index;
};

QString flowPath(const char *name)
{
    return QFINDTESTDATA(name);
}

batch::DeviceRow sampleRow(const QString &phone = QStringLiteral("9876543210"))
{
    batch::DeviceRow row;
    row.ip = QStringLiteral("192.168.1.10");
    row.port = 5555;
    row.phone = phone;
    return row;
}

batch::SmsMessage wechatSms(const QString &code, qint64 dateMs)
{
    batch::SmsMessage msg;
    msg.address = QStringLiteral("1069000000");
    msg.body = QStringLiteral("WeChat code: ") + code;
    msg.dateMs = dateMs;
    return msg;
}

bool logContainsAny(const QStringList &lines, const QString &needle)
{
    for (const QString &line : lines) {
        if (line.contains(needle)) {
            return true;
        }
    }
    return false;
}

} // namespace

class TestRegistrar : public QObject
{
    Q_OBJECT

private slots:
    void happyPathRegisters()
    {
        FakeDriver driver(flowPath("flow_happy"));
        driver.setSms({wechatSms(QStringLiteral("482913"), 1697881200000LL)});

        QStringList logLines;
        batch::RegisterOptions opts;
        const batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(),
                                        driver,
                                        opts,
                                        [&](const QString &line) { logLines.append(line); });

        QCOMPARE(outcome.state, batch::RegState::Registered);
        QVERIFY(driver.typedTexts().join(QLatin1Char('\n')).contains(QStringLiteral("9876543210")));
        const QString typed = driver.typedTexts().join(QLatin1Char('\n'));
        QVERIFY(typed.contains(QRegularExpression(QStringLiteral("[A-Za-z]"))));
        QVERIFY(typed.contains(QRegularExpression(QStringLiteral("[0-9]"))));
        for (const QString &text : driver.typedTexts()) {
            if (text.size() == 12) {
                QVERIFY(!logContainsAny(logLines, text));
            }
            if (text.size() == 6 && text == QStringLiteral("482913")) {
                QVERIFY(!logContainsAny(logLines, text));
            }
        }
    }

    void mainScreenAtStartIsAlreadyRegistered()
    {
        FakeDriver driver(flowPath("flow_main_start"));
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, batch::RegisterOptions(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::AlreadyRegistered);
        QCOMPARE(outcome.detail, QStringLiteral("device already logged in"));
    }

    void alreadyRegisteredText()
    {
        FakeDriver driver(flowPath("flow_already_registered"));
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, batch::RegisterOptions(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::AlreadyRegistered);
    }

    void captchaIsNeedsManual()
    {
        FakeDriver driver(flowPath("flow_captcha"));
        const int tapsBefore = driver.tapCount();
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, batch::RegisterOptions(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::NeedsManual);
        QCOMPARE(driver.tapCount(), tapsBefore);
    }

    void rateLimitedFails()
    {
        FakeDriver driver(flowPath("flow_rate_limit"));
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, batch::RegisterOptions(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::Failed);
        QCOMPARE(outcome.detail, QStringLiteral("rate limited"));
    }

    void smsTimeoutFails()
    {
        FakeDriver driver(flowPath("flow_sms_timeout"));
        batch::RegisterOptions opts;
        opts.smsTimeoutSec = 1;
        opts.maxSmsResend = 0;
        opts.stepTimeoutSec = 120;
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, opts, nullptr);
        QCOMPARE(outcome.state, batch::RegState::Failed);
        QCOMPARE(outcome.detail, QStringLiteral("sms timeout"));
    }

    void staleSmsNotUsed()
    {
        FakeDriver driver(flowPath("flow_stale_sms"));
        driver.setSms({wechatSms(QStringLiteral("111222"), 1697880000000LL)});
        driver.setNowMs(1697881200000LL);

        batch::RegisterOptions opts;
        opts.smsTimeoutSec = 1;
        opts.maxSmsResend = 0;
        opts.stepTimeoutSec = 120;

        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, opts, nullptr);
        QCOMPARE(outcome.state, batch::RegState::Failed);
        QVERIFY(!driver.typedTexts().contains(QStringLiteral("111222")));
    }

    void deadlineStopsLoop()
    {
        FakeDriver driver(flowPath("flow_deadline"));
        driver.setNowMs(0);
        batch::RegisterOptions opts;
        opts.deviceDeadlineSec = 1;
        opts.stepTimeoutSec = 60;
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, opts, nullptr);
        QCOMPARE(outcome.state, batch::RegState::Failed);
        QCOMPARE(outcome.detail, QStringLiteral("device deadline exceeded"));
    }

    void cancelReturnsCancelled()
    {
        FakeDriver driver(flowPath("flow_cancel"));
        driver.setCancelled(true);
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, batch::RegisterOptions(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::Cancelled);
    }

    void countryCodeSelectsRegion()
    {
        FakeDriver driver(flowPath("flow_country"));
        driver.setSms({wechatSms(QStringLiteral("482913"), 1697881200000LL)});

        batch::RegisterOutcome outcome = batch::WechatRegistrar::run(
            sampleRow(QStringLiteral("+919876543210")), driver, batch::RegisterOptions(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::Registered);
        QVERIFY(driver.typedTexts().contains(QStringLiteral("9876543210")));
        QVERIFY(!driver.typedTexts().join(QLatin1Char('\n')).contains(QStringLiteral("+919876543210")));
    }

    void generatedPasswordShape()
    {
        for (int i = 0; i < 20; ++i) {
            const QString password = batch::WechatRegistrar::generatePassword();
            QCOMPARE(password.size(), 12);
            QVERIFY(password.contains(QRegularExpression(QStringLiteral("[A-Za-z]"))));
            QVERIFY(password.contains(QRegularExpression(QStringLiteral("[0-9]"))));
        }
        const QString nick = batch::WechatRegistrar::generateNickname();
        QCOMPARE(nick.size(), 8);
        QVERIFY(nick.at(0).isLetter());
    }
};

QTEST_MAIN(TestRegistrar)
#include "tst_registrar.moc"
