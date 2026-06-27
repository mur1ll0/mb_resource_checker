#include "motherboard_map.h"
#include "slot_item.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QPixmap>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>

MotherboardMap::MotherboardMap(QWidget* parent)
    : QWidget(parent), m_view(NULL), m_scene(NULL), m_sidebarTree(NULL), m_mbInfoLabel(NULL), m_refreshButton(NULL) {
    
    setupUI();
    runHardwareScan();
}

MotherboardMap::~MotherboardMap() {
}

void MotherboardMap::setupUI() {
    // Apply a premium dark-mode stylesheet compatible with legacy Qt4
    setStyleSheet(
        "QWidget {"
        "  background-color: #121920;"
        "  color: #f8f9fa;"
        "  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;"
        "}"
        "QTreeWidget {"
        "  background-color: #18222c;"
        "  border: 1px solid #283747;"
        "  border-radius: 6px;"
        "  color: #f8f9fa;"
        "}"
        "QTreeWidget::item {"
        "  padding: 8px;"
        "  border-bottom: 1px solid #202c38;"
        "}"
        "QTreeWidget::item:hover {"
        "  background-color: #243343;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: #00bc8c;"
        "  color: #ffffff;"
        "}"
        "QTreeWidget::item:selected:!active {"
        "  background-color: #1a4d3e;"
        "  color: #ffffff;"
        "}"
        "QHeaderView::section {"
        "  background-color: #202d3a;"
        "  color: #00bc8c;"
        "  padding: 6px;"
        "  border: 1px solid #18222c;"
        "  font-weight: bold;"
        "}"
        "QPushButton {"
        "  background-color: #00bc8c;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 8px 16px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #009c74;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #007c5c;"
        "}"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // Top Header panel
    QHBoxLayout* headerLayout = new QHBoxLayout();
    m_mbInfoLabel = new QLabel("Scanning Motherboard...", this);
    m_mbInfoLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #00bc8c;");
    m_mbInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    
    m_refreshButton = new QPushButton("Refresh Scan", this);
    connect(m_refreshButton, SIGNAL(clicked()), this, SLOT(runHardwareScan()));

    headerLayout->addWidget(m_mbInfoLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(headerLayout);

    // Body panel (Split Motherboard visual and Sidebar tree info)
    QHBoxLayout* bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(15);

    // QGraphicsView configuration
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, 680, 600);

    m_view = new QGraphicsView(m_scene, this);
    m_view->setFixedSize(682, 602);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setStyleSheet("border: 2px solid #283747; border-radius: 6px; background-color: #121920;");
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    bodyLayout->addWidget(m_view);

    // Sidebar Info tree list
    m_sidebarTree = new QTreeWidget(this);
    m_sidebarTree->setColumnCount(3);
    QStringList headers;
    headers << "Location" << "Status" << "Device Installed";
    m_sidebarTree->setHeaderLabels(headers);
#if QT_VERSION >= 0x050000
    m_sidebarTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_sidebarTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_sidebarTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
#else
    m_sidebarTree->header()->setResizeMode(0, QHeaderView::ResizeToContents);
    m_sidebarTree->header()->setResizeMode(1, QHeaderView::ResizeToContents);
    m_sidebarTree->header()->setResizeMode(2, QHeaderView::Stretch);
#endif
    m_sidebarTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_sidebarTree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));

    QAction* copyAction = new QAction(this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copySelectedRow()));
    m_sidebarTree->addAction(copyAction);

    bodyLayout->addWidget(m_sidebarTree);

    mainLayout->addLayout(bodyLayout);
    
    setWindowTitle("Hardware Motherboard Map Overlay - v1.0.0");
    resize(1250, 700);
}

std::string MotherboardMap::getMotherboardName() {
    std::vector<std::wstring> props;
    props.push_back(L"Manufacturer");
    props.push_back(L"Product");

    std::vector<std::vector<std::wstring> > results;
    if (m_scanner.queryWMI(L"SELECT Manufacturer, Product FROM Win32_BaseBoard", props, results) && !results.empty()) {
        std::string mfg(results[0][0].begin(), results[0][0].end());
        std::string prod(results[0][1].begin(), results[0][1].end());
        
        // Trim whitespace
        mfg.erase(mfg.find_last_not_of(" \t\r\n") + 1);
        mfg.erase(0, mfg.find_first_not_of(" \t\r\n"));
        prod.erase(prod.find_last_not_of(" \t\r\n") + 1);
        prod.erase(0, prod.find_first_not_of(" \t\r\n"));

        return mfg + " " + prod;
    }
    return "Generic Motherboard";
}

