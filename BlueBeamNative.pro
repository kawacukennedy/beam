QT += core gui widgets qml quick

CONFIG += c++17

SOURCES += \
    src/main.cpp \
    src/bluelink_app.cpp \
    src/ui/ui_manager.cpp \
    src/bluetooth/bluetooth_manager.c \
    src/crypto/crypto_manager.c \
    src/database/db_manager.c \
    src/event_loop/event_loop.c

HEADERS += \
    src/bluelink_app.h \
    src/ui/ui_manager.h \
    src/bluetooth/bluetooth_callbacks.h \
    src/bluetooth/bluetooth_linux.h \
    src/bluetooth/bluetooth_macos.h \
    src/bluetooth/bluetooth_manager.h \
    src/bluetooth/bluetooth_windows.h \
    src/crypto/crypto_manager.h \
    src/database/db_manager.h \
    src/event_loop/event_loop.h \
    src/file_transfer/file_transfer.h \
    src/notifications/notification_manager.h \
    src/ui/ui_manager.h \
    src/utils/base64.h

# Platform-specific sources
macx:SOURCES += \
    src/bluetooth/bluetooth_macos.m \
    src/notifications/notification_manager.m

win32:SOURCES += \
    src/bluetooth/bluetooth_windows.c

unix:!macx:SOURCES += \
    src/bluetooth/bluetooth_linux.c

# QML files
QML_IMPORT_PATH += src/qml
QML_FILES += \
    src/qml/main.qml \
    src/qml/Theme.qml

# Resources
RESOURCES += qml.qrc
QMAKE_RCC_INPUTS += qml.qrc
QMAKE_RESOURCE_FLAGS = # Set to empty to prevent implicit -root
RCC_NAME = qml_resources # Explicitly set the resource name

# Other libraries (adjust paths as necessary)
LIBS += -L/usr/local/lib -L/opt/homebrew/lib -lsodium
LIBS += -lsqlite3

# macOS specific frameworks
macx:LIBS += -framework CoreBluetooth -framework Foundation -framework UserNotifications # Added UserNotifications.framework

# Linux specific libraries
unix:!macx:LIBS += -lglib-2.0 -lnotify -lsystemd -ljson-c

# Windows specific libraries
win32:LIBS += Bthprops.lib BluetoothApis.lib Setupapi.lib

# Include directories
INCLUDEPATH += src \
               src/bluetooth \
               src/crypto \
               src/database \
               src/ui \
               src/file_transfer \
               src/event_loop \
               src/notifications \
               /usr/local/include \
               /opt/homebrew/include

# Destination for the executable
DESTDIR = build/bin

# The application name
TARGET = BlueBeamNative_GUI
