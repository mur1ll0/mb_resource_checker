#ifndef MOTHERBOARD_MAP_H
#define MOTHERBOARD_MAP_H

#include <QWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <vector>
#include <string>
#include "hardware_scanner.h"
#include "types.h"

class MotherboardMap : public QWidget {
    Q_OBJECT
public:
    MotherboardMap(QWidget* parent = NULL);
    ~MotherboardMap();

private Q_SLOTS:
    void runHardwareScan();
    void copySelectedRow();
    void showContextMenu(const QPoint& pos);

private:
    void setupUI();
    void updateVisuals(const std::vector<SlotInfo>& slotList);
    std::string getMotherboardName();

    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
    QTreeWidget* m_sidebarTree;
    QLabel* m_mbInfoLabel;
    QPushButton* m_refreshButton;
    HardwareScanner m_scanner;
    std::string m_cpuName;
};

#endif // MOTHERBOARD_MAP_H
