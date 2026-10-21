#include "batchdialog.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

#include "batch/batchcontroller.h"
#include "batch/batchmodel.h"
#include "batch/batchsettings.h"
#include "batch/devicerow.h"
#include "config.h"

BatchDialog::BatchDialog(QWidget *parent, const QString &dataDir)
    : QDialog(parent)
    , m_dataDir(dataDir)
{
    setWindowTitle(tr("Batch WeChat Registration"));
    resize(960, 640);

    m_controller = new batch::BatchController(dataDirectory(), this);

    buildUi();
    setupAccessibility();
    setupTabOrder();

    connect(m_controller, &batch::BatchController::summaryChanged, this, &BatchDialog::onSummaryChanged);
    connect(m_controller, &batch::BatchController::busyChanged, this, &BatchDialog::onBusyChanged);
    connect(m_controller, &batch::BatchController::progress, this, &BatchDialog::onProgress);
    connect(m_controller, &batch::BatchController::logLine, this, &BatchDialog::onControllerLog);
    connect(m_controller->model(), &batch::BatchModel::dataChanged, this, &BatchDialog::onModelDataChanged);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, this, &BatchDialog::onHeaderSectionClicked);

    {
        batch::BatchSettings settings(dataDirectory());
        m_parallelSpin->setValue(settings.concurrency());
    }

    refreshApkLabel();
    updateSummaryDisplay(0, 0, 0, 0);
    onBusyChanged(false);
    refreshActionStates();
}

void BatchDialog::setConfirmHook(const ConfirmHook &hook)
{
    m_confirmHook = hook;
}

void BatchDialog::setTestRows(const QList<batch::DeviceRow> &rows)
{
    m_controller->model()->setRows(rows);
    m_hasSheet = !rows.isEmpty();
    int wechatInstalled = 0;
    int registered = 0;
    for (const batch::DeviceRow &row : rows) {
        if (row.wechat == batch::WechatState::Installed) {
            ++wechatInstalled;
        }
        if (row.reg == batch::RegState::Registered || row.reg == batch::RegState::AlreadyRegistered) {
            ++registered;
        }
    }
    updateSummaryDisplay(rows.size(), connectedDeviceCount(), wechatInstalled, registered);
    refreshActionStates();
}

batch::BatchModel *BatchDialog::modelForTest() const
{
    return m_controller->model();
}

QString BatchDialog::dataDirectory() const
{
    if (!m_dataDir.isEmpty()) {
        return m_dataDir;
    }
    const QString path = Config::getInstance().getConfigDirectory() + QStringLiteral("/wechat-batch");
    QDir().mkpath(path);
    return path;
}

void BatchDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);

    auto *toolbar = new QHBoxLayout();
    m_uploadBtn = new QPushButton(tr("Upload Excel..."), this);
    m_uploadBtn->setObjectName(QStringLiteral("batchUploadBtn"));
    m_recheckBtn = new QPushButton(tr("Re-check connections"), this);
    m_recheckBtn->setObjectName(QStringLiteral("batchRecheckBtn"));
    toolbar->addWidget(m_uploadBtn);
    toolbar->addWidget(m_recheckBtn);
    toolbar->addStretch();
    root->addLayout(toolbar);

    auto *apkRow = new QHBoxLayout();
    m_apkCaptionLabel = new QLabel(tr("WeChat APK:"), this);
    m_apkCaptionLabel->setObjectName(QStringLiteral("batchApkCaptionLabel"));
    m_apkLabel = new QLabel(this);
    m_apkLabel->setObjectName(QStringLiteral("batchApkPathLabel"));
    m_chooseApkBtn = new QPushButton(tr("Choose APK..."), this);
    m_chooseApkBtn->setObjectName(QStringLiteral("batchChooseApkBtn"));
    apkRow->addWidget(m_apkCaptionLabel);
    apkRow->addWidget(m_apkLabel, 1);
    apkRow->addWidget(m_chooseApkBtn);
    root->addLayout(apkRow);

    auto *parallelRow = new QHBoxLayout();
    m_parallelCaptionLabel = new QLabel(tr("Parallel devices"), this);
    m_parallelCaptionLabel->setObjectName(QStringLiteral("batchParallelCaptionLabel"));
    m_parallelSpin = new QSpinBox(this);
    m_parallelSpin->setObjectName(QStringLiteral("batchParallelSpin"));
    m_parallelSpin->setRange(1, 8);
    parallelRow->addWidget(m_parallelCaptionLabel);
    parallelRow->addWidget(m_parallelSpin);
    parallelRow->addStretch();
    root->addLayout(parallelRow);

    auto *summaryRow = new QHBoxLayout();
    m_summaryIcon = new QLabel(this);
    m_summaryIcon->setObjectName(QStringLiteral("batchSummaryIcon"));
    m_summaryWarningLabel = new QLabel(this);
    m_summaryWarningLabel->setObjectName(QStringLiteral("batchSummaryWarningLabel"));
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("batchSummaryLabel"));
    summaryRow->addWidget(m_summaryIcon);
    summaryRow->addWidget(m_summaryWarningLabel);
    summaryRow->addWidget(m_summaryLabel, 1);
    root->addLayout(summaryRow);

    m_table = new QTableView(this);
    m_table->setObjectName(QStringLiteral("batchDeviceTable"));
    m_table->setModel(m_controller->model());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setSortingEnabled(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    root->addWidget(m_table, 1);

    auto *actionRow = new QHBoxLayout();
    m_installBtn = new QPushButton(tr("Check / Install WeChat"), this);
    m_installBtn->setObjectName(QStringLiteral("batchInstallBtn"));
    m_registerBtn = new QPushButton(tr("Register selected"), this);
    m_registerBtn->setObjectName(QStringLiteral("batchRegisterBtn"));
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName(QStringLiteral("batchCancelBtn"));
    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("batchProgressBar"));
    m_progressBar->setTextVisible(true);
    actionRow->addWidget(m_installBtn);
    actionRow->addWidget(m_registerBtn);
    actionRow->addWidget(m_cancelBtn);
    actionRow->addWidget(m_progressBar, 1);
    root->addLayout(actionRow);

    auto *logRow = new QHBoxLayout();
    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setObjectName(QStringLiteral("batchLogEdit"));
    m_logEdit->setReadOnly(true);
    m_openCsvBtn = new QPushButton(tr("Open results CSV"), this);
    m_openCsvBtn->setObjectName(QStringLiteral("batchOpenCsvBtn"));
    auto *logColumn = new QVBoxLayout();
    logColumn->addWidget(m_logEdit, 1);
    logRow->addLayout(logColumn, 1);
    logRow->addWidget(m_openCsvBtn);
    root->addLayout(logRow);

    connect(m_uploadBtn, &QPushButton::clicked, this, &BatchDialog::onUploadSheet);
    connect(m_recheckBtn, &QPushButton::clicked, this, &BatchDialog::onRecheckConnections);
    connect(m_chooseApkBtn, &QPushButton::clicked, this, &BatchDialog::onChooseApk);
    connect(m_parallelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &BatchDialog::onParallelChanged);
    connect(m_installBtn, &QPushButton::clicked, this, &BatchDialog::onInstallWechat);
    connect(m_registerBtn, &QPushButton::clicked, this, &BatchDialog::onRegisterSelected);
    connect(m_cancelBtn, &QPushButton::clicked, this, &BatchDialog::onCancel);
    connect(m_openCsvBtn, &QPushButton::clicked, this, &BatchDialog::onOpenResultsCsv);
}

void BatchDialog::setupAccessibility()
{
    m_uploadBtn->setAccessibleName(tr("Upload Excel sheet"));
    m_recheckBtn->setAccessibleName(tr("Re-check device connections"));
    m_apkCaptionLabel->setAccessibleName(tr("WeChat APK label"));
    m_apkLabel->setAccessibleName(tr("WeChat APK path not chosen"));
    m_chooseApkBtn->setAccessibleName(tr("Choose WeChat APK file"));
    m_parallelCaptionLabel->setAccessibleName(tr("Parallel devices label"));
    m_parallelSpin->setAccessibleName(tr("Parallel devices"));
    m_summaryIcon->setAccessibleName(tr("Summary status warning icon"));
    m_summaryWarningLabel->setAccessibleName(tr("Summary connection warning"));
    m_summaryLabel->setAccessibleName(tr("Device batch summary"));
    m_table->setAccessibleName(tr("Device batch table"));
    m_installBtn->setAccessibleName(tr("Check and install WeChat"));
    m_registerBtn->setAccessibleName(tr("Register selected devices"));
    m_cancelBtn->setAccessibleName(tr("Cancel batch operation"));
    m_logEdit->setAccessibleName(tr("Batch operation log"));
    m_openCsvBtn->setAccessibleName(tr("Open results CSV file"));
    m_progressBar->setAccessibleName(tr("Batch progress"));
}

