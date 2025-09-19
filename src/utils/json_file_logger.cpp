#include "json_file_logger.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QThread>

JsonFileLogger::JsonFileLogger(QObject *parent) : 
    QObject(parent), 
    m_maxLogFileSize(10 * 1024 * 1024), // 10 MB
    m_maxLogFiles(5)
{
    // Default log file path
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    m_logFilePath = logDir + "/bluelink.log";

    m_logFile.setFileName(m_logFilePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        fprintf(stderr, "Failed to open log file: %s\n", qPrintable(m_logFile.errorString()));
    }
    m_logStream.setDevice(&m_logFile);
}

JsonFileLogger::~JsonFileLogger()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

JsonFileLogger& JsonFileLogger::instance()
{
    static JsonFileLogger logger;
    return logger;
}

void JsonFileLogger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    JsonFileLogger::instance().writeLog(type, context, msg);
}

void JsonFileLogger::setLogFilePath(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    m_logFilePath = path;
    m_logFile.setFileName(m_logFilePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        fprintf(stderr, "Failed to open log file: %s\n", qPrintable(m_logFile.errorString()));
    }
    m_logStream.setDevice(&m_logFile);
}

void JsonFileLogger::setMaxLogFileSize(qint64 size)
{
    QMutexLocker locker(&m_mutex);
    m_maxLogFileSize = size;
}

void JsonFileLogger::setMaxLogFiles(int count)
{
    QMutexLocker locker(&m_mutex);
    m_maxLogFiles = count;
}

void JsonFileLogger::writeLog(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&m_mutex);

    if (!m_logFile.isOpen()) {
        return;
    }

    QString levelString;
    switch (type) {
        case QtDebugMsg:    levelString = "DEBUG"; break;
        case QtInfoMsg:     levelString = "INFO"; break;
        case QtWarningMsg:  levelString = "WARN"; break;
        case QtCriticalMsg: levelString = "ERROR"; break;
        case QtFatalMsg:    levelString = "FATAL"; break;
    }

    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = levelString;
    logEntry["message"] = msg;
    logEntry["thread"] = QString::number((quintptr)QThread::currentThreadId());
    if (context.file) {
        logEntry["file"] = QString::fromUtf8(context.file);
    }
    if (context.line) {
        logEntry["line"] = context.line;
    }
    if (context.function) {
        logEntry["function"] = QString::fromUtf8(context.function);
    }
    if (context.category) {
        logEntry["category"] = QString::fromUtf8(context.category);
    }

    m_logStream << QJsonDocument(logEntry).toJson(QJsonDocument::Compact) << "\n";
    m_logStream.flush();

    if (m_logFile.size() > m_maxLogFileSize) {
        rotateLogs();
    }
}

void JsonFileLogger::rotateLogs()
{
    m_logFile.close();

    QFileInfo fileInfo(m_logFilePath);
    QDir logDir = fileInfo.dir();
    QString baseName = fileInfo.baseName();
    QString suffix = fileInfo.suffix();

    // Delete oldest log file
    QString oldestLog = QString("%1/%2.%3.%4").arg(logDir.path()).arg(baseName).arg(m_maxLogFiles - 1).arg(suffix);
    if (QFile::exists(oldestLog)) {
        QFile::remove(oldestLog);
    }

    // Rename existing log files
    for (int i = m_maxLogFiles - 2; i >= 0; --i) {
        QString oldName = QString("%1/%2.%3.%4").arg(logDir.path()).arg(baseName).arg(i).arg(suffix);
        QString newName = QString("%1/%2.%3.%4").arg(logDir.path()).arg(baseName).arg(i + 1).arg(suffix);
        if (QFile::exists(oldName)) {
            QFile::rename(oldName, newName);
        }
    }

    // Rename current log file to .0
    QString currentLog = QString("%1/%2.%3").arg(logDir.path()).arg(baseName).arg(suffix);
    QString newCurrentLog = QString("%1/%2.0.%3").arg(logDir.path()).arg(baseName).arg(suffix);
    if (QFile::exists(currentLog)) {
        QFile::rename(currentLog, newCurrentLog);
    }

    // Reopen the main log file
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        fprintf(stderr, "Failed to reopen log file after rotation: %s\n", qPrintable(m_logFile.errorString()));
    }
    m_logStream.setDevice(&m_logFile);
}
