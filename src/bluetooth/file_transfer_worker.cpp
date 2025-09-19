#include "file_transfer_worker.h"
#include "crypto_manager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>

#define MAX_BLE_WRITE_DATA_SIZE 500 // Max data size for a single BLE write (considering MTU and overhead)
#define MAX_RETRIES 3
#define RETRY_DELAY_MS 500

FileTransferWorker::FileTransferWorker(QObject *parent) :
    QObject(parent),
    m_fileSize(0),
    m_isSending(false),
    m_bytesTransferred(0),
    m_currentChunkIndex(0),
    m_retryCount(0),
    m_characteristic(nullptr),
    m_peripheral(nullptr)
{
    connect(&m_retryTimer, &QTimer::timeout, this, &FileTransferWorker::sendNextChunk);
}

FileTransferWorker::~FileTransferWorker()
{
    if (m_file.isOpen()) {
        m_file.close();
    }
}

void FileTransferWorker::setPeripheralIdentifier(const QString &identifier)
{
    m_peripheralIdentifier = identifier;
}

void FileTransferWorker::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
}

void FileTransferWorker::setFileName(const QString &fileName)
{
    m_fileName = fileName;
}

void FileTransferWorker::setFileSize(long fileSize)
{
    m_fileSize = fileSize;
}

void FileTransferWorker::setEncryptionKey(const QByteArray &key)
{
    m_encryptionKey = key;
}

void FileTransferWorker::setIsSending(bool sending)
{
    m_isSending = sending;
}

void FileTransferWorker::setCharacteristic(void *characteristic)
{
    m_characteristic = characteristic;
}

void FileTransferWorker::setPeripheral(void *peripheral)
{
    m_peripheral = peripheral;
}

void FileTransferWorker::startTransfer()
{
    if (m_isSending) {
        m_file.setFileName(m_filePath);
        if (!m_file.open(QIODevice::ReadOnly)) {
            emit transferError(m_peripheralIdentifier, m_fileName, QString("Could not open file for reading: %1").arg(m_file.errorString()));
            emit transferFinished(m_peripheralIdentifier, m_fileName, false);
            return;
        }

        // Send metadata first (chunk 0)
        QJsonObject metadata;
        metadata["filename"] = m_fileName;
        metadata["size"] = (qint64)m_fileSize;
        metadata["key"] = QString(m_encryptionKey.toBase64());
        QJsonDocument doc(metadata);
        QByteArray metadataBytes = doc.toJson(QJsonDocument::Compact);

        QByteArray encryptedMetadata = encryptChunk(metadataBytes, 0);
        if (encryptedMetadata.isEmpty()) {
            emit transferError(m_peripheralIdentifier, m_fileName, "Failed to encrypt metadata.");
            emit transferFinished(m_peripheralIdentifier, m_fileName, false);
            return;
        }
        m_lastSentData = encryptedMetadata;
        emit sendDataToCharacteristic(m_peripheral, m_characteristic, encryptedMetadata, true);
        m_currentChunkIndex++; // Increment for the next data chunk
        qDebug() << "Sent file metadata for" << m_fileName << ". Size:" << m_fileSize << ". Starting data transfer...";
    } else {
        m_file.setFileName(m_filePath);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            emit transferError(m_peripheralIdentifier, m_fileName, QString("Could not open file for writing: %1").arg(m_file.errorString()));
            emit transferFinished(m_peripheralIdentifier, m_fileName, false);
            return;
        }
        qDebug() << "Ready to receive file" << m_fileName << "to" << m_filePath;
    }
}

void FileTransferWorker::sendNextChunk()
{
    m_retryTimer.stop();

    if (m_bytesTransferred >= m_fileSize) {
        qDebug() << "File" << m_fileName << "sent completely.";
        m_file.close();
        emit transferFinished(m_peripheralIdentifier, m_fileName, true);
        return;
    }

    QByteArray chunk = m_file.read(MAX_BLE_WRITE_DATA_SIZE);
    if (chunk.isEmpty() && m_file.error() != QFile::NoError) {
        emit transferError(m_peripheralIdentifier, m_fileName, QString("Error reading file: %1").arg(m_file.errorString()));
        emit transferFinished(m_peripheralIdentifier, m_fileName, false);
        return;
    }

    QByteArray encryptedChunk = encryptChunk(chunk, m_currentChunkIndex);
    if (encryptedChunk.isEmpty()) {
        emit transferError(m_peripheralIdentifier, m_fileName, "Failed to encrypt file chunk.");
        emit transferFinished(m_peripheralIdentifier, m_fileName, false);
        return;
    }

    m_lastSentData = encryptedChunk;
    emit sendDataToCharacteristic(m_peripheral, m_characteristic, encryptedChunk, true);
    m_bytesTransferred += chunk.size();
    m_currentChunkIndex++;
    m_retryCount = 0; // Reset retry count for new chunk
    emit transferProgress(m_peripheralIdentifier, m_fileName, (double)m_bytesTransferred / m_fileSize);
}