void BatchDialog::setupTabOrder()
{
    setTabOrder(m_uploadBtn, m_recheckBtn);
    setTabOrder(m_recheckBtn, m_chooseApkBtn);
    setTabOrder(m_chooseApkBtn, m_parallelSpin);
    setTabOrder(m_parallelSpin, m_table);
    setTabOrder(m_table, m_installBtn);
    setTabOrder(m_installBtn, m_registerBtn);
    setTabOrder(m_registerBtn, m_cancelBtn);
    setTabOrder(m_cancelBtn, m_logEdit);
}

void BatchDialog::appendLog(const QString &line)
{
    const QString stamped = QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ') + line;
    m_logEdit->appendPlainText(stamped);
}

void BatchDialog::refreshApkLabel()
{
    batch::BatchSettings settings(dataDirectory());
    const QString path = settings.apkPath();
    if (path.isEmpty()) {
        m_apkLabel->setText(tr("not chosen"));
        m_apkLabel->setAccessibleName(tr("WeChat APK path not chosen"));
    } else {
        m_apkLabel->setText(path);
        m_apkLabel->setAccessibleName(tr("WeChat APK path %1").arg(path));
    }
}

int BatchDialog::connectedDeviceCount() const
{
    int count = 0;
    const batch::BatchModel *model = m_controller->model();
    for (int i = 0; i < model->rowCount(); ++i) {
        if (model->row(i).conn == batch::ConnState::Connected) {
            ++count;
        }
    }
    return count;
}

int BatchDialog::eligibleRegisterCount() const
{
    int count = 0;
    const batch::BatchModel *model = m_controller->model();
    for (int idx : model->checkedRows()) {
        const batch::DeviceRow &row = model->row(idx);
        if (row.conn == batch::ConnState::Connected && row.wechat == batch::WechatState::Installed
            && row.reg != batch::RegState::Registered && row.reg != batch::RegState::AlreadyRegistered) {
            ++count;
        }
    }
    return count;
}

QString BatchDialog::registerDisabledReason() const
{
    const batch::BatchModel *model = m_controller->model();
    const QList<int> checked = model->checkedRows();
    if (checked.isEmpty()) {
        return tr("Select at least one connected device with WeChat installed that is not already registered.");
    }

    bool hasConnectedChecked = false;
    bool hasInstalledChecked = false;
    bool hasUnregisteredChecked = false;
    for (int idx : checked) {
        const batch::DeviceRow &row = model->row(idx);
        if (row.conn == batch::ConnState::Connected) {
            hasConnectedChecked = true;
        }
        if (row.wechat == batch::WechatState::Installed) {
            hasInstalledChecked = true;
        }
        if (row.reg != batch::RegState::Registered && row.reg != batch::RegState::AlreadyRegistered) {
            hasUnregisteredChecked = true;
        }
    }

    if (!hasConnectedChecked) {
        return tr("Select at least one connected device with WeChat installed that is not already registered.");
    }
    if (!hasInstalledChecked) {
        return tr("Selected rows must have WeChat installed.");
    }
    if (!hasUnregisteredChecked) {
        return tr("Selected rows are already registered.");
    }
    return tr("Select at least one connected device with WeChat installed that is not already registered.");
}

void BatchDialog::refreshActionStates()
{
    const bool busy = m_controller->isBusy();
    const int connected = connectedDeviceCount();
    const int eligible = eligibleRegisterCount();

    const bool installEnabled = !busy && m_hasSheet && connected >= 1;
    m_installBtn->setEnabled(installEnabled);
    if (installEnabled) {
        m_installBtn->setToolTip(QString());
    } else if (!m_hasSheet || connected < 1) {
        m_installBtn->setToolTip(tr("Upload a sheet and wait for at least one connected device"));
    } else {
        m_installBtn->setToolTip(QString());
    }

    const bool registerEnabled = !busy && eligible >= 1;
    m_registerBtn->setEnabled(registerEnabled);
    m_registerBtn->setToolTip(registerEnabled ? QString() : registerDisabledReason());

    m_uploadBtn->setEnabled(!busy);
    m_recheckBtn->setEnabled(!busy && m_hasSheet);
    m_chooseApkBtn->setEnabled(!busy);
    m_parallelSpin->setEnabled(!busy);
    m_table->setEnabled(!busy);
    m_openCsvBtn->setEnabled(!busy);
}

