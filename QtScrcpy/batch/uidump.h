#ifndef BATCH_UIDUMP_H
#define BATCH_UIDUMP_H

#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

namespace batch {

struct UiNode
{
    QString text;
    QString desc;
    QString resId;
    QString cls;
    QRect bounds;
    bool clickable = false;
    bool enabled = true;
    bool checked = false;

    QPoint center() const
    {
        const int right = bounds.left() + bounds.width();
        const int bottom = bounds.top() + bounds.height();
        return QPoint((bounds.left() + right) / 2, (bounds.top() + bottom) / 2);
    }
};

class UiDump
{
public:
    static QList<UiNode> parse(const QString &xml);
    static const UiNode *findAny(const QList<UiNode> &nodes,
                                 const QStringList &textsOrDescs,
                                 bool exact = false);
    static QList<UiNode> editTexts(const QList<UiNode> &nodes);

private:
    UiDump();
};

} // namespace batch

#endif // BATCH_UIDUMP_H