void FileTransferWorker::handleReceivedData(const QByteArray &data)
{
    if (m_isSending) {
        // This worker is sending, so it shouldn't receive data chunks
        qWarning() << "FileTransferWorker (sending) received unexpected data.";
        return;
    }

    if (m_currentChunkIndex == 0) {
        // This should be the metadata, but it's handled in bluetooth_macos.m
        // This worker is only for data chunks after metadata is processed.
        qWarning() << "FileTransferWorker (receiving) received data before metadata was processed.";
        return;
    }

    processIncomingChunk(data);
}

void FileTransferWorker::handleWriteConfirmation()
{
    if (m_isSending) {
        m_retryCount = 0; // Successful write, reset retry count
        sendNextChunk();
    } else {
        qWarning() << "FileTransferWorker (receiving) received unexpected write confirmation.";
    }
}

void FileTransferWorker::processIncomingChunk(const QByteArray &encryptedData)
{
    QByteArray decryptedData = decryptChunk(encryptedData, m_currentChunkIndex);
    if (decryptedData.isEmpty()) {
        emit transferError(m_peripheralIdentifier, m_fileName, "Failed to decrypt file chunk.");
        // Consider retry or abort
        return;
    }

    if (m_file.write(decryptedData) == -1) {
        emit transferError(m_peripheralIdentifier, m_fileName, QString("Error writing decrypted data to file: %1").arg(m_file.errorString()));
        emit transferFinished(m_peripheralIdentifier, m_fileName, false);
        return;
    }

    m_bytesTransferred += decryptedData.size();
    m_currentChunkIndex++;
    emit transferProgress(m_peripheralIdentifier, m_fileName, (double)m_bytesTransferred / m_fileSize);

    if (m_bytesTransferred >= m_fileSize) {
        qDebug() << "File" << m_fileName << "received completely.";
        m_file.close();
        emit transferFinished(m_peripheralIdentifier, m_fileName, true);
    }
}

QByteArray FileTransferWorker::encryptChunk(const QByteArray &chunk, unsigned long long chunkIndex)
{
    if (m_encryptionKey.size() != crypto_secretbox_KEYBYTES) {
        qWarning() << "Encryption key has incorrect size.";
        return QByteArray();
    }

    QByteArray encryptedData(chunk.size() + crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES, 0);
    size_t actual_ciphertext_len = 0;

    if (crypto_encrypt_file_chunk(
            (const unsigned char*)chunk.constData(), chunk.size(),
            (const unsigned char*)m_encryptionKey.constData(),
            chunkIndex,
            (unsigned char*)encryptedData.data(), encryptedData.size(),
            &actual_ciphertext_len)) {
        encryptedData.resize(actual_ciphertext_len);
        return encryptedData;
    } else {
        qWarning() << "crypto_encrypt_file_chunk failed.";
        return QByteArray();
    }
}

QByteArray FileTransferWorker::decryptChunk(const QByteArray &encryptedChunk, unsigned long long chunkIndex)
{
    if (m_encryptionKey.size() != crypto_secretbox_KEYBYTES) {
        qWarning() << "Encryption key has incorrect size.";
        return QByteArray();
    }

    QByteArray decryptedData(encryptedChunk.size(), 0);
    size_t actual_decrypted_len = 0;

    if (crypto_decrypt_file_chunk(
            (const unsigned char*)encryptedChunk.constData(), encryptedChunk.size(),
            (const unsigned char*)m_encryptionKey.constData(),
            chunkIndex,
            (unsigned char*)decryptedData.data(), decryptedData.size(),
            &actual_decrypted_len)) {
        decryptedData.resize(actual_decrypted_len);
        return decryptedData;
    } else {
        qWarning() << "crypto_decrypt_file_chunk failed.";
        return QByteArray();
    }
}
