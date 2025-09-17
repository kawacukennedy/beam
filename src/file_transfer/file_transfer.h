#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <stdbool.h>
#include <stddef.h>

// Enum for file transfer status
typedef enum {
    FILE_TRANSFER_STATUS_PENDING,
    FILE_TRANSFER_STATUS_TRANSFERRING,
    FILE_TRANSFER_STATUS_PAUSED,
    FILE_TRANSFER_STATUS_COMPLETED,
    FILE_TRANSFER_STATUS_FAILED
} FileTransferStatus;

// Callback for file transfer progress updates
typedef void (*FileTransferProgressCallback)(int transfer_id, double progress, FileTransferStatus status);

// Function to initialize the file transfer module
void file_transfer_init(void);

// Function to start sending a file
int file_transfer_send_file(int device_id, const char* file_path, FileTransferProgressCallback callback);

// Function to handle receiving a file request
int file_transfer_receive_request(int device_id, const char* filename, long long file_size, FileTransferProgressCallback callback);

// Function to pause a file transfer
bool file_transfer_pause(int transfer_id);

// Function to resume a file transfer
bool file_transfer_resume(int transfer_id);

// Function to cancel a file transfer
bool file_transfer_cancel(int transfer_id);

// Function to process an incoming file chunk
bool file_transfer_process_incoming_chunk(int transfer_id, unsigned long long chunk_index,
                                          const unsigned char* chunk_data, size_t chunk_len);

// Function to get the next chunk to send
bool file_transfer_get_next_chunk(int transfer_id, unsigned long long* chunk_index,
                                  unsigned char* chunk_buffer, size_t chunk_buffer_len,
                                  size_t* actual_chunk_len);

#endif // FILE_TRANSFER_H
