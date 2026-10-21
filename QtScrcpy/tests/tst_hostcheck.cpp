#include <QtTest>

#include "hostcheck.h"

class TestHostCheck : public QObject
{
    Q_OBJECT

private slots:
    void publicRoutableIPv4()
    {
        QCOMPARE(batch::HostCheck::isPublicRoutableIPv4(QStringLiteral("100.101.102.103")), false);
        QCOMPARE(batch::HostCheck::isPublicRoutableIPv4(QStringLiteral("192.168.1.5")), false);
        QCOMPARE(batch::HostCheck::isPublicRoutableIPv4(QStringLiteral("8.8.8.8")), true);
    }

    void ipv4Literal()
    {
        QCOMPARE(batch::HostCheck::isIPv4Literal(QStringLiteral("100.1.2")), false);
        QCOMPARE(batch::HostCheck::isIPv4Literal(QStringLiteral("tailscale-host")), false);
    }
};

QTEST_MAIN(TestHostCheck)
#include "tst_hostcheck.moc"
