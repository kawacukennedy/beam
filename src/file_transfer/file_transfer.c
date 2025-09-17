#include "file_transfer.h"
#include <stdio.h>
#include <stdlib.h> // For malloc/free
#include <string.h> // For memcpy

// Dummy structure to simulate a file transfer
typedef struct {
    int id;
    int device_id;
    char file_path[512];
    long long file_size;
    FileTransferStatus status;
    int chunks_total;
    int chunks_sent;
    FileTransferProgressCallback callback;
    // FILE* file_handle; // In a real implementation
} InternalFileTransfer;

static InternalFileTransfer transfers[10]; // Simple array for dummy transfers
static int next_transfer_id = 1;

void file_transfer_init(void) {
    printf("File Transfer module initialized.\n");
    for (int i = 0; i < 10; ++i) {
        transfers[i].id = 0; // Mark as unused
    }
}

int file_transfer_send_file(int device_id, const char* file_path, FileTransferProgressCallback callback) {
    printf("Initiating file transfer for '%s' to device %d (dummy)...\n", file_path, device_id);

    int id = next_transfer_id++;
    if (id > 10) { // Simple overflow check
        fprintf(stderr, "Too many dummy transfers.\n");
        return -1;
    }

    InternalFileTransfer* ft = &transfers[id - 1];
    ft->id = id;
    ft->device_id = device_id;
    strncpy(ft->file_path, file_path, sizeof(ft->file_path) - 1);
    ft->file_path[sizeof(ft->file_path) - 1] = '\0';
    ft->file_size = 1024 * 1024 * 10; // Dummy 10MB file
    ft->status = FILE_TRANSFER_STATUS_TRANSFERRING;
    ft->chunks_total = 10; // Dummy 10 chunks
    ft->chunks_sent = 0;
    ft->callback = callback;

    if (callback) {
        callback(id, 0.0, FILE_TRANSFER_STATUS_TRANSFERRING);
    }
    return id;
}

int file_transfer_receive_request(int device_id, const char* filename, long long file_size, FileTransferProgressCallback callback) {
    printf("Receiving file request for '%s' from device %d (dummy)...\n", filename, device_id);

    int id = next_transfer_id++;
    if (id > 10) {
        fprintf(stderr, "Too many dummy transfers.\n");
        return -1;
    }

    InternalFileTransfer* ft = &transfers[id - 1];
    ft->id = id;
    ft->device_id = device_id;
    snprintf(ft->file_path, sizeof(ft->file_path), "/tmp/received_%s", filename); // Dummy save path
    ft->file_size = file_size;
    ft->status = FILE_TRANSFER_STATUS_PENDING;
    ft->chunks_total = (int)(file_size / (8 * 1024 * 1024)) + 1; // 8MB chunks
    ft->chunks_sent = 0;
    ft->callback = callback;

    if (callback) {
        callback(id, 0.0, FILE_TRANSFER_STATUS_PENDING);
    }
    return id;
}

bool file_transfer_pause(int transfer_id) {
    printf("Pausing file transfer %d (dummy)...\n", transfer_id);
    if (transfer_id > 0 && transfer_id <= 10 && transfers[transfer_id - 1].id == transfer_id) {
        transfers[transfer_id - 1].status = FILE_TRANSFER_STATUS_PAUSED;
        if (transfers[transfer_id - 1].callback) {
            transfers[transfer_id - 1].callback(transfer_id, (double)transfers[transfer_id - 1].chunks_sent / transfers[transfer_id - 1].chunks_total, FILE_TRANSFER_STATUS_PAUSED);
        }
        return true;
    }
    return false;
}

bool file_transfer_resume(int transfer_id) {
    printf("Resuming file transfer %d (dummy)...\n", transfer_id);
    if (transfer_id > 0 && transfer_id <= 10 && transfers[transfer_id - 1].id == transfer_id) {
        transfers[transfer_id - 1].status = FILE_TRANSFER_STATUS_TRANSFERRING;
        if (transfers[transfer_id - 1].callback) {
            transfers[transfer_id - 1].callback(transfer_id, (double)transfers[transfer_id - 1].chunks_sent / transfers[transfer_id - 1].chunks_total, FILE_TRANSFER_STATUS_TRANSFERRING);
        }
        return true;
    }
    return false;
}

