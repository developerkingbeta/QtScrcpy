#include "batchmodel.h"

#include <QApplication>
#include <QBrush>
#include <QPalette>
#include <QStyle>

namespace batch {

BatchModel::BatchModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int BatchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

int BatchModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColCount;
}

QString BatchModel::connLabel(ConnState state)
{
    switch (state) {
    case ConnState::Unknown:
        return QStringLiteral("Unknown");
    case ConnState::Checking:
        return QStringLiteral("Checking");
    case ConnState::Connected:
        return QStringLiteral("Connected");
    case ConnState::Unauthorized:
        return QStringLiteral("Unauthorized (accept prompt on phone)");
    case ConnState::Offline:
        return QStringLiteral("Offline");
    case ConnState::Unreachable:
        return QStringLiteral("Unreachable");
    case ConnState::Invalid:
        return QStringLiteral("Invalid");
    }
    return QStringLiteral("Unknown");
}

QString BatchModel::wechatLabel(const DeviceRow &row)
{
    switch (row.wechat) {
    case WechatState::Unknown:
        return QStringLiteral("Unknown");
    case WechatState::Checking:
        return QStringLiteral("Checking");
    case WechatState::Missing:
        return QStringLiteral("Missing");
    case WechatState::Installed:
        if (row.wechatVersion.isEmpty()) {
            return QStringLiteral("Installed");
        }
        return QStringLiteral("Installed ") + row.wechatVersion;
    case WechatState::Installing:
        return QStringLiteral("Installing");
    case WechatState::InstallFailed:
        return QStringLiteral("Install failed");
    }
    return QStringLiteral("Unknown");
}

QString BatchModel::regLabel(RegState state)
{
    switch (state) {
    case RegState::Idle:
        return QString();
    case RegState::Running:
        return QStringLiteral("Running");
    case RegState::Registered:
        return QStringLiteral("Registered");
    case RegState::AlreadyRegistered:
        return QStringLiteral("Already registered");
    case RegState::NeedsManual:
        return QStringLiteral("Needs manual");
    case RegState::Failed:
        return QStringLiteral("Failed");
    case RegState::Cancelled:
        return QStringLiteral("Cancelled");
    }
    return QString();
}

QVariant BatchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const DeviceRow &device = m_rows.at(index.row());
    const Column column = static_cast<Column>(index.column());

    if (role == Qt::CheckStateRole && column == ColSelect) {
        return m_checked.value(index.row()) ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::DisplayRole) {
        switch (column) {
        case ColSelect:
            return QVariant();
        case ColIp:
            return device.serial();
        case ColPhone:
            return device.phone;
        case ColConn:
            return connLabel(device.conn);
        case ColWechat:
            return wechatLabel(device);
        case ColReg:
            return regLabel(device.reg);
        case ColDetail:
            return device.detail;
        case ColCount:
            break;
        }
    }

    if (role == Qt::DecorationRole && QApplication::style()) {
        QStyle::StandardPixmap icon = QStyle::SP_MessageBoxInformation;
        switch (column) {
        case ColConn:
            switch (device.conn) {
            case ConnState::Connected:
                icon = QStyle::SP_DialogApplyButton;
                break;
            case ConnState::Unauthorized:
            case ConnState::Offline:
            case ConnState::Unreachable:
            case ConnState::Invalid:
                icon = QStyle::SP_MessageBoxWarning;
                break;
            default:
                icon = QStyle::SP_MessageBoxInformation;
                break;
            }
            break;
        case ColWechat:
            switch (device.wechat) {
            case WechatState::Installed:
                icon = QStyle::SP_DialogApplyButton;
                break;
            case WechatState::Missing:
            case WechatState::InstallFailed:
                icon = QStyle::SP_MessageBoxWarning;
                break;
            default:
                icon = QStyle::SP_MessageBoxInformation;
                break;
            }
            break;
        case ColReg:
            switch (device.reg) {
            case RegState::Registered:
            case RegState::AlreadyRegistered:
                icon = QStyle::SP_DialogApplyButton;
                break;
            case RegState::NeedsManual:
            case RegState::Failed:
                icon = QStyle::SP_MessageBoxWarning;
                break;
            default:
                icon = QStyle::SP_MessageBoxInformation;
                break;
            }
            break;
        default:
            return QVariant();
        }
        return QApplication::style()->standardIcon(icon);
    }

    if (role == Qt::ForegroundRole) {
        const QPalette palette = QApplication::palette();
        switch (column) {
        case ColConn:
            if (device.conn == ConnState::Connected) {
                return QBrush(palette.color(QPalette::Link));
            }
            if (device.conn == ConnState::Unauthorized || device.conn == ConnState::Offline
                || device.conn == ConnState::Unreachable || device.conn == ConnState::Invalid) {
                return QBrush(palette.color(QPalette::ToolTipText));
            }
            break;
        case ColWechat:
            if (device.wechat == WechatState::Installed) {
                return QBrush(palette.color(QPalette::Link));
            }
            if (device.wechat == WechatState::Missing || device.wechat == WechatState::InstallFailed) {
                return QBrush(palette.color(QPalette::ToolTipText));
            }
            break;
        case ColReg:
            if (device.reg == RegState::Registered || device.reg == RegState::AlreadyRegistered) {
                return QBrush(palette.color(QPalette::Link));
            }
            if (device.reg == RegState::NeedsManual || device.reg == RegState::Failed) {
                return QBrush(palette.color(QPalette::ToolTipText));
            }
            break;
        default:
            break;
        }
    }

    return QVariant();
}

