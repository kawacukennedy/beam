#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <stdbool.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function to initialize the database manager
void db_manager_init(const char* db_path);

// Function to open the database connection
bool db_open(const char* db_path);

// Function to close the database connection
bool db_close(void);

// Function to execute a SQL query (for DDL and simple DML)
bool db_execute_query(const char* query);

// Function to create all necessary tables
bool db_create_tables(void);

// Device operations
long db_add_device(const char* name, const char* mac, bool paired, long last_seen, int signal_strength);
bool db_update_device(long id, const char* name, const char* mac, bool paired, long last_seen, int signal_strength);
bool db_delete_device(long id);

typedef struct {
    long id;
    char name[256];
    char mac[18];
    bool paired;
    long last_seen;
    int signal_strength;
} DeviceInfo;

DeviceInfo* db_get_device_by_id(long id);
DeviceInfo* db_get_device_by_mac(const char* mac);
DeviceInfo** db_get_all_devices(int* count);
void db_free_device_info(DeviceInfo* device);
void db_free_device_info_array(DeviceInfo** devices, int count);

// Chat operations
typedef struct {
    long id;
    long device_id;
    long timestamp;
    char sender[256];
    char message_type[64];
    char content[2048];
} ChatMessage;

long db_add_chat_message(long device_id, long timestamp, const char* sender, const char* message_type, const char* content);
ChatMessage* db_get_chat_message_by_id(long id);
ChatMessage** db_get_chat_messages_for_device(long device_id, int* count);
void db_free_chat_message(ChatMessage* message);
void db_free_chat_message_array(ChatMessage** messages, int count);

// File Transfer operations
typedef struct {
    long id;
    long device_id;
    char filename[256];
    long size;
    double progress;
    char status[64];
    long timestamp;
} FileTransfer;

long db_add_file_transfer(long device_id, const char* filename, long size, double progress, const char* status, long timestamp);
bool db_update_file_transfer_progress(long id, double progress, const char* status);
bool db_update_file_transfer_status(long id, const char* status);
FileTransfer* db_get_file_transfer_by_id(long id);
FileTransfer** db_get_file_transfers_for_device(long device_id, int* count);
void db_free_file_transfer(FileTransfer* transfer);
void db_free_file_transfer_array(FileTransfer** transfers, int count);

#ifdef __cplusplus
}
#endif

#endif // DB_MANAGER_H