void MotherboardMap::runHardwareScan() {
    m_refreshButton->setEnabled(false);
    m_refreshButton->setText("Scanning...");
    
    // Set motherboard info header
    std::string mbName = getMotherboardName();
    m_cpuName = m_scanner.scanCPU();
    m_mbInfoLabel->setText(QString("Motherboard: %1  |  CPU: %2")
        .arg(QString::fromLocal8Bit(mbName.c_str()))
        .arg(QString::fromLocal8Bit(m_cpuName.c_str())));

    // Execute scan
    std::vector<SlotInfo> slotList = m_scanner.scanHardware();

    // Redraw slots visual & populate list view
    updateVisuals(slotList);

    m_refreshButton->setEnabled(true);
    m_refreshButton->setText("Refresh Scan");
}

void MotherboardMap::updateVisuals(const std::vector<SlotInfo>& slotList) {
    m_scene->clear();
    m_sidebarTree->clear();

    // Draw generic motherboard background
    QPixmap bgPixmap(":/images/motherboard_bg.png");
    if (bgPixmap.isNull()) {
        // Build futuristic vector schematic layout at a tighter 680x600 size
        bgPixmap = QPixmap(680, 600);
        bgPixmap.fill(QColor(18, 25, 32));

        QPainter painter(&bgPixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        // Circuit board grid lines
        painter.setPen(QPen(QColor(28, 38, 48), 1, Qt::DotLine));
        for (int x = 0; x < 680; x += 40) {
            painter.drawLine(x, 0, x, 600);
        }
        for (int y = 0; y < 600; y += 40) {
            painter.drawLine(0, y, 680, y);
        }

        // Draw CPU Socket Area
        painter.setPen(QPen(QColor(40, 188, 140, 150), 2));
        painter.setBrush(QBrush(QColor(23, 33, 43)));
        painter.drawRoundedRect(260, 100, 150, 140, 8.0, 8.0);
        
        // Chip pin indicators inside CPU socket
        painter.setPen(QPen(QColor(40, 188, 140, 50), 1));
        for (int i = 270; i < 400; i += 8) {
            painter.drawLine(i, 110, i, 230);
        }
        
        // Draw CPU Name word-wrapped inside the socket
        painter.setPen(QColor(0, 188, 140));
        QFont font = painter.font();
        font.setPointSize(9);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRect(265, 105, 140, 130), Qt::AlignCenter | Qt::TextWordWrap, QString::fromLocal8Bit(m_cpuName.c_str()));

        // Draw Heatsinks & Chipset
        painter.setPen(QPen(QColor(60, 75, 90), 1.5));
        painter.setBrush(QBrush(QColor(32, 43, 54)));
        painter.drawRoundedRect(440, 320, 80, 80, 4.0, 4.0);
        painter.drawText(QRect(440, 320, 80, 80), Qt::AlignCenter, "CHIPSET");

        // VRM heatsink left
        painter.drawRect(205, 90, 45, 160);
        painter.drawText(QRect(205, 90, 45, 160), Qt::AlignCenter, "VRM");
        // VRM heatsink top
        painter.drawRect(260, 45, 150, 45);

        // High-tech PCB bus tracings
        painter.setPen(QPen(QColor(0, 188, 140, 30), 1.5));
        // CPU to RAM bus
        for (int i = 0; i < 4; ++i) {
            painter.drawLine(410, 120 + i*20, 435, 120 + i*20);
        }
        // CPU to PCIe bus
        painter.drawLine(260, 220, 80, 300);
        painter.drawLine(290, 240, 80, 350);
        painter.drawLine(310, 240, 180, 270);
    }
    
    m_scene->addPixmap(bgPixmap);

    // Instanciate tree widget categories
    QTreeWidgetItem* ramGroup = new QTreeWidgetItem(m_sidebarTree);
    ramGroup->setText(0, "Memory Slots (RAM)");
    ramGroup->setExpanded(true);

    QTreeWidgetItem* pcieGroup = new QTreeWidgetItem(m_sidebarTree);
    pcieGroup->setText(0, "PCI Express Expansion");
    pcieGroup->setExpanded(true);

    QTreeWidgetItem* m2Group = new QTreeWidgetItem(m_sidebarTree);
    m2Group->setText(0, "M.2 Expansion Slots");
    m2Group->setExpanded(true);

    QTreeWidgetItem* sataGroup = new QTreeWidgetItem(m_sidebarTree);
    sataGroup->setText(0, "SATA Ports");
    sataGroup->setExpanded(true);

    int ramCount = 0;
    int pcieCount = 0;
    int m2Count = 0;
    int sataCount = 0;

    for (size_t i = 0; i < slotList.size(); ++i) {
        const SlotInfo& info = slotList[i];
        QRectF rect;

        if (info.type == SLOT_RAM) {
            // Memory slots: vertically aligned, usually on the right side of the CPU (X=440+)
            rect = QRectF(440 + ramCount * 18, 80, 12, 180);
            ramCount++;
            
            QTreeWidgetItem* item = new QTreeWidgetItem(ramGroup);
            item->setText(0, QString::fromLocal8Bit(info.name.c_str()));
            item->setText(1, info.occupied ? "Occupied" : "Free");
            item->setText(2, QString::fromLocal8Bit(info.deviceName.c_str()));
            if (info.occupied) item->setTextColor(1, QColor(220, 53, 69));
            else item->setTextColor(1, QColor(40, 167, 69));
        }
        else if (info.type == SLOT_PCIE) {
            // PCIe slots: horizontal slots below the CPU (X=80, Y=310+)
            double height = (info.name.find("x16") != std::string::npos || info.name.find("X16") != std::string::npos || pcieCount == 0) ? 14 : 10;
            double width = (height == 14) ? 320 : 150;
            
            rect = QRectF(80, 310 + pcieCount * 55, width, height);
            pcieCount++;

            QTreeWidgetItem* item = new QTreeWidgetItem(pcieGroup);
            item->setText(0, QString::fromLocal8Bit(info.name.c_str()));
            item->setText(1, info.occupied ? "Occupied" : "Free");
            item->setText(2, QString::fromLocal8Bit(info.deviceName.c_str()));
            if (info.occupied) item->setTextColor(1, QColor(220, 53, 69));
            else item->setTextColor(1, QColor(40, 167, 69));
        }
        else if (info.type == SLOT_M2) {
            // M.2 slots: rectangular cards (X=180, Y=275+, 90x24 size representing form factor 2280)
            rect = QRectF(180, 275 + m2Count * 110, 90, 24);
            m2Count++;

            QTreeWidgetItem* item = new QTreeWidgetItem(m2Group);
            item->setText(0, QString::fromLocal8Bit(info.name.c_str()));
            item->setText(1, info.occupied ? "Occupied" : "Free");
            item->setText(2, QString::fromLocal8Bit(info.deviceName.c_str()));
            if (info.occupied) item->setTextColor(1, QColor(220, 53, 69));
            else item->setTextColor(1, QColor(40, 167, 69));
        }
        else if (info.type == SLOT_SATA) {
            // SATA ports: small squares on bottom right (X=580+, Y=440+)
            rect = QRectF(580 + (sataCount % 2) * 32, 440 + (sataCount / 2) * 26, 22, 16);
            sataCount++;

            QTreeWidgetItem* item = new QTreeWidgetItem(sataGroup);
            item->setText(0, QString::fromLocal8Bit(info.name.c_str()));
            item->setText(1, info.occupied ? "Occupied" : "Free");
            item->setText(2, QString::fromLocal8Bit(info.deviceName.c_str()));
            if (info.occupied) item->setTextColor(1, QColor(220, 53, 69));
            else item->setTextColor(1, QColor(40, 167, 69));
        }

        // Add to graphics scene
        SlotItem* visualSlot = new SlotItem(info, rect);
        m_scene->addItem(visualSlot);
    }
}

void MotherboardMap::copySelectedRow() {
    QTreeWidgetItem* item = m_sidebarTree->currentItem();
    if (!item) return;
    
    int col = m_sidebarTree->currentColumn();
    if (col < 0) col = 0;
    
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(item->text(col));
}

void MotherboardMap::showContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_sidebarTree->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction* copyLoc = menu.addAction("Copy Location");
    QAction* copyDev = menu.addAction("Copy Device Installed");
    QAction* copyRow = menu.addAction("Copy Row Info");

    QAction* selectedAction = menu.exec(m_sidebarTree->mapToGlobal(pos));
    if (!selectedAction) return;

    QClipboard* clipboard = QApplication::clipboard();
    if (selectedAction == copyLoc) {
        clipboard->setText(item->text(0));
    } else if (selectedAction == copyDev) {
        clipboard->setText(item->text(2));
    } else if (selectedAction == copyRow) {
        QString text = item->text(0) + "\t" + item->text(1) + "\t" + item->text(2);
        clipboard->setText(text);
    }
}
