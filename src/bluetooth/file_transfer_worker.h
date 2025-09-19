#ifndef FILE_TRANSFER_WORKER_H
#define FILE_TRANSFER_WORKER_H

#include <QObject>
#include <QFile>
#include <QByteArray>
#include <QTimer>
#include <sodium.h>

// Note: MAX_BLE_WRITE_DATA_SIZE (defined in .cpp) is the effective chunk size for BLE.
// The 8MB chunk size mentioned in JSON is not feasible over BLE directly.
// This would require L2CAP or another higher-level protocol for large file transfers.

class FileTransferWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileTransferWorker(QObject *parent = nullptr);
    ~FileTransferWorker();

    void setPeripheralIdentifier(const QString &identifier);
    void setFilePath(const QString &filePath);
    void setFileName(const QString &fileName);
    void setFileSize(long fileSize);
    void setEncryptionKey(const QByteArray &key);
    void setIsSending(bool sending);
    void setCharacteristic(void *characteristic); // CBCharacteristic*
    void setPeripheral(void *peripheral); // CBPeripheral*

signals:
    void transferProgress(const QString &deviceAddress, const QString &fileName, double progress);
    void transferFinished(const QString &deviceAddress, const QString &fileName, bool success);
    void transferError(const QString &deviceAddress, const QString &fileName, const QString &error);
    void sendDataToCharacteristic(void *peripheral, void *characteristic, const QByteArray &data, bool withResponse);
    void metadataReady(const QString &deviceAddress, const QByteArray &metadata);

public slots:
    void startTransfer();
    void handleReceivedData(const QByteArray &data);
    void handleWriteConfirmation();

private:
    QString m_peripheralIdentifier;
    QString m_filePath;
    QString m_fileName;
    long m_fileSize;
    QByteArray m_encryptionKey;
    bool m_isSending;
    QFile m_file;
    long m_bytesTransferred;
    unsigned long long m_currentChunkIndex;
    QTimer m_retryTimer;
    int m_retryCount;
    QByteArray m_lastSentData; // For retry mechanism

    void *m_characteristic; // Raw pointer to CBCharacteristic
    void *m_peripheral;     // Raw pointer to CBPeripheral

    void sendNextChunk();
    void processIncomingChunk(const QByteArray &encryptedData);
    QByteArray encryptChunk(const QByteArray &chunk, unsigned long long chunkIndex);
    QByteArray decryptChunk(const QByteArray &encryptedChunk, unsigned long long chunkIndex);
};

#endif // FILE_TRANSFER_WORKER_H
