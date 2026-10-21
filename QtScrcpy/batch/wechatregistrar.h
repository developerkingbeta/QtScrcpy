#ifndef BATCH_WECHATREGISTRAR_H
#define BATCH_WECHATREGISTRAR_H

#include <functional>

#include "devicerow.h"
#include "smsreader.h"
#include "uidump.h"

namespace batch {

struct RegisterOptions
{
    int stepTimeoutSec = 20;
    int smsTimeoutSec = 120;
    // Wait for a human to finish the on-device slider/puzzle after Start.
    int captchaTimeoutSec = 150;
    int deviceDeadlineSec = 600;
    int maxSmsResend = 1;
};

struct RegisterOutcome
{
    RegState state = RegState::Failed;
    QString detail;
};

class IUiDriver
{
public:
    virtual ~IUiDriver();

    virtual QList<UiNode> dump() = 0;
    virtual bool tap(int x, int y) = 0;
    virtual bool typeText(const QString &asciiText) = 0;
    virtual bool pressBack() = 0;
    virtual bool launchWechat() = 0;
    virtual QList<SmsMessage> readSms() = 0;
    virtual QString notificationDump() = 0;
    virtual qint64 deviceNowMs() = 0;
    virtual void sleepMs(int ms) = 0;
    virtual bool cancelled() const = 0;
};

class WechatRegistrar
{
public:
    static RegisterOutcome run(const DeviceRow &row,
                               IUiDriver &driver,
                               const RegisterOptions &opts,
                               const std::function<void(const QString &)> &log);

    static QString generatePassword();
    static QString generateNickname();
};

} // namespace batch

#endif // BATCH_WECHATREGISTRAR_H
