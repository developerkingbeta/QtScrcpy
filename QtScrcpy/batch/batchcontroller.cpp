#include "batchcontroller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QMetaObject>
#include <QRunnable>
#include <QThread>

#include "adbuidriver.h"
#include "deviceimport.h"
#include "deviceprobe.h"
#include "sheetreader.h"

namespace batch {

BatchController::RegistrarFactory BatchController::s_registrarFactory;

class RowJob : public QRunnable
{
public:
    RowJob(BatchController *controller,
           BatchController::JobKind kind,
           int rowIndex,
           DeviceRow row,
           QString apkPath,
           quint64 generation,
           std::atomic<bool> *cancelFlag)
        : m_controller(controller)
        , m_kind(kind)
        , m_rowIndex(rowIndex)
        , m_row(row)
        , m_apkPath(apkPath)
        , m_generation(generation)
        , m_cancelFlag(cancelFlag)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (!m_controller || !m_cancelFlag) {
            return;
        }

        const std::atomic<bool> *cancel = m_cancelFlag;
        if (cancel->load()) {
            QMetaObject::invokeMethod(m_controller,
                                      "onJobFinished",
                                      Qt::QueuedConnection,
                                      Q_ARG(int, m_rowIndex),
                                      Q_ARG(batch::DeviceRow, m_row),
                                      Q_ARG(int, static_cast<int>(m_kind)),
                                      Q_ARG(quint64, m_generation),
                                      Q_ARG(bool, true));
            return;
        }

        switch (m_kind) {
        case BatchController::JobKind::Check:
            m_row.conn = ConnState::Checking;
            m_row.wechat = WechatState::Checking;
            DeviceProbe::probeConnection(m_row, cancel);
            if (m_row.conn == ConnState::Connected && !cancel->load()) {
                DeviceProbe::probeWechat(m_row, cancel);
            } else if (m_row.conn != ConnState::Connected) {
                m_row.wechat = WechatState::Unknown;
            }
            break;
        case BatchController::JobKind::Install:
            if (m_row.conn == ConnState::Connected && m_row.wechat == WechatState::Missing) {
                m_row.wechat = WechatState::Installing;
                QString err;
                if (!DeviceProbe::installWechat(m_row, m_apkPath, &err, cancel) && !err.isEmpty()) {
                    m_row.detail = err;
                }
            }
            break;
        case BatchController::JobKind::Register: {
            const RegisterOutcome outcome = m_controller->runRegistrar(m_row);
            m_row.reg = outcome.state;
            if (!outcome.detail.isEmpty()) {
                m_row.detail = outcome.detail;
            }
            break;
        }
        }

        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(m_controller,
                                  "onJobFinished",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, m_rowIndex),
                                  Q_ARG(batch::DeviceRow, m_row),
                                  Q_ARG(int, static_cast<int>(m_kind)),
                                  Q_ARG(quint64, m_generation),
                                  Q_ARG(bool, cancelled));
    }

private:
    BatchController *m_controller;
    BatchController::JobKind m_kind;
    int m_rowIndex;
    DeviceRow m_row;
    QString m_apkPath;
    quint64 m_generation;
    std::atomic<bool> *m_cancelFlag;
};

