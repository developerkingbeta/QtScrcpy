#ifndef BATCH_SHEETREADER_H
#define BATCH_SHEETREADER_H

#include <QList>
#include <QString>
#include <QStringList>

namespace batch {

typedef QList<QStringList> SheetTable;

class SheetReader
{
public:
    static SheetTable read(const QString &path, QString *error);

private:
    SheetReader();
};

} // namespace batch

#endif // BATCH_SHEETREADER_H
