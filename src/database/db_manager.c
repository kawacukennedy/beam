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
        "CREATE TABLE IF NOT EXISTS devices (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        address TEXT UNIQUE NOT NULL,
        trusted INTEGER DEFAULT 0,
        last_seen INTEGER,
        icon_path TEXT,
        paired BOOLEAN DEFAULT FALSE
        )";

    const char *create_messages_table = 
        "CREATE TABLE IF NOT EXISTS messages (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        device_id INTEGER NOT NULL,
        timestamp INTEGER NOT NULL,
        content TEXT NOT NULL,
        type TEXT NOT NULL,
        status INTEGER NOT NULL,
        encrypted BOOLEAN DEFAULT FALSE,
        read BOOLEAN DEFAULT FALSE,
        FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
        )";

    const char *create_files_table = 
        "CREATE TABLE IF NOT EXISTS files (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        device_id INTEGER NOT NULL,
        filename TEXT NOT NULL,
        path TEXT NOT NULL,
        size INTEGER NOT NULL,
        status INTEGER NOT NULL,
        checksum TEXT,
        encrypted BOOLEAN DEFAULT FALSE,
        chunks_total INTEGER,
        chunks_sent INTEGER,
        last_chunk_timestamp INTEGER,
        FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
        )";

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