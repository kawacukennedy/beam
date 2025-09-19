#include "db_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For malloc, realloc, free

static sqlite3 *db = NULL; // Global database connection handle

void db_manager_init(const char* db_path) {
    printf("Database Manager initialized with path: %s\n", db_path);
    // No specific initialization needed for SQLite itself beyond opening the database.
    // This function can be used for any pre-connection setup if necessary.
}

bool db_open(const char* db_path) {
    if (db != NULL) {
        printf("Database already open. Closing existing connection.\n");
        db_close();
    }

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = NULL;
        return false;
    }
    printf("Database opened successfully: %s\n", db_path);

    // Enable foreign key constraints
    if (!db_execute_query("PRAGMA foreign_keys = ON;")) {
        fprintf(stderr, "Failed to enable foreign key constraints.\n");
        return false;
    }

    return true;
}

bool db_close(void) {
    if (db) {
        int rc = sqlite3_close(db);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Cannot close database: %s\n", sqlite3_errmsg(db));
            return false;
        }
        db = NULL;
        printf("Database closed successfully.\n");
    }
    return true;
}

bool db_execute_query(const char* query) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot execute query.\n");
        return false;
    }

    char *err_msg = 0;
    int rc = sqlite3_exec(db, query, 0, 0, &err_msg);

    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    sqlite3_free(err_msg);
    return true;
}

bool db_create_tables(void) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot create tables.\n");
        return false;
    }

    const char *create_devices_table =
        "CREATE TABLE IF NOT EXISTS devices (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    name TEXT NOT NULL,\n"
        "    mac TEXT UNIQUE NOT NULL,\n"
        "    paired BOOLEAN DEFAULT FALSE,\n"
        "    last_seen INTEGER,\n"
        "    signal_strength INTEGER\n"
        ");";

    const char *create_chats_table =
        "CREATE TABLE IF NOT EXISTS chats (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    device_id INTEGER NOT NULL,\n"
        "    timestamp INTEGER NOT NULL,\n"
        "    sender TEXT NOT NULL,\n"
        "    message_type TEXT NOT NULL,\n"
        "    content TEXT NOT NULL,\n"
        "    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE\n"
        ");";

    const char *create_file_transfers_table =
        "CREATE TABLE IF NOT EXISTS file_transfers (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    device_id INTEGER NOT NULL,\n"
        "    filename TEXT NOT NULL,\n"
        "    size INTEGER NOT NULL,\n"
        "    progress REAL DEFAULT 0.0,\n"
        "    status TEXT NOT NULL,\n"
        "    timestamp INTEGER NOT NULL,\n"
        "    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE\n"
        ");";

    if (!db_execute_query(create_devices_table)) {
        fprintf(stderr, "Failed to create devices table.\n");
        return false;
    }
    if (!db_execute_query(create_chats_table)) {
        fprintf(stderr, "Failed to create chats table.\n");
        return false;
    }
    if (!db_execute_query(create_file_transfers_table)) {
        fprintf(stderr, "Failed to create file_transfers table.\n");
        return false;
    }

    printf("Database tables created/verified successfully.\n");
    return true;
}


long db_add_device(const char* name, const char* mac, bool paired, long last_seen, int signal_strength) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot add device.\n");
        return -1;
    }

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO devices (name, mac, paired, last_seen, signal_strength) VALUES (?, ?, ?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, mac, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, paired ? 1 : 0);
    sqlite3_bind_int64(stmt, 4, last_seen);
    sqlite3_bind_int(stmt, 5, signal_strength);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    long last_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return last_id;
}

bool db_update_device(long id, const char* name, const char* mac, bool paired, long last_seen, int signal_strength) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot update device.\n");
        return false;
    }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE devices SET name = ?, mac = ?, paired = ?, last_seen = ?, signal_strength = ? WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, mac, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, paired ? 1 : 0);
    sqlite3_bind_int64(stmt, 4, last_seen);
    sqlite3_bind_int(stmt, 5, signal_strength);
    sqlite3_bind_int64(stmt, 6, id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool db_delete_device(long id) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot delete device.\n");
        return false;
    }

    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM devices WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

