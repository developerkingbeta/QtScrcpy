#include "sheetreader.h"

#include "miniz.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QPair>
#include <QTextStream>
#include <QXmlStreamReader>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

namespace batch {

namespace {

QString localPath(const QString &path)
{
    return QFileInfo(path).absoluteFilePath();
}

bool parseCellRef(const QString &ref, int *outRow1, int *outCol0)
{
    int i = 0;
    while (i < ref.size() && ref.at(i).isLetter()) {
        ++i;
    }
    if (i == 0 || i >= ref.size()) {
        return false;
    }
    const QString colLetters = ref.left(i).toUpper();
    bool ok = false;
    const int row1 = ref.mid(i).toInt(&ok);
    if (!ok || row1 < 1) {
        return false;
    }

    int col0 = 0;
    for (int j = 0; j < colLetters.size(); ++j) {
        col0 = col0 * 26 + (colLetters.at(j).unicode() - QLatin1Char('A').unicode() + 1);
    }
    col0 -= 1;

    if (outRow1) {
        *outRow1 = row1;
    }
    if (outCol0) {
        *outCol0 = col0;
    }
    return true;
}

QStringList parseSharedStrings(const QByteArray &xml)
{
    QStringList strings;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("si")) {
            QString text;
            while (!(reader.isEndElement() && reader.name() == QLatin1String("si"))) {
                reader.readNext();
                if (reader.isStartElement() && reader.name() == QLatin1String("t")) {
                    text += reader.readElementText();
                }
            }
            strings.append(text);
        }
    }
    return strings;
}

SheetTable parseWorksheet(const QByteArray &xml, const QStringList &sharedStrings)
{
    QMap<QPair<int, int>, QString> cells;
    int maxRow0 = -1;
    int maxCol0 = -1;

    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("c")) {
            continue;
        }

        const QString ref = reader.attributes().value(QLatin1String("r")).toString();
        int row1 = 0;
        int col0 = 0;
        if (!parseCellRef(ref, &row1, &col0)) {
            continue;
        }
        const int row0 = row1 - 1;
        maxRow0 = qMax(maxRow0, row0);
        maxCol0 = qMax(maxCol0, col0);

        const QString cellType = reader.attributes().value(QLatin1String("t")).toString();
        QString value;
        while (!(reader.isEndElement() && reader.name() == QLatin1String("c"))) {
            reader.readNext();
            if (reader.isStartElement() && reader.name() == QLatin1String("v")) {
                value = reader.readElementText();
            } else if (reader.isStartElement() && reader.name() == QLatin1String("is")) {
                while (!(reader.isEndElement() && reader.name() == QLatin1String("is"))) {
                    reader.readNext();
                    if (reader.isStartElement() && reader.name() == QLatin1String("t")) {
                        value += reader.readElementText();
                    }
                }
            }
        }

        if (cellType == QLatin1String("s")) {
            bool ok = false;
            const int idx = value.toInt(&ok);
            if (ok && idx >= 0 && idx < sharedStrings.size()) {
                value = sharedStrings.at(idx);
            }
        }

        cells.insert(qMakePair(row0, col0), value);
    }

    SheetTable table;
    if (maxRow0 < 0) {
        return table;
    }
    for (int r = 0; r <= maxRow0; ++r) {
        QStringList row;
        for (int c = 0; c <= maxCol0; ++c) {
            row.append(cells.value(qMakePair(r, c)));
        }
        table.append(row);
    }
    return table;
}

bool worksheetUsesSharedStringCells(const QByteArray &xml)
{
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("c")) {
            if (reader.attributes().value(QLatin1String("t")) == QLatin1String("s")) {
                return true;
            }
        }
    }
    return false;
}

QByteArray extractZipEntry(const QString &zipPath, const char *entryName, QString *error)
{
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);

    const QByteArray zipPathUtf8 = QFile::encodeName(zipPath);
    if (!mz_zip_reader_init_file(&zip, zipPathUtf8.constData(), 0)) {
        if (error) {
            *error = QStringLiteral("Failed to open xlsx archive");
        }
        return QByteArray();
    }

    size_t size = 0;
    void *data = mz_zip_reader_extract_file_to_heap(&zip, entryName, &size, 0);
    mz_zip_reader_end(&zip);

    if (!data) {
        if (error) {
            *error = QStringLiteral("Missing %1 in xlsx").arg(QString::fromUtf8(entryName));
        }
        return QByteArray();
    }

    QByteArray bytes(static_cast<const char *>(data), static_cast<int>(size));
    mz_free(data);
    return bytes;
}

SheetTable readXlsx(const QString &path, QString *error)
{
    const QByteArray sheetXml = extractZipEntry(path, "xl/worksheets/sheet1.xml", error);
    if (sheetXml.isEmpty()) {
        return SheetTable();
    }

    const bool needsSharedStrings = worksheetUsesSharedStringCells(sheetXml);
    QByteArray sharedXml;
    if (needsSharedStrings) {
        sharedXml = extractZipEntry(path, "xl/sharedStrings.xml", error);
        if (sharedXml.isEmpty()) {
            return SheetTable();
        }
    } else {
        QString ignoredErr;
        sharedXml = extractZipEntry(path, "xl/sharedStrings.xml", &ignoredErr);
    }

    const QStringList sharedStrings = parseSharedStrings(sharedXml);
    return parseWorksheet(sheetXml, sharedStrings);
}

QStringList parseCsvLine(const QString &line)
{
    QStringList fields;
    QString field;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (inQuotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                    field.append(QLatin1Char('"'));
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.append(ch);
            }
        } else if (ch == QLatin1Char('"')) {
            inQuotes = true;
        } else if (ch == QLatin1Char(',')) {
            fields.append(field);
            field.clear();
        } else {
            field.append(ch);
        }
    }
    fields.append(field);
    return fields;
}

SheetTable readCsv(const QString &path, QString *error)
{
    Q_UNUSED(error);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Failed to open CSV file");
        }
        return SheetTable();
    }

    QByteArray raw = file.readAll();
    if (raw.startsWith("\xEF\xBB\xBF")) {
        raw = raw.mid(3);
    }

    SheetTable table;
    QBuffer buffer(&raw);
    buffer.open(QIODevice::ReadOnly);
    QTextStream stream(&buffer);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.isEmpty() && stream.atEnd()) {
            break;
        }
        table.append(parseCsvLine(line));
    }
    return table;
}

} // namespace

SheetReader::SheetReader()
{
}

SheetTable SheetReader::read(const QString &path, QString *error)
{
    if (error) {
        error->clear();
    }

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("xls")) {
        if (error) {
            *error = QStringLiteral("Save as .xlsx");
        }
        return SheetTable();
    }
    if (suffix == QLatin1String("xlsx")) {
        return readXlsx(localPath(path), error);
    }
    if (suffix == QLatin1String("csv")) {
        return readCsv(localPath(path), error);
    }

    if (error) {
        *error = QStringLiteral("Unsupported file type");
    }
    return SheetTable();
}

} // namespace batch
