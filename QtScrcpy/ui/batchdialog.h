#ifndef BATCHDIALOG_H
#define BATCHDIALOG_H

#include <QDialog>
#include <functional>

namespace batch {
class BatchController;
class BatchModel;
struct DeviceRow;
} // namespace batch

class QPushButton;
class QProgressBar;
class QPlainTextEdit;
class QLabel;
class QSpinBox;
class QTableView;

class BatchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchDialog(QWidget *parent = nullptr, const QString &dataDir = QString());

    using ConfirmHook = std::function<bool(int count)>;
    void setConfirmHook(const ConfirmHook &hook);
    void setTestRows(const QList<batch::DeviceRow> &rows);

    batch::BatchController *controllerForTest() const { return m_controller; }
    batch::BatchModel *modelForTest() const;
    void refreshActionStatesForTest() { refreshActionStates(); }

signals:
    void logLine(const QString &line);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onUploadSheet();
    void onRecheckConnections();
    void onChooseApk();
    void onParallelChanged(int value);
    void onInstallWechat();
    void onRegisterSelected();
    void onCancel();
    void onOpenResultsCsv();
    void onSummaryChanged(int total, int connected, int wechatInstalled, int registered);
    void onBusyChanged(bool busy);
    void onProgress(int done, int total);
    void onControllerLog(const QString &line);
    void onHeaderSectionClicked(int section);
    void onModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

private:
    void buildUi();
    void setupAccessibility();
    void setupTabOrder();
    void appendLog(const QString &line);
    void refreshActionStates();
    void refreshApkLabel();
    void updateSummaryDisplay(int total, int connected, int wechatInstalled, int registered);
    int connectedDeviceCount() const;
    int eligibleRegisterCount() const;
    QString registerDisabledReason() const;
    bool confirmRegister(int count);
    QString resolveApkPath();
    QString dataDirectory() const;

    batch::BatchController *m_controller = nullptr;
    QString m_dataDir;
    bool m_hasSheet = false;
    ConfirmHook m_confirmHook;

    QPushButton *m_uploadBtn = nullptr;
    QPushButton *m_recheckBtn = nullptr;
    QLabel *m_apkLabel = nullptr;
    QPushButton *m_chooseApkBtn = nullptr;
    QSpinBox *m_parallelSpin = nullptr;
    QLabel *m_summaryIcon = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QTableView *m_table = nullptr;
    QPushButton *m_installBtn = nullptr;
    QPushButton *m_registerBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    QPushButton *m_openCsvBtn = nullptr;
};

#endif // BATCHDIALOG_H