DeviceInfo* db_get_device_by_id(long id) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot get device by ID.\n");
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name, mac, paired, last_seen, signal_strength FROM devices WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, id);

    DeviceInfo *device = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        device = (DeviceInfo*)malloc(sizeof(DeviceInfo));
        if (device) {
            device->id = sqlite3_column_int64(stmt, 0);
            strncpy(device->name, (const char*)sqlite3_column_text(stmt, 1), sizeof(device->name) - 1);
            device->name[sizeof(device->name) - 1] = '\0';
            strncpy(device->mac, (const char*)sqlite3_column_text(stmt, 2), sizeof(device->mac) - 1);
            device->mac[sizeof(device->mac) - 1] = '\0';
            device->paired = sqlite3_column_int(stmt, 3) == 1;
            device->last_seen = sqlite3_column_int64(stmt, 4);
            device->signal_strength = sqlite3_column_int(stmt, 5);
        }
    }

    sqlite3_finalize(stmt);
    return device;
}

DeviceInfo* db_get_device_by_mac(const char* mac) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot get device by MAC.\n");
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name, mac, paired, last_seen, signal_strength FROM devices WHERE mac = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, mac, -1, SQLITE_STATIC);

    DeviceInfo *device = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        device = (DeviceInfo*)malloc(sizeof(DeviceInfo));
        if (device) {
            device->id = sqlite3_column_int64(stmt, 0);
            strncpy(device->name, (const char*)sqlite3_column_text(stmt, 1), sizeof(device->name) - 1);
            device->name[sizeof(device->name) - 1] = '\0';
            strncpy(device->mac, (const char*)sqlite3_column_text(stmt, 2), sizeof(device->mac) - 1);
            device->mac[sizeof(device->mac) - 1] = '\0';
            device->paired = sqlite3_column_int(stmt, 3) == 1;
            device->last_seen = sqlite3_column_int64(stmt, 4);
            device->signal_strength = sqlite3_column_int(stmt, 5);
        }
    }

    sqlite3_finalize(stmt);
    return device;
}

DeviceInfo** db_get_all_devices(int* count) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot get all devices.\n");
        *count = 0;
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, name, mac, paired, last_seen, signal_strength FROM devices;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        *count = 0;
        return NULL;
    }

    DeviceInfo** devices = NULL;
    *count = 0;
    int capacity = 10; // Initial capacity
    devices = (DeviceInfo**)malloc(capacity * sizeof(DeviceInfo*));
    if (!devices) {
        fprintf(stderr, "Memory allocation failed for devices array.\n");
        sqlite3_finalize(stmt);
        return NULL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            devices = (DeviceInfo**)realloc(devices, capacity * sizeof(DeviceInfo*));
            if (!devices) {
                fprintf(stderr, "Memory re-allocation failed for devices array.\n");
                sqlite3_finalize(stmt);
                *count = 0;
                return NULL;
            }
        }

        DeviceInfo *device = (DeviceInfo*)malloc(sizeof(DeviceInfo));
        if (device) {
            device->id = sqlite3_column_int64(stmt, 0);
            strncpy(device->name, (const char*)sqlite3_column_text(stmt, 1), sizeof(device->name) - 1);
            device->name[sizeof(device->name) - 1] = '\0';
            strncpy(device->mac, (const char*)sqlite3_column_text(stmt, 2), sizeof(device->mac) - 1);
            device->mac[sizeof(device->mac) - 1] = '\0';
            device->paired = sqlite3_column_int(stmt, 3) == 1;
            device->last_seen = sqlite3_column_int64(stmt, 4);
            device->signal_strength = sqlite3_column_int(stmt, 5);
            devices[*count] = device;
            (*count)++;
        } else {
            fprintf(stderr, "Memory allocation failed for a device.\n");
            // Clean up already allocated devices
            for (int i = 0; i < *count; ++i) {
                free(devices[i]);
            }
            free(devices);
            sqlite3_finalize(stmt);
            *count = 0;
            return NULL;
        }
    }

    sqlite3_finalize(stmt);
    return devices;
}

void db_free_device_info(DeviceInfo* device) {
    free(device);
}

void db_free_device_info_array(DeviceInfo** devices, int count) {
    for (int i = 0; i < count; ++i) {
        free(devices[i]);
    }
    free(devices);
}

