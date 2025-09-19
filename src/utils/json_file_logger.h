#ifndef JSON_FILE_LOGGER_H
#define JSON_FILE_LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>

class JsonFileLogger : public QObject
{
    Q_OBJECT

public:
    static JsonFileLogger& instance();
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    void setLogFilePath(const QString &path);
    void setMaxLogFileSize(qint64 size);
    void setMaxLogFiles(int count);

private:
    explicit JsonFileLogger(QObject *parent = nullptr);
    ~JsonFileLogger();
    Q_DISABLE_COPY(JsonFileLogger)

    void writeLog(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    void rotateLogs();

    QFile m_logFile;
    QTextStream m_logStream;
    QMutex m_mutex;
    QString m_logFilePath;
    qint64 m_maxLogFileSize;
    int m_maxLogFiles;
};

#endif // JSON_FILE_LOGGER_H
