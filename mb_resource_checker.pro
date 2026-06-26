QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mb_resource_checker
TEMPLATE = app

# Ensure a release-focused build that hides the console window on Windows
CONFIG += release

# Target Windows XP compatibility (0x0501) to ensure the compiler uses 
# legacy-compatible Win32 API declarations.
DEFINES += _WIN32_WINNT=0x0501

# Add C++ source files
SOURCES += src/main.cpp \
           src/hardware_scanner.cpp \
           src/motherboard_map.cpp \
           src/slot_item.cpp

# Add Header files
HEADERS += src/types.h \
           src/hardware_scanner.h \
           src/motherboard_map.h \
           src/slot_item.h

# Add Qt Resource Compiler configuration
RESOURCES += resources/resources.qrc

# Link essential low-level Windows APIs
# Works for both MSVC and MinGW toolchains
LIBS += -lsetupapi -lole32 -loleaut32 -lwbemuuid

# Directives to compile as a standard executable
# We will package dependencies dynamically using windeployqt

# Set the application icon
RC_ICONS = resources/app_icon.ico

