#ifndef BATCH_HOSTCHECK_H
#define BATCH_HOSTCHECK_H

#include <QString>

namespace batch {

class HostCheck
{
public:
    static bool isIPv4Literal(const QString &host);
    static bool isPublicRoutableIPv4(const QString &host);

private:
    HostCheck();
};

} // namespace batch

#endif // BATCH_HOSTCHECK_H
