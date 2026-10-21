#ifndef BATCH_ADBUIDRIVER_H
#define BATCH_ADBUIDRIVER_H

#include <atomic>

#include "wechatregistrar.h"

namespace batch {

class AdbUiDriver : public IUiDriver
{
public:
    AdbUiDriver(const QString &serial, const std::atomic<bool> *cancel);

    QList<UiNode> dump() override;
    bool tap(int x, int y) override;
    bool typeText(const QString &asciiText) override;
    bool pressBack() override;
    bool launchWechat() override;
    QList<SmsMessage> readSms() override;
    QString notificationDump() override;
    qint64 deviceNowMs() override;
    qint64 steadyNowMs() override;
    void sleepMs(int ms) override;
    bool cancelled() const override;

private:
    QString m_serial;
    const std::atomic<bool> *m_cancel;
};

} // namespace batch

#endif // BATCH_ADBUIDRIVER_H