long db_add_chat_message(long device_id, long timestamp, const char* sender, const char* message_type, const char* content) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot add chat message.\n");
        return -1;
    }

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO chats (device_id, timestamp, sender, message_type, content) VALUES (?, ?, ?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, device_id);
    sqlite3_bind_int64(stmt, 2, timestamp);
    sqlite3_bind_text(stmt, 3, sender, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, message_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, content, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    long last_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return last_id;
}

ChatMessage* db_get_chat_message_by_id(long id) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot get chat message by ID.\n");
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, device_id, timestamp, sender, message_type, content FROM chats WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, id);

    ChatMessage *message = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        message = (ChatMessage*)malloc(sizeof(ChatMessage));
        if (message) {
            message->id = sqlite3_column_int64(stmt, 0);
            message->device_id = sqlite3_column_int64(stmt, 1);
            message->timestamp = sqlite3_column_int64(stmt, 2);
            strncpy(message->sender, (const char*)sqlite3_column_text(stmt, 3), sizeof(message->sender) - 1);
            message->sender[sizeof(message->sender) - 1] = '\0';
            strncpy(message->message_type, (const char*)sqlite3_column_text(stmt, 4), sizeof(message->message_type) - 1);
            message->message_type[sizeof(message->message_type) - 1] = '\0';
            strncpy(message->content, (const char*)sqlite3_column_text(stmt, 5), sizeof(message->content) - 1);
            message->content[sizeof(message->content) - 1] = '\0';
        }
    }

    sqlite3_finalize(stmt);
    return message;
}

ChatMessage** db_get_chat_messages_for_device(long device_id, int* count) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot get chat messages for device.\n");
        *count = 0;
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, device_id, timestamp, sender, message_type, content FROM chats WHERE device_id = ? ORDER BY timestamp ASC;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        *count = 0;
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, device_id);

    ChatMessage** messages = NULL;
    *count = 0;
    int capacity = 10; // Initial capacity
    messages = (ChatMessage**)malloc(capacity * sizeof(ChatMessage*));
    if (!messages) {
        fprintf(stderr, "Memory allocation failed for messages array.\n");
        sqlite3_finalize(stmt);
        return NULL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            messages = (ChatMessage**)realloc(messages, capacity * sizeof(ChatMessage*));
            if (!messages) {
                fprintf(stderr, "Memory re-allocation failed for messages array.\n");
                sqlite3_finalize(stmt);
                *count = 0;
                return NULL;
            }
        }

        ChatMessage *message = (ChatMessage*)malloc(sizeof(ChatMessage));
        if (message) {
            message->id = sqlite3_column_int64(stmt, 0);
            message->device_id = sqlite3_column_int64(stmt, 1);
            message->timestamp = sqlite3_column_int64(stmt, 2);
            strncpy(message->sender, (const char*)sqlite3_column_text(stmt, 3), sizeof(message->sender) - 1);
            message->sender[sizeof(message->sender) - 1] = '\0';
            strncpy(message->message_type, (const char*)sqlite3_column_text(stmt, 4), sizeof(message->message_type) - 1);
            message->message_type[sizeof(message->message_type) - 1] = '\0';
            strncpy(message->content, (const char*)sqlite3_column_text(stmt, 5), sizeof(message->content) - 1);
            message->content[sizeof(message->content) - 1] = '\0';
            messages[*count] = message;
            (*count)++;
        } else {
            fprintf(stderr, "Memory allocation failed for a message.\n");
            // Clean up already allocated messages
            for (int i = 0; i < *count; ++i) {
                free(messages[i]);
            }
            free(messages);
            sqlite3_finalize(stmt);
            *count = 0;
            return NULL;
        }
    }

    sqlite3_finalize(stmt);
    return messages;
}

void db_free_chat_message(ChatMessage* message) {
    free(message);
}

void db_free_chat_message_array(ChatMessage** messages, int count) {
    for (int i = 0; i < count; ++i) {
        free(messages[i]);
    }
    free(messages);
}

long db_add_file_transfer(long device_id, const char* filename, long size, double progress, const char* status, long timestamp) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot add file transfer.\n");
        return -1;
    }

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO file_transfers (device_id, filename, size, progress, status, timestamp) VALUES (?, ?, ?, ?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, device_id);
    sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, size);
    sqlite3_bind_double(stmt, 4, progress);
    sqlite3_bind_text(stmt, 5, status, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, timestamp);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    long last_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return last_id;
}

