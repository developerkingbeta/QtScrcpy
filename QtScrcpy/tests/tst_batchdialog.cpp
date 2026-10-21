#include <QtTest>
#include <QPushButton>
#include <QTemporaryDir>
#include <atomic>

#include "../ui/batchdialog.h"
#include "batch/batchcontroller.h"
#include "batch/devicerow.h"

class TestBatchDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        batch::BatchController::clearRegistrarFactory();
    }

    void cleanup()
    {
        batch::BatchController::clearRegistrarFactory();
    }

    void registerDisabledUntilEligibleRowChecked()
    {
        QTemporaryDir dataDir;
        QVERIFY(dataDir.isValid());

        BatchDialog dialog(nullptr, dataDir.path());
        batch::DeviceRow eligible;
        eligible.ip = QStringLiteral("100.64.0.1");
        eligible.phone = QStringLiteral("15500000001");
        eligible.conn = batch::ConnState::Connected;
        eligible.wechat = batch::WechatState::Installed;
        eligible.reg = batch::RegState::Idle;

        batch::DeviceRow notInstalled = eligible;
        notInstalled.ip = QStringLiteral("100.64.0.2");
        notInstalled.wechat = batch::WechatState::Missing;

        dialog.setTestRows({eligible, notInstalled});
        dialog.modelForTest()->setAllChecked(false);
        dialog.refreshActionStatesForTest();

        auto *registerBtn = dialog.findChild<QPushButton *>(QStringLiteral("batchRegisterBtn"));
        QVERIFY(registerBtn);
        QVERIFY(!registerBtn->isEnabled());

        dialog.modelForTest()->setData(dialog.modelForTest()->index(0, batch::BatchModel::ColSelect),
                                       Qt::Checked,
                                       Qt::CheckStateRole);
        dialog.refreshActionStatesForTest();
        QVERIFY(registerBtn->isEnabled());

        dialog.modelForTest()->setData(dialog.modelForTest()->index(0, batch::BatchModel::ColSelect),
                                       Qt::Unchecked,
                                       Qt::CheckStateRole);
        dialog.modelForTest()->setData(dialog.modelForTest()->index(1, batch::BatchModel::ColSelect),
                                       Qt::Checked,
                                       Qt::CheckStateRole);
        dialog.refreshActionStatesForTest();
        QVERIFY(!registerBtn->isEnabled());
    }

    void installDisabledWithoutConnectedDevices()
    {
        QTemporaryDir dataDir;
        QVERIFY(dataDir.isValid());

        BatchDialog dialog(nullptr, dataDir.path());
        batch::DeviceRow offline;
        offline.ip = QStringLiteral("100.64.0.3");
        offline.phone = QStringLiteral("15500000003");
        offline.conn = batch::ConnState::Offline;

        dialog.setTestRows({offline});
        dialog.refreshActionStatesForTest();

        auto *installBtn = dialog.findChild<QPushButton *>(QStringLiteral("batchInstallBtn"));
        QVERIFY(installBtn);
        QVERIFY(!installBtn->isEnabled());
        QCOMPARE(installBtn->toolTip(),
                 QStringLiteral("Upload a sheet and wait for at least one connected device"));
    }

    void cancelHiddenWhenIdle()
    {
        QTemporaryDir dataDir;
        QVERIFY(dataDir.isValid());

        BatchDialog dialog(nullptr, dataDir.path());
        auto *cancelBtn = dialog.findChild<QPushButton *>(QStringLiteral("batchCancelBtn"));
        QVERIFY(cancelBtn);
        QVERIFY(!cancelBtn->isVisible());
    }

    void confirmationDeclinedStartsNothing()
    {
        QTemporaryDir dataDir;
        QVERIFY(dataDir.isValid());

        bool registerStarted = false;
        batch::BatchController::setRegistrarFactory([&registerStarted](const batch::DeviceRow &, const std::atomic<bool> *) {
            registerStarted = true;
            batch::RegisterOutcome outcome;
            outcome.state = batch::RegState::Registered;
            return outcome;
        });

        BatchDialog dialog(nullptr, dataDir.path());
        dialog.setConfirmHook([](int) { return false; });

        batch::DeviceRow row;
        row.ip = QStringLiteral("100.64.0.4");
        row.phone = QStringLiteral("15500000004");
        row.conn = batch::ConnState::Connected;
        row.wechat = batch::WechatState::Installed;
        dialog.setTestRows({row});
        dialog.refreshActionStatesForTest();

        auto *registerBtn = dialog.findChild<QPushButton *>(QStringLiteral("batchRegisterBtn"));
        QVERIFY(registerBtn->isEnabled());
        QTest::mouseClick(registerBtn, Qt::LeftButton);

        QVERIFY(!registerStarted);
        QVERIFY(!dialog.controllerForTest()->isBusy());
    }
};

QTEST_MAIN(TestBatchDialog)
#include "tst_batchdialog.moc"
