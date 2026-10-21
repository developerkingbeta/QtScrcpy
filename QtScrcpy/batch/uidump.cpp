#include "uidump.h"

#include <algorithm>

#include <QRegularExpression>
#include <QXmlStreamReader>

namespace batch {

namespace {

bool parseBoolAttr(const QString &value, bool defaultValue)
{
    if (value.isEmpty()) {
        return defaultValue;
    }
    return value == QLatin1String("true");
}

QRect parseBounds(const QString &value)
{
    static const QRegularExpression re(
        QStringLiteral(R"(\[(\d+),(\d+)\]\[(\d+),(\d+)\])"));
    const QRegularExpressionMatch match = re.match(value);
    if (!match.hasMatch()) {
        return QRect();
    }
    const int left = match.captured(1).toInt();
    const int top = match.captured(2).toInt();
    const int right = match.captured(3).toInt();
    const int bottom = match.captured(4).toInt();
    return QRect(left, top, right - left, bottom - top);
}

void readNode(QXmlStreamReader &reader, QList<UiNode> *out)
{
    const QXmlStreamAttributes attrs = reader.attributes();
    UiNode node;
    node.text = attrs.value(QStringLiteral("text")).toString();
    node.desc = attrs.value(QStringLiteral("content-desc")).toString();
    node.resId = attrs.value(QStringLiteral("resource-id")).toString();
    node.cls = attrs.value(QStringLiteral("class")).toString();
    node.bounds = parseBounds(attrs.value(QStringLiteral("bounds")).toString());
    node.clickable = parseBoolAttr(attrs.value(QStringLiteral("clickable")).toString(), false);
    node.enabled = parseBoolAttr(attrs.value(QStringLiteral("enabled")).toString(), true);
    node.checked = parseBoolAttr(attrs.value(QStringLiteral("checked")).toString(), false);
    out->append(node);

    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("node")) {
            readNode(reader, out);
        } else {
            reader.skipCurrentElement();
        }
    }
}

bool matchesNeedle(const QString &haystack, const QString &needle, bool exact)
{
    if (exact) {
        return haystack.compare(needle, Qt::CaseInsensitive) == 0;
    }
    return haystack.contains(needle, Qt::CaseInsensitive);
}

bool nodeMatches(const UiNode &node, const QStringList &needles, bool exact)
{
    for (const QString &needle : needles) {
        if (matchesNeedle(node.text, needle, exact) || matchesNeedle(node.desc, needle, exact)) {
            return true;
        }
    }
    return false;
}

} // namespace

QList<UiNode> UiDump::parse(const QString &xml)
{
    QList<UiNode> nodes;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("node")) {
            readNode(reader, &nodes);
        }
    }
    return nodes;
}

const UiNode *UiDump::findAny(const QList<UiNode> &nodes,
                              const QStringList &textsOrDescs,
                              bool exact)
{
    for (const UiNode &node : nodes) {
        if (nodeMatches(node, textsOrDescs, exact)) {
            return &node;
        }
    }
    return nullptr;
}

QList<UiNode> UiDump::editTexts(const QList<UiNode> &nodes)
{
    QList<UiNode> edits;
    for (const UiNode &node : nodes) {
        if (node.cls == QLatin1String("android.widget.EditText")) {
            edits.append(node);
        }
    }
    std::sort(edits.begin(), edits.end(), [](const UiNode &a, const UiNode &b) {
        if (a.bounds.top() != b.bounds.top()) {
            return a.bounds.top() < b.bounds.top();
        }
        return a.bounds.left() < b.bounds.left();
    });
    return edits;
}

} // namespace batch