bool db_update_file_transfer_progress(long id, double progress, const char* status) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot update file transfer progress.\n");
        return false;
    }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE file_transfers SET progress = ?, status = ? WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_double(stmt, 1, progress);
    sqlite3_bind_text(stmt, 2, status, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool db_update_file_transfer_status(long id, const char* status) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot update file transfer status.\n");
        return false;
    }

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE file_transfers SET status = ? WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

FileTransfer* db_get_file_transfer_by_id(long id) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot get file transfer by ID.\n");
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, device_id, filename, size, progress, status, timestamp FROM file_transfers WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, id);

    FileTransfer *transfer = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        transfer = (FileTransfer*)malloc(sizeof(FileTransfer));
        if (transfer) {
            transfer->id = sqlite3_column_int64(stmt, 0);
            transfer->device_id = sqlite3_column_int64(stmt, 1);
            strncpy(transfer->filename, (const char*)sqlite3_column_text(stmt, 2), sizeof(transfer->filename) - 1);
            transfer->filename[sizeof(transfer->filename) - 1] = '\0';
            transfer->size = sqlite3_column_int64(stmt, 3);
            transfer->progress = sqlite3_column_double(stmt, 4);
            strncpy(transfer->status, (const char*)sqlite3_column_text(stmt, 5), sizeof(transfer->status) - 1);
            transfer->status[sizeof(transfer->status) - 1] = '\0';
            transfer->timestamp = sqlite3_column_int64(stmt, 6);
        }
    }

    sqlite3_finalize(stmt);
    return transfer;
}

FileTransfer** db_get_file_transfers_for_device(long device_id, int* count) {
    if (!db) {
        fprintf(stderr, "Database not open. Cannot get file transfers for device.\n");
        *count = 0;
        return NULL;
    }

    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, device_id, filename, size, progress, status, timestamp FROM file_transfers WHERE device_id = ? ORDER BY timestamp ASC;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        *count = 0;
        return NULL;
    }

    sqlite3_bind_int64(stmt, 1, device_id);

    FileTransfer** transfers = NULL;
    *count = 0;
    int capacity = 10; // Initial capacity
    transfers = (FileTransfer**)malloc(capacity * sizeof(FileTransfer*));
    if (!transfers) {
        fprintf(stderr, "Memory allocation failed for transfers array.\n");
        sqlite3_finalize(stmt);
        return NULL;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*count >= capacity) {
            capacity *= 2;
            transfers = (FileTransfer**)realloc(transfers, capacity * sizeof(FileTransfer*));
            if (!transfers) {
                fprintf(stderr, "Memory re-allocation failed for transfers array.\n");
                sqlite3_finalize(stmt);
                *count = 0;
                return NULL;
            }
        }

        FileTransfer *transfer = (FileTransfer*)malloc(sizeof(FileTransfer));
        if (transfer) {
            transfer->id = sqlite3_column_int64(stmt, 0);
            transfer->device_id = sqlite3_column_int64(stmt, 1);
            strncpy(transfer->filename, (const char*)sqlite3_column_text(stmt, 2), sizeof(transfer->filename) - 1);
            transfer->filename[sizeof(transfer->filename) - 1] = '\0';
            transfer->size = sqlite3_column_int64(stmt, 3);
            transfer->progress = sqlite3_column_double(stmt, 4);
            strncpy(transfer->status, (const char*)sqlite3_column_text(stmt, 5), sizeof(transfer->status) - 1);
            transfer->status[sizeof(transfer->status) - 1] = '\0';
            transfer->timestamp = sqlite3_column_int64(stmt, 6);
            transfers[*count] = transfer;
            (*count)++;
        } else {
            fprintf(stderr, "Memory allocation failed for a transfer.\n");
            // Clean up already allocated transfers
            for (int i = 0; i < *count; ++i) {
                free(transfers[i]);
            }
            free(transfers);
            sqlite3_finalize(stmt);
            *count = 0;
            return NULL;
        }
    }

    sqlite3_finalize(stmt);
    return transfers;
}

void db_free_file_transfer(FileTransfer* transfer) {
    free(transfer);
}

void db_free_file_transfer_array(FileTransfer** transfers, int count) {
    for (int i = 0; i < count; ++i) {
        free(transfers[i]);
    }
    free(transfers);
}


