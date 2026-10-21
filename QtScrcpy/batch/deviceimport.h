#ifndef BATCH_DEVICEIMPORT_H
#define BATCH_DEVICEIMPORT_H

#include "devicerow.h"

#include <QList>
#include <QString>
#include <QStringList>

#include "sheetreader.h"

namespace batch {

struct ImportResult {
    QList<DeviceRow> rows;
    QStringList warnings;
    QString error;
};

class DeviceImport
{
public:
    static ImportResult fromTable(const SheetTable &table);

private:
    DeviceImport();
};

} // namespace batch

#endif // BATCH_DEVICEIMPORT_H
