#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <stdbool.h>
#include <sqlite3.h>

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

// TODO: Add more specific database operation functions (e.g., for devices, messages, files)

#endif // DB_MANAGER_H