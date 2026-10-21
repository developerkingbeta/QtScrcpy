#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
        , m_advanceEverySleeps(0)
        , m_sleepsSinceAdvance(0)
        , m_tapNeedsAdvance(false)
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

    int screenCount() const { return m_screens.size(); }

    QList<batch::UiNode> dump() override
    {
        // Apply a pending tap advance (button taps). Focus tap + typeText is
        // coalesced in typeText so country search does not skip the result row.
        if (m_tapNeedsAdvance) {
            m_tapNeedsAdvance = false;
            advance();
        }
        // Tiny fake-clock tick so step/sms/deadline budgets still expire if a
        // path forgets to sleepMs (never wall-sleep here).
        m_nowMs += 50;
        return batch::UiDump::parse(currentXml());
    }

    bool tap(int, int) override
    {
        ++m_tapCount;
        m_tapNeedsAdvance = true;
        return true;
    }

    bool typeText(const QString &asciiText) override
    {
        m_typedTexts.append(asciiText);
        // Focus tap + type = one screen change (not two).
        m_tapNeedsAdvance = false;
        advance();
        return true;
    }

    bool pressBack() override
    {
        // Production uses BACK to dismiss the IME after password. Fixtures have
        // no IME layer — advancing here skips the Agree/Submit screens and
        // lands on SMS before formSubmitted, causing step-timeout stalls.
        m_tapNeedsAdvance = false;
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

    qint64 steadyNowMs() override { return m_nowMs; }

    void sleepMs(int ms) override
    {
        // Instant: advance fake clock only — never QThread::msleep / wall wait.
        if (ms > 0) {
            m_nowMs += ms;
        }
        // Simulate a human completing an on-device puzzle while we poll.
        if (m_advanceEverySleeps > 0) {
            ++m_sleepsSinceAdvance;
            if (m_sleepsSinceAdvance >= m_advanceEverySleeps) {
                m_sleepsSinceAdvance = 0;
                advance();
            }
        }
    }

    bool cancelled() const override { return m_cancel.load(); }

    void setCancelled(bool value) { m_cancel.store(value); }

    void setSms(const QList<batch::SmsMessage> &sms) { m_sms = sms; }

    void setNotifications(const QString &dump) { m_notifications = dump; }

    int tapCount() const { return m_tapCount; }

    QStringList typedTexts() const { return m_typedTexts; }

    void setNowMs(qint64 ms) { m_nowMs = ms; }

    // Advance the fixture screen every N sleepMs calls (0 = disabled).
    void setAdvanceEverySleeps(int n)
    {
        m_advanceEverySleeps = n;
        m_sleepsSinceAdvance = 0;
    }

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
    int m_advanceEverySleeps;
    int m_sleepsSinceAdvance;
    bool m_tapNeedsAdvance;
};

QString flowPath(const char *name)
{
    const QString leaf = QString::fromUtf8(name);
    // 1) Next to this source file (works for ./tst_registrar and ctest).
    const QString besideSource =
        QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/fixtures/") + leaf;
    if (QDir(besideSource).exists()) {
        return besideSource;
    }
#ifdef QSC_TEST_FIXTURES_DIR
    const QString baked = QString::fromUtf8(QSC_TEST_FIXTURES_DIR) + QLatin1Char('/') + leaf;
    if (QDir(baked).exists()) {
        return baked;
    }
#endif
    const QString fromCwd = QDir::current().absoluteFilePath(leaf);
    if (QDir(fromCwd).exists()) {
        return fromCwd;
    }
    return QFINDTESTDATA(name);
}

