#ifndef SLOT_ITEM_H
#define SLOT_ITEM_H

#include <QGraphicsItem>
#include <QRectF>
#include "types.h"

class SlotItem : public QGraphicsItem {
public:
    SlotItem(const SlotInfo& info, const QRectF& rect, QGraphicsItem* parent = NULL);

    // Required overrides for custom QGraphicsItem
    QRectF boundingRect() const;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

protected:
    // Hover event overrides to support hover highlight micro-interactions
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);

private:
    SlotInfo m_info;
    QRectF m_rect;
    bool m_isHovered;
};

#endif // SLOT_ITEM_H
