#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include "bluelink_app.h"
#include "ui/ui_manager.h"
#include "utils/json_file_logger.h" // Keep this if it's still used for other logging

// Custom message handler to log output to a file
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString logFilePath = "app_log.txt"; // Log file name

    QFile outFile(logFilePath);
    // Open in append mode, create if not exists
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&outFile);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
        switch (type) {
            case QtDebugMsg:
                ts << "[Debug] ";
                break;
            case QtInfoMsg:
                ts << "[Info] ";
                break;
            case QtWarningMsg:
                ts << "[Warning] ";
                break;
            case QtCriticalMsg:
                ts << "[Critical] ";
                break;
            case QtFatalMsg:
                ts << "[Fatal] ";
                break;
        }
        ts << msg << " (" << context.file << ":" << context.line << ", " << context.function << ")\n";
        outFile.close();
    }

    // Also call the default handler to ensure messages are still printed to console if possible
    // or if there's another handler installed.
    // This might be redundant if the original problem is that console output isn't visible.
    // But it's good practice.
    // You can comment this out if you only want file logging.
    // qInstallMessageHandler(0); // Temporarily uninstall to prevent recursion
    // qt_message_output(type, context, msg); // Call default handler
    // qInstallMessageHandler(customMessageHandler); // Reinstall
}


int main(int argc, char *argv[]) {
    // Install custom message handler for file logging
    qInstallMessageHandler(customMessageHandler);

    // Install custom message handler for JSON logging (if still needed, otherwise remove)
    // qInstallMessageHandler(JsonFileLogger::messageHandler); // This will be overridden by customMessageHandler

    QApplication app(argc, argv);

    BlueLinkApp blueLinkApp;
    blueLinkApp.start();

    return app.exec();
}