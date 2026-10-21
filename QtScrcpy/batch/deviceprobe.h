#ifndef BATCH_DEVICEPROBE_H
#define BATCH_DEVICEPROBE_H

#include <atomic>

#include <QString>

#include "devicerow.h"

namespace batch {

class DeviceProbe
{
public:
    static ConnState probeConnection(DeviceRow &row, const std::atomic<bool> *cancel = nullptr);
    static WechatState probeWechat(DeviceRow &row, const std::atomic<bool> *cancel = nullptr);
    static bool installWechat(DeviceRow &row,
                              const QString &apkPath,
                              QString *error = nullptr,
                              const std::atomic<bool> *cancel = nullptr);

private:
    DeviceProbe();
};

} // namespace batch

#endif // BATCH_DEVICEPROBE_H
