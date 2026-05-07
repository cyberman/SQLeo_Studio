#ifndef CENTEREDICONITEMDELEGATE_H
#define CENTEREDICONITEMDELEGATE_H

#include "gui_global.h"
#include <QStyledItemDelegate>

class GUI_API_EXPORT CenteredIconItemDelegate : public QStyledItemDelegate
{
    public:
        explicit CenteredIconItemDelegate(QObject* parent = nullptr);

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

#endif // CENTEREDICONITEMDELEGATE_H
