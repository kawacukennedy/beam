#include <QApplication>
#include "bluelink_app.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    BlueLinkApp blueLinkApp;
    blueLinkApp.start();

    return app.exec();
}
