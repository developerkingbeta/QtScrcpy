#ifndef BATCH_BATCHMODEL_H
#define BATCH_BATCHMODEL_H

#include <QAbstractTableModel>
#include <QList>

#include "devicerow.h"

namespace batch {

class BatchModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColSelect,
        ColIp,
        ColPhone,
        ColConn,
        ColWechat,
        ColReg,
        ColDetail,
        ColCount
    };

    explicit BatchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setRows(const QList<DeviceRow> &rows);
    const DeviceRow &row(int rowIndex) const;
    void updateRow(int rowIndex, const DeviceRow &row);
    QList<int> checkedRows() const;
    void setAllChecked(bool checked);

private:
    static QString connLabel(ConnState state);
    static QString wechatLabel(const DeviceRow &row);
    static QString regLabel(RegState state);

    QList<DeviceRow> m_rows;
    QList<bool> m_checked;
};

} // namespace batch

#endif // BATCH_BATCHMODEL_H
