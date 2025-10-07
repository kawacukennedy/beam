// Placeholder for SQLite header
typedef struct sqlite3 sqlite3;

#define SQLITE_OK 0

int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_close(sqlite3 *db);