bool file_transfer_cancel(int transfer_id) {
    printf("Cancelling file transfer %d (dummy)...\n", transfer_id);
    if (transfer_id > 0 && transfer_id <= 10 && transfers[transfer_id - 1].id == transfer_id) {
        transfers[transfer_id - 1].status = FILE_TRANSFER_STATUS_FAILED; // Or a specific cancelled status
        if (transfers[transfer_id - 1].callback) {
            transfers[transfer_id - 1].callback(transfer_id, (double)transfers[transfer_id - 1].chunks_sent / transfers[transfer_id - 1].chunks_total, FILE_TRANSFER_STATUS_FAILED);
        }
        transfers[transfer_id - 1].id = 0; // Mark as unused
        return true;
    }
    return false;
}

bool file_transfer_process_incoming_chunk(int transfer_id, unsigned long long chunk_index,
                                          const unsigned char* chunk_data, size_t chunk_len) {
    printf("Processing incoming chunk %llu for transfer %d (dummy)...\n", chunk_index, transfer_id);
    if (transfer_id > 0 && transfer_id <= 10 && transfers[transfer_id - 1].id == transfer_id) {
        transfers[transfer_id - 1].chunks_sent++;
        double progress = (double)transfers[transfer_id - 1].chunks_sent / transfers[transfer_id - 1].chunks_total;
        if (transfers[transfer_id - 1].callback) {
            transfers[transfer_id - 1].callback(transfer_id, progress, transfers[transfer_id - 1].status);
        }
        if (transfers[transfer_id - 1].chunks_sent >= transfers[transfer_id - 1].chunks_total) {
            transfers[transfer_id - 1].status = FILE_TRANSFER_STATUS_COMPLETED;
            if (transfers[transfer_id - 1].callback) {
                transfers[transfer_id - 1].callback(transfer_id, 1.0, FILE_TRANSFER_STATUS_COMPLETED);
            }
        }
        return true;
    }
    return false;
}

bool file_transfer_get_next_chunk(int transfer_id, unsigned long long* chunk_index,
                                  unsigned char* chunk_buffer, size_t chunk_buffer_len,
                                  size_t* actual_chunk_len) {
    printf("Getting next chunk for transfer %d (dummy)...\n", transfer_id);
    if (transfer_id > 0 && transfer_id <= 10 && transfers[transfer_id - 1].id == transfer_id) {
        if (transfers[transfer_id - 1].chunks_sent < transfers[transfer_id - 1].chunks_total) {
            *chunk_index = transfers[transfer_id - 1].chunks_sent;
            // Dummy chunk data
            size_t len = (chunk_buffer_len < 1024) ? chunk_buffer_len : 1024; // Max 1KB dummy data
            memset(chunk_buffer, (int)('A' + *chunk_index % 26), len);
            *actual_chunk_len = len;

            transfers[transfer_id - 1].chunks_sent++;
            double progress = (double)transfers[transfer_id - 1].chunks_sent / transfers[transfer_id - 1].chunks_total;
            if (transfers[transfer_id - 1].callback) {
                transfers[transfer_id - 1].callback(transfer_id, progress, transfers[transfer_id - 1].status);
            }
            if (transfers[transfer_id - 1].chunks_sent >= transfers[transfer_id - 1].chunks_total) {
                transfers[transfer_id - 1].status = FILE_TRANSFER_STATUS_COMPLETED;
                if (transfers[transfer_id - 1].callback) {
                    transfers[transfer_id - 1].callback(transfer_id, 1.0, FILE_TRANSFER_STATUS_COMPLETED);
                }
            }
            return true;
        }
    }
    return false;
}
