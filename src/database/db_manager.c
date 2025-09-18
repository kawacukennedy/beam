#include "db_manager.h"
#include <stdio.h>
#include <string.h>

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
        "    address TEXT UNIQUE NOT NULL,\n"
        "    trusted INTEGER DEFAULT 0,\n"
        "    last_seen INTEGER,\n"
        "    icon_path TEXT,\n"
        "    paired BOOLEAN DEFAULT FALSE\n"
        ";";

    const char *create_messages_table =
        "CREATE TABLE IF NOT EXISTS messages (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    device_id INTEGER NOT NULL,\n"
        "    timestamp INTEGER NOT NULL,\n"
        "    content TEXT NOT NULL,\n"
        "    type TEXT NOT NULL,\n"
        "    status INTEGER NOT NULL,\n"
        "    encrypted BOOLEAN DEFAULT FALSE,\n"
        "    read BOOLEAN DEFAULT FALSE,\n"
        "    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE\n"
        ";";

    const char *create_files_table =
        "CREATE TABLE IF NOT EXISTS files (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    device_id INTEGER NOT NULL,\n"
        "    filename TEXT NOT NULL,\n"
        "    path TEXT NOT NULL,\n"
        "    size INTEGER NOT NULL,\n"
        "    status INTEGER NOT NULL,\n"
        "    checksum TEXT,\n"
        "    encrypted BOOLEAN DEFAULT FALSE,\n"
        "    chunks_total INTEGER,\n"
        "    chunks_sent INTEGER,\n"
        "    last_chunk_timestamp INTEGER,\n"
        "    FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE\n"
        ";";

    if (!db_execute_query(create_devices_table)) {
        fprintf(stderr, "Failed to create devices table.\n");
        return false;
    }
    if (!db_execute_query(create_messages_table)) {
        fprintf(stderr, "Failed to create messages table.\n");
        return false;
    }
    if (!db_execute_query(create_files_table)) {
        fprintf(stderr, "Failed to create files table.\n");
        return false;
    }

    printf("Database tables created/verified successfully.\n");
    return true;
}
