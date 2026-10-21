#ifndef BATCH_BATCHCONTROLLER_H
#define BATCH_BATCHCONTROLLER_H

#include <QObject>
#include <QThreadPool>

#include <atomic>
#include <functional>

#include "batchmodel.h"
#include "batchsettings.h"
#include "registrationstore.h"
#include "wechatregistrar.h"

namespace batch {

class RowJob;

class BatchController : public QObject
{
    Q_OBJECT

public:
    using RegistrarFactory = std::function<RegisterOutcome(const DeviceRow &, const std::atomic<bool> *)>;

    explicit BatchController(const QString &dataDir, QObject *parent = nullptr);
    ~BatchController() override;

    BatchModel *model();

    bool loadSheet(const QString &path, QString *error);
    void checkAll();
    void ensureWechat(const QString &apkPath);
    void registerRows(const QList<int> &rows);
    void cancel();
    bool isBusy() const;
    QString resultsCsvPath() const;

    static void setRegistrarFactory(const RegistrarFactory &factory);
    static void clearRegistrarFactory();

public slots:
    void onJobFinished(int rowIndex,
                       batch::DeviceRow row,
                       int kindValue,
                       quint64 generation,
                       bool cancelled);

signals:
    void logLine(const QString &line);
    void progress(int done, int total);
    void busyChanged(bool busy);
    void summaryChanged(int total, int connected, int wechatInstalled, int registered);

private:
    friend class RowJob;

    enum class JobKind { Check, Install, Register };

    void startJobs(JobKind kind, const QList<int> &rowIndices, const QString &apkPath = QString());
    void applyRowUpdate(int rowIndex, const DeviceRow &row);
    void finalizeCancelledRows();
    void setBusy(bool busy);
    void emitSummary();
    RegisterOutcome runRegistrar(const DeviceRow &row);
    void logSkip(const DeviceRow &row, const QString &reason);

    BatchModel m_model;
    BatchSettings m_settings;
    RegistrationStore m_store;
    QThreadPool m_threadPool;
    std::atomic<bool> m_cancelFlag;
    bool m_busy;
    int m_activeJobs;
    int m_completedJobs;
    int m_totalJobs;
    quint64 m_runGeneration;
    QString m_currentApkPath;

    static RegistrarFactory s_registrarFactory;
};

} // namespace batch

#endif // BATCH_BATCHCONTROLLER_H