// Short budgets: FakeDriver advances deviceNowMs instantly (no wall waits).
// stepTimeout must cover multi-dump SignUpForm fills (dump ticks +50ms each).
batch::RegisterOptions fastOpts()
{
    batch::RegisterOptions opts;
    opts.stepTimeoutSec = 15;
    opts.smsTimeoutSec = 3;
    opts.captchaTimeoutSec = 3;
    opts.deviceDeadlineSec = 30;
    return opts;
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
        QVERIFY2(driver.screenCount() > 0, qPrintable(flowPath("flow_happy")));
        driver.setSms({wechatSms(QStringLiteral("482913"), 1697881200000LL)});

        QStringList logLines;
        const batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(),
                                        driver,
                                        fastOpts(),
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
        QVERIFY2(driver.screenCount() > 0, qPrintable(flowPath("flow_main_start")));
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, fastOpts(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::AlreadyRegistered);
        QCOMPARE(outcome.detail, QStringLiteral("device already logged in"));
    }

    void alreadyRegisteredText()
    {
        FakeDriver driver(flowPath("flow_already_registered"));
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, fastOpts(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::AlreadyRegistered);
    }

    void captchaTimeoutNeedsManualAfterStart()
    {
        FakeDriver driver(flowPath("flow_captcha"));
        QVERIFY2(driver.screenCount() > 0, qPrintable(flowPath("flow_captcha")));
        batch::RegisterOptions opts = fastOpts();
        opts.captchaTimeoutSec = 2;

        QStringList logLines;
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(),
                                        driver,
                                        opts,
                                        [&](const QString &line) { logLines.append(line); });

        QCOMPARE(outcome.state, batch::RegState::NeedsManual);
        QVERIFY(outcome.detail.contains(QStringLiteral("security verification")));
        QVERIFY(outcome.detail.contains(QStringLiteral("timeout")));
        QVERIFY(driver.tapCount() >= 1);
        QVERIFY(logContainsAny(logLines, QStringLiteral("tap security start")));
        QVERIFY(logContainsAny(logLines, QStringLiteral("waiting security puzzle")));
    }

    void captchaPassResumesAfterHumanSolve()
    {
        FakeDriver driver(flowPath("flow_security_pass"));
        QVERIFY2(driver.screenCount() > 0, qPrintable(flowPath("flow_security_pass")));
        driver.setSms({wechatSms(QStringLiteral("482913"), 1697881200000LL)});
        // After Start → puzzle, one sleep advances to SMS (human solved).
        driver.setAdvanceEverySleeps(1);

        QStringList logLines;
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(),
                                        driver,
                                        fastOpts(),
                                        [&](const QString &line) { logLines.append(line); });

        QCOMPARE(outcome.state, batch::RegState::Registered);
        QVERIFY(logContainsAny(logLines, QStringLiteral("tap security start")));
        QVERIFY(logContainsAny(logLines, QStringLiteral("waiting security puzzle")));
        QVERIFY(driver.typedTexts().contains(QStringLiteral("482913")));
    }

    void captchaVerificationFailedExits()
    {
        FakeDriver driver(flowPath("flow_security_fail"));
        QVERIFY2(driver.screenCount() > 0, qPrintable(flowPath("flow_security_fail")));
        QStringList logLines;
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(),
                                        driver,
                                        fastOpts(),
                                        [&](const QString &line) { logLines.append(line); });
        QCOMPARE(outcome.state, batch::RegState::Failed);
        QCOMPARE(outcome.detail, QStringLiteral("security verification failed"));
        QVERIFY(logContainsAny(logLines, QStringLiteral("security verification failed")));
    }

    void rateLimitedFails()
    {
        FakeDriver driver(flowPath("flow_rate_limit"));
        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, fastOpts(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::Failed);
        QCOMPARE(outcome.detail, QStringLiteral("rate limited"));
    }

    void smsTimeoutFails()
    {
        FakeDriver driver(flowPath("flow_sms_timeout"));
        QVERIFY2(driver.screenCount() > 0, qPrintable(flowPath("flow_sms_timeout")));
        batch::RegisterOptions opts = fastOpts();
        opts.smsTimeoutSec = 1;
        opts.maxSmsResend = 0;
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

        batch::RegisterOptions opts = fastOpts();
        opts.smsTimeoutSec = 1;
        opts.maxSmsResend = 0;

        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, opts, nullptr);
        QCOMPARE(outcome.state, batch::RegState::Failed);
        QVERIFY(!driver.typedTexts().contains(QStringLiteral("111222")));
    }

    void deadlineStopsLoop()
    {
        FakeDriver driver(flowPath("flow_deadline"));
        driver.setNowMs(0);
        batch::RegisterOptions opts = fastOpts();
        opts.deviceDeadlineSec = 1;
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
            batch::WechatRegistrar::run(sampleRow(), driver, fastOpts(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::Cancelled);
    }

    void countryCodeSelectsRegion()
    {
        FakeDriver driver(flowPath("flow_country"));
        driver.setSms({wechatSms(QStringLiteral("482913"), 1697881200000LL)});

        batch::RegisterOutcome outcome = batch::WechatRegistrar::run(
            sampleRow(QStringLiteral("+919876543210")), driver, fastOpts(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::Registered);
        QVERIFY(driver.typedTexts().contains(QStringLiteral("9876543210")));
        QVERIFY(!driver.typedTexts().join(QLatin1Char('\n')).contains(QStringLiteral("+919876543210")));
    }

    void countryMismatchOpensPickerForIndia()
    {
        FakeDriver driver(flowPath("flow_country_us91"));
        driver.setSms({wechatSms(QStringLiteral("482913"), 1697881200000LL)});

        batch::RegisterOutcome outcome = batch::WechatRegistrar::run(
            sampleRow(QStringLiteral("+919876543210")), driver, fastOpts(), nullptr);
        QCOMPARE(outcome.state, batch::RegState::Registered);
        QVERIFY(driver.typedTexts().contains(QStringLiteral("91")));
        QVERIFY(driver.typedTexts().contains(QStringLiteral("9876543210")));
        QVERIFY(!driver.typedTexts().join(QLatin1Char('\n')).contains(QStringLiteral("+919876543210")));
    }

    void staleNotificationCodeNotTyped()
    {
        FakeDriver driver(flowPath("flow_stale_sms"));
        driver.setSms({wechatSms(QStringLiteral("111222"), 1697880000000LL)});
        driver.setNowMs(1697881200000LL);
        driver.setNotifications(QStringLiteral(
            "NotificationRecord(0xabc123: pkg=com.tencent.mm)\n"
            "    android.title=WeChat\n"
            "    android.text=Your Weixin code is 654321\n"));

        batch::RegisterOptions opts = fastOpts();
        opts.smsTimeoutSec = 1;
        opts.maxSmsResend = 0;

        batch::RegisterOutcome outcome =
            batch::WechatRegistrar::run(sampleRow(), driver, opts, nullptr);
        QCOMPARE(outcome.state, batch::RegState::Failed);
        QVERIFY(!driver.typedTexts().contains(QStringLiteral("654321")));
        QVERIFY(!driver.typedTexts().contains(QStringLiteral("111222")));
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
