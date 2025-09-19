#include <QApplication>
#include "bluelink_app.h"
#include "ui/ui_manager.h"
#include "utils/json_file_logger.h"

int main(int argc, char *argv[]) {
    // Install custom message handler for JSON logging
    qInstallMessageHandler(JsonFileLogger::messageHandler);

    QApplication app(argc, argv);

    BlueLinkApp blueLinkApp;
    blueLinkApp.start();

    return app.exec();
}
