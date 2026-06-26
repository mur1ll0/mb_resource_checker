#include "slot_item.h"
#include <QPainter>
#include <QGraphicsSceneHoverEvent>
#include <QString>
#include <QStyleOptionGraphicsItem>

static QString getTypeString(SlotType t) {
    switch (t) {
        case SLOT_PCIE: return "PCI Express";
        case SLOT_M2: return "M.2 Slot";
        case SLOT_SATA: return "SATA Port";
        case SLOT_RAM: return "DDR Memory Socket";
        default: return "Slot";
    }
}

SlotItem::SlotItem(const SlotInfo& info, const QRectF& rect, QGraphicsItem* parent)
    : QGraphicsItem(parent), m_info(info), m_rect(rect), m_isHovered(false) {
    
    // Enable hover tracking for dynamic state change visual effects
    setAcceptHoverEvents(true);

    // Create a detailed rich text tooltip
    QString statusColor = m_info.occupied ? "#dc3545" : "#28a745";
    QString statusText = m_info.occupied ? "OCCUPIED" : "FREE";
    
    QString detailsText = QString::fromLocal8Bit(m_info.details.c_str());
    if (detailsText.isEmpty()) {
        detailsText = "No additional data";
    }

    QString tooltipHtml = QString(
        "<div style='font-family: Arial; font-size: 11px; color: #ffffff; background-color: #212529; padding: 6px; border-radius: 4px;'>"
        "  <b style='font-size: 13px; color: #f8f9fa;'>%1</b><br/>"
        "  <span style='color: #6c757d;'>Type:</span> %2<br/>"
        "  <span style='color: #6c757d;'>Status:</span> <b style='color: %3;'>%4</b><br/>"
        "  <span style='color: #6c757d;'>Device:</span> <b>%5</b><br/>"
        "  <span style='color: #6c757d;'>Info:</span> <small>%6</small>"
        "</div>"
    )
    .arg(QString::fromLocal8Bit(m_info.name.c_str()))
    .arg(getTypeString(m_info.type))
    .arg(statusColor)
    .arg(statusText)
    .arg(QString::fromLocal8Bit(m_info.deviceName.c_str()))
    .arg(detailsText);

    setToolTip(tooltipHtml);
}

QRectF SlotItem::boundingRect() const {
    // Return bounding rect adjusted for pen width highlights on hover
    return m_rect.adjusted(-3.0, -3.0, 3.0, 3.0);
}

void SlotItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    // Palette: Sleek Red for Occupied, Vibrant Green for Available
    QColor baseColor = m_info.occupied ? QColor(220, 53, 69) : QColor(40, 167, 69);
    
    // Smooth transition simulation for alpha transparency on hover
    int alpha = m_isHovered ? 190 : 110;
    QColor fillColor = baseColor;
    fillColor.setAlpha(alpha);

    // Border highlights
    QPen pen(baseColor, m_isHovered ? 2.5 : 1.5);
    painter->setPen(pen);
    painter->setBrush(QBrush(fillColor));

    if (m_info.type == SLOT_M2) {
        // Draw M.2 SSD board style with connector pins and screw mount notch
        painter->drawRoundedRect(m_rect, 2.0, 2.0);

        // Gold pins on the left edge (connection socket side)
        QColor goldColor(218, 165, 32);
        painter->setPen(QPen(goldColor, 1.0));
        painter->setBrush(QBrush(goldColor));
        painter->drawRect(QRectF(m_rect.x(), m_rect.y() + 4, 3, m_rect.height() - 8));

        // Circular mounting notch on the right edge (screw mount side)
        painter->setPen(QPen(baseColor, m_isHovered ? 2.5 : 1.5));
        painter->setBrush(QBrush(QColor(18, 25, 32))); // Matches motherboard background
        painter->drawEllipse(QPointF(m_rect.right() - 4, m_rect.center().y()), 3, 3);
    } else {
        painter->drawRoundedRect(m_rect, 4.0, 4.0);
    }

    // Label render inside the overlays (if size is large enough)
    if (m_rect.width() > 30 && m_rect.height() > 12) {
        painter->setPen(Qt::white);
        QFont font = painter->font();
        font.setPointSize(m_isHovered ? 8 : 7);
        font.setBold(true);
        painter->setFont(font);
        
        // Show designator inside slot overlay
        QString label = QString::fromLocal8Bit(m_info.name.c_str());
        painter->drawText(m_rect, Qt::AlignCenter, label);
    }
}

void SlotItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    Q_UNUSED(event);
    m_isHovered = true;
    update(); // Request repaint to trigger glow effect
}

void SlotItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    Q_UNUSED(event);
    m_isHovered = false;
    update(); // Request repaint to return to normal
}