void BatchDialog::updateSummaryDisplay(int total, int connected, int wechatInstalled, int registered)
{
    const QString summary = tr("%1 devices | %2 connected | %3 WeChat installed | %4 registered")
                                .arg(total)
                                .arg(connected)
                                .arg(wechatInstalled)
                                .arg(registered);
    m_summaryLabel->setText(summary);

    if (total > 0 && connected < total) {
        const QString warningText = tr("Not all devices are connected.");
        const QIcon warningIcon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
        m_summaryIcon->setPixmap(warningIcon.pixmap(16, 16));
        m_summaryIcon->setToolTip(warningText);
        m_summaryWarningLabel->setText(warningText);
    } else {
        m_summaryIcon->clear();
        m_summaryIcon->setToolTip(QString());
        m_summaryWarningLabel->clear();
    }
}

bool BatchDialog::confirmRegister(int count)
{
    if (m_confirmHook) {
        return m_confirmHook(count);
    }
    const QString text =
        tr("Register %1 WeChat accounts with the listed phone numbers? Automation cannot solve "
           "captchas; those devices will be marked \"Needs manual\".")
            .arg(count);
    return QMessageBox::question(this, tr("Confirm registration"), text) == QMessageBox::Yes;
}

void BatchDialog::closeEvent(QCloseEvent *event)
{
    if (m_controller->isBusy()) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            tr("Batch in progress"),
            tr("Operations are still running. Cancel them and close?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_controller->cancel();
    }
    QDialog::closeEvent(event);
}

void BatchDialog::onUploadSheet()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open device sheet"),
        QString(),
        tr("Spreadsheets (*.xlsx *.csv);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_controller->loadSheet(path, &error)) {
        QMessageBox::warning(this, tr("Import failed"), error);
        return;
    }
    m_hasSheet = true;
    appendLog(tr("Loaded sheet: %1").arg(path));
    refreshActionStates();
}

void BatchDialog::onRecheckConnections()
{
    m_controller->checkAll();
}

void BatchDialog::onChooseApk()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose WeChat APK"), QString(), tr("Android packages (*.apk);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    batch::BatchSettings settings(dataDirectory());
    settings.setApkPath(path);
    refreshApkLabel();
}

QString BatchDialog::resolveApkPath()
{
    batch::BatchSettings settings(dataDirectory());
    QString path = settings.apkPath();
    if (!path.isEmpty()) {
        return path;
    }
    path = QFileDialog::getOpenFileName(
        this, tr("Choose WeChat APK"), QString(), tr("Android packages (*.apk);;All files (*)"));
    if (!path.isEmpty()) {
        settings.setApkPath(path);
        refreshApkLabel();
    }
    return path;
}

void BatchDialog::onParallelChanged(int value)
{
    batch::BatchSettings settings(dataDirectory());
    settings.setConcurrency(value);
}

void BatchDialog::onInstallWechat()
{
    const QString apkPath = resolveApkPath();
    if (apkPath.isEmpty()) {
        return;
    }
    m_controller->ensureWechat(apkPath);
}

void BatchDialog::onRegisterSelected()
{
    const int count = eligibleRegisterCount();
    if (count < 1 || !confirmRegister(count)) {
        return;
    }
    m_controller->registerRows(m_controller->model()->checkedRows());
}

void BatchDialog::onCancel()
{
    m_controller->cancel();
}

void BatchDialog::onOpenResultsCsv()
{
    const QString csvPath = m_controller->resultsCsvPath();
    if (csvPath.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(csvPath));
}

void BatchDialog::onSummaryChanged(int total, int connected, int wechatInstalled, int registered)
{
    updateSummaryDisplay(total, connected, wechatInstalled, registered);
    refreshActionStates();
}

void BatchDialog::onBusyChanged(bool busy)
{
    m_cancelBtn->setVisible(busy);
    m_progressBar->setVisible(busy);
    if (!busy) {
        m_progressBar->setValue(0);
    }
    refreshActionStates();
}

void BatchDialog::onProgress(int done, int total)
{
    m_progressBar->setMaximum(qMax(1, total));
    m_progressBar->setValue(done);
}

void BatchDialog::onControllerLog(const QString &line)
{
    appendLog(line);
    emit logLine(line);
}

void BatchDialog::onHeaderSectionClicked(int section)
{
    if (section != batch::BatchModel::ColSelect) {
        return;
    }
    batch::BatchModel *model = m_controller->model();
    const int rowCount = model->rowCount();
    const QList<int> checked = model->checkedRows();
    const bool allChecked = rowCount > 0 && checked.size() == rowCount;
    model->setAllChecked(!allChecked);
    refreshActionStates();
}

void BatchDialog::onModelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(topLeft);
    Q_UNUSED(bottomRight);
    if (roles.isEmpty() || roles.contains(Qt::CheckStateRole)) {
        refreshActionStates();
    }
}