BatchController::BatchController(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_settings(dataDir)
    , m_store(dataDir)
    , m_cancelFlag(false)
    , m_busy(false)
    , m_activeJobs(0)
    , m_completedJobs(0)
    , m_totalJobs(0)
    , m_runGeneration(0)
{
    qRegisterMetaType<batch::DeviceRow>("batch::DeviceRow");
    m_threadPool.setMaxThreadCount(m_settings.concurrency());
}

BatchController::~BatchController()
{
    cancel();
}

BatchModel *BatchController::model()
{
    return &m_model;
}

void BatchController::setRegistrarFactory(const RegistrarFactory &factory)
{
    s_registrarFactory = factory;
}

void BatchController::clearRegistrarFactory()
{
    s_registrarFactory = RegistrarFactory();
}

bool BatchController::loadSheet(const QString &path, QString *error)
{
    if (isBusy()) {
        if (error) {
            *error = QStringLiteral("Busy");
        }
        return false;
    }

    QString readError;
    const SheetTable table = SheetReader::read(path, &readError);
    if (!readError.isEmpty()) {
        if (error) {
            *error = readError;
        }
        return false;
    }

    const ImportResult imported = DeviceImport::fromTable(table);
    if (!imported.error.isEmpty()) {
        if (error) {
            *error = imported.error;
        }
        return false;
    }

    QList<DeviceRow> rows = imported.rows;
    for (DeviceRow &row : rows) {
        if (m_store.isRegistered(row.ip, row.phone)) {
            row.reg = RegState::Registered;
        }
    }

    for (const QString &warning : imported.warnings) {
        emit logLine(warning);
    }

    m_model.setRows(rows);
    emitSummary();
    checkAll();
    return true;
}

void BatchController::checkAll()
{
    if (isBusy()) {
        return;
    }

    QList<int> indices;
    for (int i = 0; i < m_model.rowCount(); ++i) {
        indices.append(i);
    }
    startJobs(JobKind::Check, indices);
}

void BatchController::ensureWechat(const QString &apkPath)
{
    if (isBusy()) {
        return;
    }

    m_settings.setApkPath(apkPath);
    m_currentApkPath = apkPath;

    QList<int> indices;
    for (int i = 0; i < m_model.rowCount(); ++i) {
        const DeviceRow &row = m_model.row(i);
        if (row.conn == ConnState::Connected) {
            indices.append(i);
        }
    }
    startJobs(JobKind::Install, indices, apkPath);
}

void BatchController::registerRows(const QList<int> &rows)
{
    if (isBusy()) {
        return;
    }

    QList<int> indices;
    for (int rowIndex : rows) {
        if (rowIndex < 0 || rowIndex >= m_model.rowCount()) {
            continue;
        }
        const DeviceRow &row = m_model.row(rowIndex);
        if (row.conn != ConnState::Connected) {
            logSkip(row, QStringLiteral("not connected"));
            continue;
        }
        if (row.wechat != WechatState::Installed) {
            logSkip(row, QStringLiteral("WeChat not installed"));
            continue;
        }
        if (row.reg == RegState::Registered || row.reg == RegState::AlreadyRegistered) {
            logSkip(row, QStringLiteral("already registered"));
            continue;
        }
        indices.append(rowIndex);
    }

    startJobs(JobKind::Register, indices);
}

void BatchController::cancel()
{
    m_cancelFlag.store(true);
    ++m_runGeneration;
    m_threadPool.waitForDone();
    if (QCoreApplication::instance()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    finalizeCancelledRows();
    m_activeJobs = 0;
    m_completedJobs = 0;
    if (m_busy) {
        setBusy(false);
    }
}

bool BatchController::isBusy() const
{
    return m_busy;
}

QString BatchController::resultsCsvPath() const
{
    return m_store.csvPath();
}

void BatchController::startJobs(JobKind kind, const QList<int> &rowIndices, const QString &apkPath)
{
    if (rowIndices.isEmpty()) {
        return;
    }

    m_threadPool.setMaxThreadCount(m_settings.concurrency());
    m_cancelFlag.store(false);
    ++m_runGeneration;
    m_activeJobs = rowIndices.size();
    m_completedJobs = 0;
    m_totalJobs = rowIndices.size();
    setBusy(true);
    emit progress(0, m_totalJobs);

    for (int rowIndex : rowIndices) {
        DeviceRow row = m_model.row(rowIndex);
        if (kind == JobKind::Check) {
            row.conn = ConnState::Checking;
            row.wechat = WechatState::Checking;
            applyRowUpdate(rowIndex, row);
        } else if (kind == JobKind::Register) {
            row.reg = RegState::Running;
            applyRowUpdate(rowIndex, row);
        }
        m_threadPool.start(new RowJob(this, kind, rowIndex, row, apkPath, m_runGeneration, &m_cancelFlag));
    }
}

void BatchController::logSkip(const DeviceRow &row, const QString &reason)
{
    emit logLine(QStringLiteral("skipped %1: %2").arg(row.ip, reason));
}

RegisterOutcome BatchController::runRegistrar(const DeviceRow &row)
{
    if (s_registrarFactory) {
        return s_registrarFactory(row, &m_cancelFlag);
    }

    AdbUiDriver driver(row.serial(), &m_cancelFlag);
    RegisterOptions opts;
    return WechatRegistrar::run(row,
                                driver,
                                opts,
                                [this](const QString &line) { emit logLine(line); });
}

void BatchController::applyRowUpdate(int rowIndex, const DeviceRow &row)
{
    m_model.updateRow(rowIndex, row);
    emitSummary();
}

void BatchController::onJobFinished(int rowIndex,
                                    DeviceRow row,
                                    int kindValue,
                                    quint64 generation,
                                    bool cancelled)
{
    Q_UNUSED(kindValue);
    if (generation != m_runGeneration) {
        return;
    }

    if (cancelled && row.reg == RegState::Running) {
        row.reg = RegState::Cancelled;
    }
    if (cancelled && row.conn == ConnState::Checking) {
        row.conn = ConnState::Unknown;
    }
    if (cancelled
        && (row.wechat == WechatState::Checking || row.wechat == WechatState::Installing)) {
        row.wechat = WechatState::Unknown;
    }

    if (kindValue == static_cast<int>(JobKind::Register)) {
        const QString detailSuffix =
            row.detail.isEmpty() ? QString() : QStringLiteral(" %1").arg(row.detail);
        if (cancelled && row.reg == RegState::Cancelled) {
            emit logLine(QStringLiteral("[CANCELLED] %1%2").arg(row.serial(), detailSuffix));
        } else if (!cancelled) {
            switch (row.reg) {
            case RegState::Registered: {
                emit logLine(QStringLiteral("[REGISTERED] %1 %2").arg(row.serial(), row.phone));
                RegistrationRecord record;
                record.time = QDateTime::currentDateTimeUtc();
                record.ip = row.ip;
                record.phone = row.phone;
                record.state = RegState::Registered;
                record.detail = row.detail;
                m_store.append(record);
                break;
            }
            case RegState::AlreadyRegistered: {
                emit logLine(QStringLiteral("[ALREADY_REGISTERED] %1%2").arg(row.serial(), detailSuffix));
                RegistrationRecord record;
                record.time = QDateTime::currentDateTimeUtc();
                record.ip = row.ip;
                record.phone = row.phone;
                record.state = RegState::AlreadyRegistered;
                record.detail = row.detail;
                m_store.append(record);
                break;
            }
            case RegState::NeedsManual:
                emit logLine(QStringLiteral("[NEEDS_MANUAL] %1%2").arg(row.serial(), detailSuffix));
                break;
            case RegState::Failed:
                emit logLine(QStringLiteral("[FAILED] %1%2").arg(row.serial(), detailSuffix));
                break;
            case RegState::Cancelled:
                emit logLine(QStringLiteral("[CANCELLED] %1%2").arg(row.serial(), detailSuffix));
                break;
            default:
                break;
            }
        }
    }

    applyRowUpdate(rowIndex, row);

    ++m_completedJobs;
    emit progress(m_completedJobs, m_totalJobs);

    if (m_completedJobs >= m_activeJobs) {
        setBusy(false);
    }
}

void BatchController::finalizeCancelledRows()
{
    for (int i = 0; i < m_model.rowCount(); ++i) {
        DeviceRow row = m_model.row(i);
        bool changed = false;
        if (row.conn == ConnState::Checking) {
            row.conn = ConnState::Unknown;
            changed = true;
        }
        if (row.wechat == WechatState::Checking || row.wechat == WechatState::Installing) {
            row.wechat = WechatState::Unknown;
            changed = true;
        }
        if (row.reg == RegState::Running) {
            row.reg = RegState::Cancelled;
            changed = true;
        }
        if (changed) {
            m_model.updateRow(i, row);
        }
    }
    emitSummary();
}

void BatchController::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(m_busy);
}

void BatchController::emitSummary()
{
    int connected = 0;
    int wechatInstalled = 0;
    int registered = 0;
    const int total = m_model.rowCount();
    for (int i = 0; i < total; ++i) {
        const DeviceRow &row = m_model.row(i);
        if (row.conn == ConnState::Connected) {
            ++connected;
        }
        if (row.wechat == WechatState::Installed) {
            ++wechatInstalled;
        }
        if (row.reg == RegState::Registered || row.reg == RegState::AlreadyRegistered) {
            ++registered;
        }
    }
    emit summaryChanged(total, connected, wechatInstalled, registered);
}

} // namespace batch
