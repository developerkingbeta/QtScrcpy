#include "wechatregistrar.h"

#include <QElapsedTimer>
#include <QRegularExpression>

#include <random>

namespace batch {

namespace {

enum class ScreenKind {
    Unknown,
    MainTabs,
    PrivacyDialog,
    PermissionDialog,
    WelcomeRegister,
    SignUpForm,
    CountryPicker,
    SmsVerify,
    SmsResend,
    AgreementTerms,
    AlreadyRegisteredMsg,
    CaptchaSecurity,
    RateLimited
};

struct TextVariantRow
{
    ScreenKind kind;
    const char *const *variants;
    int variantCount;
};

// Single calibration table (EN + ZH) for Task 10.
static const char *const kMainTabsVariants[] = {
    "Chats", "微信", "Contacts", "通讯录", "Discover", "发现", "Me", "我"};
static const char *const kPrivacyVariants[] = {"Agree", "同意"};
static const char *const kPermissionVariants[] = {"Allow", "允许", "While using the app"};
static const char *const kWelcomeVariants[] = {
    "Sign up via Mobile", "Sign Up", "Register", "注册"};
static const char *const kSignUpVariants[] = {"Accept and Continue", "Next", "下一步"};
static const char *const kCountryPickerVariants[] = {"Search", "搜索"};
static const char *const kSmsVerifyVariants[] = {"Verification code", "验证码"};
static const char *const kSmsResendVariants[] = {"Resend", "重新发送"};
static const char *const kAgreementVariants[] = {
    "I have read and accept", "I agree", "同意协议", "Terms of Service"};
static const char *const kAlreadyRegisteredVariants[] = {
    "already registered", "已注册", "already linked"};
static const char *const kCaptchaVariants[] = {
    "Security Verification", "安全验证", "drag", "拖动", "puzzle", "拼图"};
static const char *const kRateLimitedVariants[] = {
    "too frequent", "过于频繁", "try again later", "稍后再试"};

const TextVariantRow kTextVariants[] = {
    {ScreenKind::MainTabs, kMainTabsVariants, 8},
    {ScreenKind::PrivacyDialog, kPrivacyVariants, 2},
    {ScreenKind::PermissionDialog, kPermissionVariants, 3},
    {ScreenKind::WelcomeRegister, kWelcomeVariants, 4},
    {ScreenKind::SignUpForm, kSignUpVariants, 3},
    {ScreenKind::CountryPicker, kCountryPickerVariants, 2},
    {ScreenKind::SmsVerify, kSmsVerifyVariants, 2},
    {ScreenKind::SmsResend, kSmsResendVariants, 2},
    {ScreenKind::AgreementTerms, kAgreementVariants, 4},
    {ScreenKind::AlreadyRegisteredMsg, kAlreadyRegisteredVariants, 3},
    {ScreenKind::CaptchaSecurity, kCaptchaVariants, 6},
    {ScreenKind::RateLimited, kRateLimitedVariants, 4},
};

bool nodeMatchesVariants(const UiNode &node, const TextVariantRow &row)
{
    for (int i = 0; i < row.variantCount; ++i) {
        const QString needle = QString::fromUtf8(row.variants[i]);
        if (node.text.contains(needle, Qt::CaseInsensitive)
            || node.desc.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool anyNodeMatches(const QList<UiNode> &nodes, ScreenKind kind)
{
    for (const TextVariantRow &row : kTextVariants) {
        if (row.kind != kind) {
            continue;
        }
        for (const UiNode &node : nodes) {
            if (nodeMatchesVariants(node, row)) {
                return true;
            }
        }
    }
    return false;
}

bool hasMainTabs(const QList<UiNode> &nodes)
{
    for (const TextVariantRow &row : kTextVariants) {
        if (row.kind != ScreenKind::MainTabs) {
            continue;
        }
        for (int i = 0; i + 1 < row.variantCount; i += 2) {
            const QString en = QString::fromUtf8(row.variants[i]);
            const QString zh = QString::fromUtf8(row.variants[i + 1]);
            bool found = false;
            for (const UiNode &node : nodes) {
                const QString t = node.text + node.desc;
                if (t.contains(en, Qt::CaseInsensitive) || t.contains(zh, Qt::CaseInsensitive)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
        return true;
    }
    return false;
}

ScreenKind classify(const QList<UiNode> &nodes)
{
    if (anyNodeMatches(nodes, ScreenKind::CaptchaSecurity)) {
        return ScreenKind::CaptchaSecurity;
    }
    if (anyNodeMatches(nodes, ScreenKind::RateLimited)) {
        return ScreenKind::RateLimited;
    }
    if (anyNodeMatches(nodes, ScreenKind::AlreadyRegisteredMsg)) {
        return ScreenKind::AlreadyRegisteredMsg;
    }
    if (hasMainTabs(nodes)) {
        return ScreenKind::MainTabs;
    }
    if (anyNodeMatches(nodes, ScreenKind::SmsVerify)
        || anyNodeMatches(nodes, ScreenKind::SmsResend)) {
        return ScreenKind::SmsVerify;
    }
    if (anyNodeMatches(nodes, ScreenKind::CountryPicker)) {
        return ScreenKind::CountryPicker;
    }
    const QList<UiNode> edits = UiDump::editTexts(nodes);
    if (edits.size() >= 2 && anyNodeMatches(nodes, ScreenKind::SignUpForm)) {
        return ScreenKind::SignUpForm;
    }
    if (anyNodeMatches(nodes, ScreenKind::WelcomeRegister)) {
        return ScreenKind::WelcomeRegister;
    }
    if (anyNodeMatches(nodes, ScreenKind::PermissionDialog)) {
        return ScreenKind::PermissionDialog;
    }
    if (anyNodeMatches(nodes, ScreenKind::PrivacyDialog)) {
        return ScreenKind::PrivacyDialog;
    }
    return ScreenKind::Unknown;
}

QStringList variantListFor(ScreenKind kind)
{
    QStringList list;
    for (const TextVariantRow &row : kTextVariants) {
        if (row.kind == kind) {
            for (int i = 0; i < row.variantCount; ++i) {
                list << QString::fromUtf8(row.variants[i]);
            }
            break;
        }
    }
    return list;
}

const UiNode *findTapTarget(const QList<UiNode> &nodes, ScreenKind kind)
{
    const QStringList needles = variantListFor(kind);
    return UiDump::findAny(nodes, needles, false);
}

QString extractDialCode(const QString &phone)
{
    if (!phone.startsWith(QLatin1Char('+'))) {
        return QString();
    }
    const QString rest = phone.mid(1);
    // Longest-first ITU calling codes so "+9198..." resolves to "+91", not "+919".
    static const char *const kCallingCodes[] = {
        "998", "997", "996", "995", "994", "993", "992", "977", "976", "975", "974", "973",
        "972", "971", "970", "968", "967", "966", "965", "964", "963", "962", "961", "960",
        "886", "880", "856", "855", "853", "852", "850", "692", "691", "690", "689", "688",
        "687", "686", "685", "683", "682", "681", "680", "679", "678", "677", "676", "675",
        "674", "673", "672", "670", "599", "598", "597", "595", "593", "592", "591", "590",
        "509", "508", "507", "506", "505", "504", "503", "502", "501", "500", "423", "421",
        "420", "389", "387", "386", "385", "383", "382", "381", "380", "378", "377", "376",
        "375", "374", "373", "372", "371", "370", "359", "358", "357", "356", "355", "354",
        "353", "352", "351", "350", "299", "298", "297", "291", "290", "269", "268", "267",
        "266", "265", "264", "263", "262", "261", "260", "258", "257", "256", "255", "254",
        "253", "252", "251", "250", "249", "248", "246", "245", "244", "243", "242", "241",
        "240", "239", "238", "237", "236", "235", "234", "233", "232", "231", "230", "229",
        "228", "227", "226", "225", "224", "223", "222", "221", "220", "218", "216", "213",
        "212", "211", "98",  "95",  "94",  "93",  "92",  "91",  "90",  "86",  "84",  "82",
        "81",  "66",  "65",  "64",  "63",  "62",  "61",  "60",  "58",  "57",  "56",  "55",
        "54",  "53",  "52",  "51",  "49",  "48",  "47",  "46",  "45",  "44",  "43",  "41",
        "40",  "39",  "36",  "34",  "33",  "32",  "31",  "30",  "27",  "20",  "7",   "1"};
    for (size_t i = 0; i < sizeof(kCallingCodes) / sizeof(kCallingCodes[0]); ++i) {
        const QString code = QString::fromLatin1(kCallingCodes[i]);
        if (rest.startsWith(code)) {
            return QLatin1Char('+') + code;
        }
    }
    static const QRegularExpression re(QStringLiteral(R"(^\+(\d{1,3}))"));
    const QRegularExpressionMatch match = re.match(phone);
    if (!match.hasMatch()) {
        return QString();
    }
    return QLatin1Char('+') + match.captured(1);
}

QString plusDialInText(const QString &text)
{
    static const QRegularExpression re(QStringLiteral(R"([+＋](\d{1,3}))"));
    const QRegularExpressionMatch match = re.match(text);
    if (!match.hasMatch()) {
        return QString();
    }
    return QLatin1Char('+') + match.captured(1);
}

// Any dial code shown on the signup form (e.g. "United States（+1）").
QString formShownDialCode(const QList<UiNode> &nodes)
{
    QString best;
    for (const UiNode &node : nodes) {
        const QString candidates[] = {node.text, node.desc};
        for (size_t i = 0; i < 2; ++i) {
            const QString dial = plusDialInText(candidates[i]);
            if (dial.isEmpty()) {
                continue;
            }
            if (best.isEmpty() || dial.size() > best.size()) {
                best = dial;
            }
        }
    }
    return best;
}

const UiNode *findCountryRegionOpener(const QList<UiNode> &nodes, const QString &shownDial)
{
    if (shownDial.isEmpty()) {
        return nullptr;
    }
    const UiNode *clickableWithDial = nullptr;
    for (const UiNode &node : nodes) {
        const QString hay = node.text + node.desc;
        if (!hay.contains(shownDial)) {
            continue;
        }
        if (node.clickable) {
            return &node;
        }
        if (!clickableWithDial) {
            clickableWithDial = &node;
        }
    }
    return clickableWithDial;
}

// Prefer a visible "+NN" row that is a prefix of phone (form / picker calibration).
QString dialCodeFromNodes(const QString &phone, const QList<UiNode> &nodes)
{
    if (!phone.startsWith(QLatin1Char('+'))) {
        return QString();
    }
    static const QRegularExpression codeRe(QStringLiteral(R"(^\+\d{1,3}$)"));
    QString best;
    for (const UiNode &node : nodes) {
        const QString candidates[] = {node.text.trimmed(), node.desc.trimmed()};
        for (size_t i = 0; i < 2; ++i) {
            const QString &cand = candidates[i];
            if (!codeRe.match(cand).hasMatch()) {
                continue;
            }
            if (phone.startsWith(cand) && cand.size() > best.size()) {
                best = cand;
            }
        }
    }
    return best;
}

QString nationalNumber(const QString &phone)
{
    const QString dial = extractDialCode(phone);
    if (dial.isEmpty()) {
        return phone;
    }
    return phone.mid(dial.size());
}

} // namespace

IUiDriver::~IUiDriver() {}

QString WechatRegistrar::generatePassword()
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char digits[] = "0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> letterDist(0, static_cast<int>(sizeof(letters) - 2));
    std::uniform_int_distribution<int> digitDist(0, static_cast<int>(sizeof(digits) - 2));
    std::uniform_int_distribution<int> pickDist(0, 1);

    QString out;
    out.reserve(12);
    out.append(QLatin1Char(letters[letterDist(gen)]));
    out.append(QLatin1Char(digits[digitDist(gen)]));
    for (int i = 2; i < 12; ++i) {
        if (pickDist(gen) == 0) {
            out.append(QLatin1Char(letters[letterDist(gen)]));
        } else {
            out.append(QLatin1Char(digits[digitDist(gen)]));
        }
    }
    return out;
}

QString WechatRegistrar::generateNickname()
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char alnum[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> letterDist(0, static_cast<int>(sizeof(letters) - 2));
    std::uniform_int_distribution<int> alnumDist(0, static_cast<int>(sizeof(alnum) - 2));

    QString out;
    out.reserve(8);
    out.append(QLatin1Char(letters[letterDist(gen)]));
    for (int i = 1; i < 8; ++i) {
        out.append(QLatin1Char(alnum[alnumDist(gen)]));
    }
    return out;
}

RegisterOutcome WechatRegistrar::run(const DeviceRow &row,
                                     IUiDriver &driver,
                                     const RegisterOptions &opts,
                                     const std::function<void(const QString &)> &log)
{
    RegisterOutcome outcome;
    QElapsedTimer runTimer;
    runTimer.start();

    const QString password = generatePassword();
    const QString nickname = generateNickname();
    QString dialCode = extractDialCode(row.phone);
    QString phoneEntry = nationalNumber(row.phone);

    bool formSubmitted = false;
    bool countrySelected = dialCode.isEmpty();
    int formFieldsTyped = 0;
    bool agreementTicked = false;
    qint64 notBeforeMs = 0;
    QElapsedTimer smsWaitTimer;
    bool smsWaitStarted = false;
    int smsResendCount = 0;
    bool smsCodeTyped = false;
    bool countrySearchTyped = false;

    ScreenKind lastScreen = ScreenKind::Unknown;
    QElapsedTimer stepTimer;
    stepTimer.start();

    auto logStep = [&](const QString &step) {
        if (log) {
            log(step);
        }
    };

    auto checkDeadline = [&]() -> bool {
        if (runTimer.elapsed() > static_cast<qint64>(opts.deviceDeadlineSec) * 1000) {
            outcome.state = RegState::Failed;
            outcome.detail = QStringLiteral("device deadline exceeded");
            return true;
        }
        return false;
    };

    auto checkCancelled = [&]() -> bool {
        if (driver.cancelled()) {
            outcome.state = RegState::Cancelled;
            outcome.detail = QStringLiteral("cancelled");
            return true;
        }
        return false;
    };

    auto noteProgress = [&](ScreenKind screen) -> bool {
        if (screen != lastScreen) {
            lastScreen = screen;
            stepTimer.restart();
            return false;
        }
        if (stepTimer.elapsed() > static_cast<qint64>(opts.stepTimeoutSec) * 1000) {
            outcome.state = RegState::Failed;
            outcome.detail = QStringLiteral("step timeout");
            return true;
        }
        return false;
    };

    logStep(QStringLiteral("launch"));
    driver.launchWechat();

    while (outcome.detail.isEmpty()) {
        if (checkCancelled() || checkDeadline()) {
            break;
        }

        const QList<UiNode> nodes = driver.dump();
        const ScreenKind screen = classify(nodes);
        if (noteProgress(screen)) {
            break;
        }

        if (screen == ScreenKind::CaptchaSecurity) {
            outcome.state = RegState::NeedsManual;
            outcome.detail = QStringLiteral("security verification");
            break;
        }
        if (screen == ScreenKind::RateLimited) {
            outcome.state = RegState::Failed;
            outcome.detail = QStringLiteral("rate limited");
            break;
        }
        if (screen == ScreenKind::AlreadyRegisteredMsg) {
            outcome.state = RegState::AlreadyRegistered;
            outcome.detail = QStringLiteral("already registered on screen");
            break;
        }
        if (screen == ScreenKind::MainTabs) {
            if (formSubmitted) {
                outcome.state = RegState::Registered;
                outcome.detail = QStringLiteral("registration complete");
                logStep(QStringLiteral("complete"));
            } else {
                outcome.state = RegState::AlreadyRegistered;
                outcome.detail = QStringLiteral("device already logged in");
            }
            break;
        }

        switch (screen) {
        case ScreenKind::PrivacyDialog: {
            logStep(QStringLiteral("dismiss privacy"));
            const UiNode *target = findTapTarget(nodes, ScreenKind::PrivacyDialog);
            if (target) {
                driver.tap(target->center().x(), target->center().y());
            }
            break;
        }
        case ScreenKind::PermissionDialog: {
            logStep(QStringLiteral("dismiss permission"));
            const UiNode *target = findTapTarget(nodes, ScreenKind::PermissionDialog);
            if (target) {
                driver.tap(target->center().x(), target->center().y());
            }
            break;
        }
        case ScreenKind::WelcomeRegister: {
            logStep(QStringLiteral("tap register"));
            const UiNode *target = findTapTarget(nodes, ScreenKind::WelcomeRegister);
            if (target) {
                driver.tap(target->center().x(), target->center().y());
            }
            break;
        }
        case ScreenKind::CountryPicker: {
            logStep(QStringLiteral("pick country"));
            const UiNode *target = UiDump::findAny(nodes, {dialCode}, false);
            if (target) {
                driver.tap(target->center().x(), target->center().y());
                countrySelected = true;
                break;
            }
            if (!countrySearchTyped && !dialCode.isEmpty()) {
                const QList<UiNode> edits = UiDump::editTexts(nodes);
                if (!edits.isEmpty()) {
                    logStep(QStringLiteral("search country"));
                    driver.tap(edits.first().center().x(), edits.first().center().y());
                    driver.typeText(dialCode.mid(1));
                    countrySearchTyped = true;
                }
            }
            break;
        }
        case ScreenKind::SignUpForm: {
            if (!countrySelected && !dialCode.isEmpty()) {
                const QString shownDial = formShownDialCode(nodes);
                const QString prefixDial = dialCodeFromNodes(row.phone, nodes);
                if (!shownDial.isEmpty() && shownDial == dialCode) {
                    countrySelected = true;
                } else if (!shownDial.isEmpty() && shownDial != dialCode) {
                    logStep(QStringLiteral("open country picker"));
                    const UiNode *dialRow = findCountryRegionOpener(nodes, shownDial);
                    if (dialRow) {
                        driver.tap(dialRow->center().x(), dialRow->center().y());
                    }
                    break;
                } else if (!prefixDial.isEmpty()) {
                    countrySelected = true;
                } else {
                    logStep(QStringLiteral("open country picker"));
                    const UiNode *dialRow = UiDump::findAny(nodes, {dialCode}, false);
                    if (dialRow) {
                        driver.tap(dialRow->center().x(), dialRow->center().y());
                    }
                    break;
                }
            }
            if (formFieldsTyped == 0) {
                logStep(QStringLiteral("fill nickname"));
                const QList<UiNode> edits = UiDump::editTexts(nodes);
                if (!edits.isEmpty()) {
                    driver.tap(edits.first().center().x(), edits.first().center().y());
                    driver.typeText(nickname);
                    ++formFieldsTyped;
                }
                break;
            }
            if (formFieldsTyped == 1) {
                logStep(QStringLiteral("fill phone"));
                const QList<UiNode> edits = UiDump::editTexts(nodes);
                if (edits.size() > 1) {
                    driver.tap(edits.at(1).center().x(), edits.at(1).center().y());
                    driver.typeText(phoneEntry);
                    ++formFieldsTyped;
                }
                break;
            }
            if (formFieldsTyped == 2) {
                logStep(QStringLiteral("fill password"));
                const QList<UiNode> edits = UiDump::editTexts(nodes);
                if (edits.size() > 2) {
                    driver.tap(edits.at(2).center().x(), edits.at(2).center().y());
                    driver.typeText(password);
                    ++formFieldsTyped;
                }
                break;
            }
            if (!agreementTicked) {
                logStep(QStringLiteral("accept terms"));
                const UiNode *agreeNode = findTapTarget(nodes, ScreenKind::AgreementTerms);
                if (agreeNode) {
                    driver.tap(agreeNode->center().x(), agreeNode->center().y());
                    agreementTicked = true;
                }
                break;
            }
            logStep(QStringLiteral("submit signup"));
            const UiNode *next = findTapTarget(nodes, ScreenKind::SignUpForm);
            if (next) {
                const qint64 atNextMs = driver.deviceNowMs();
                notBeforeMs = atNextMs - 30000;
                smsWaitTimer.restart();
                smsWaitStarted = true;
                driver.tap(next->center().x(), next->center().y());
                formSubmitted = true;
            }
            break;
        }
        case ScreenKind::SmsVerify: {
            if (!formSubmitted) {
                break;
            }
            if (!smsWaitStarted) {
                smsWaitTimer.restart();
                smsWaitStarted = true;
            }
            if (!smsCodeTyped) {
                QString code = SmsReader::extractWechatCode(driver.readSms(), notBeforeMs);
                if (code.isEmpty() && smsResendCount >= 1) {
                    code = SmsReader::extractCodeFromNotifications(driver.notificationDump());
                }
                if (code.isEmpty()) {
                    if (smsWaitTimer.elapsed() > static_cast<qint64>(opts.smsTimeoutSec) * 1000) {
                        if (smsResendCount < opts.maxSmsResend) {
                            logStep(QStringLiteral("resend sms"));
                            const UiNode *resend = findTapTarget(nodes, ScreenKind::SmsResend);
                            if (resend) {
                                driver.tap(resend->center().x(), resend->center().y());
                            }
                            ++smsResendCount;
                            smsWaitTimer.restart();
                        } else {
                            outcome.state = RegState::Failed;
                            outcome.detail = QStringLiteral("sms timeout");
                        }
                    } else {
                        driver.sleepMs(200);
                    }
                    break;
                }
                logStep(QStringLiteral("enter sms code"));
                const QList<UiNode> edits = UiDump::editTexts(nodes);
                if (!edits.isEmpty()) {
                    driver.tap(edits.first().center().x(), edits.first().center().y());
                }
                driver.typeText(code);
                smsCodeTyped = true;
                break;
            }
            logStep(QStringLiteral("confirm sms"));
            const UiNode *next = UiDump::findAny(nodes, variantListFor(ScreenKind::SignUpForm), false);
            if (next) {
                driver.tap(next->center().x(), next->center().y());
            }
            break;
        }
        default:
            driver.sleepMs(200);
            break;
        }

        if (checkCancelled() || checkDeadline()) {
            break;
        }
    }

    if (outcome.detail.isEmpty()) {
        outcome.state = RegState::Failed;
        outcome.detail = QStringLiteral("registration stalled");
    }

    return outcome;
}

} // namespace batch