QVariant BatchModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }
    switch (static_cast<Column>(section)) {
    case ColSelect:
        return QStringLiteral("Select");
    case ColIp:
        return QStringLiteral("Device");
    case ColPhone:
        return QStringLiteral("Phone");
    case ColConn:
        return QStringLiteral("Connection");
    case ColWechat:
        return QStringLiteral("WeChat");
    case ColReg:
        return QStringLiteral("Registration");
    case ColDetail:
        return QStringLiteral("Detail");
    case ColCount:
        break;
    }
    return QVariant();
}

bool BatchModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.column() != ColSelect || role != Qt::CheckStateRole) {
        return false;
    }
    if (index.row() < 0 || index.row() >= m_checked.size()) {
        return false;
    }
    m_checked[index.row()] = value.toInt() == Qt::Checked;
    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

Qt::ItemFlags BatchModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == ColSelect) {
        flags |= Qt::ItemIsUserCheckable;
    }
    return flags;
}

void BatchModel::setRows(const QList<DeviceRow> &rows)
{
    beginResetModel();
    m_rows = rows;
    m_checked.clear();
    for (int i = 0; i < rows.size(); ++i) {
        m_checked.append(true);
    }
    endResetModel();
}

const DeviceRow &BatchModel::row(int rowIndex) const
{
    return m_rows.at(rowIndex);
}

void BatchModel::updateRow(int rowIndex, const DeviceRow &row)
{
    if (rowIndex < 0 || rowIndex >= m_rows.size()) {
        return;
    }
    m_rows[rowIndex] = row;
    const QModelIndex topLeft = index(rowIndex, 0);
    const QModelIndex bottomRight = index(rowIndex, ColCount - 1);
    emit dataChanged(topLeft, bottomRight);
}

QList<int> BatchModel::checkedRows() const
{
    QList<int> rows;
    for (int i = 0; i < m_checked.size(); ++i) {
        if (m_checked.at(i)) {
            rows.append(i);
        }
    }
    return rows;
}

void BatchModel::setAllChecked(bool checked)
{
    for (int i = 0; i < m_checked.size(); ++i) {
        m_checked[i] = checked;
    }
    if (!m_checked.isEmpty()) {
        emit dataChanged(index(0, ColSelect), index(m_checked.size() - 1, ColSelect), {Qt::CheckStateRole});
    }
}

} // namespace batch
