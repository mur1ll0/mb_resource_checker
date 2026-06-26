#include <QApplication>
#include "motherboard_map.h"

int main(int argc, char *argv[]) {
    // Enable high DPI scaling if supported (for newer Windows 10/11 machines)
#if QT_VERSION >= 0x050600
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication app(argc, argv);
    
    MotherboardMap mainMap;
    mainMap.show();
    
    return app.exec();
}
