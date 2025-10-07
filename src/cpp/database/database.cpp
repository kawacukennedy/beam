#include "database.h"
#include <sqlite3.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

class Database::Impl {
public:
    sqlite3* db;
    std::mutex mtx;

    Impl() : db(nullptr) {
        if (sqlite3_open("bluebeam.db", &db) != SQLITE_OK) {
            std::cerr << "Failed to open database" << std::endl;
        }
        init_tables();
    }

    ~Impl() {
        if (db) sqlite3_close(db);
    }

    void init_tables() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS devices (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                bluetooth_address TEXT UNIQUE NOT NULL,
                trusted BOOLEAN DEFAULT 0,
                last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
                fingerprint TEXT
            );
            CREATE TABLE IF NOT EXISTS messages (
                id TEXT PRIMARY KEY,
                conversation_id TEXT,
                sender_id TEXT,
                receiver_id TEXT,
                content BLOB NOT NULL,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                status TEXT DEFAULT 'sent' CHECK (status IN ('sent', 'delivered', 'read'))
            );
            CREATE TABLE IF NOT EXISTS files (
                id TEXT PRIMARY KEY,
                sender_id TEXT,
                receiver_id TEXT,
                filename TEXT NOT NULL,
                size BIGINT NOT NULL,
                checksum TEXT NOT NULL,
                path TEXT NOT NULL,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                status TEXT DEFAULT 'pending' CHECK (status IN ('pending', 'in_progress', 'complete', 'failed'))
            );
            CREATE INDEX IF NOT EXISTS idx_devices_addr ON devices(bluetooth_address);
            CREATE INDEX IF NOT EXISTS idx_messages_conv ON messages(conversation_id);
        )";

        char* err_msg = nullptr;
        if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
            std::cerr << "SQL error: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
    }

    bool add_device(const Device& device) {
        std::string sql = "INSERT OR REPLACE INTO devices (id, name, bluetooth_address, trusted, last_seen, fingerprint) VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement" << std::endl;
            return false;
        }

        sqlite3_bind_text(stmt, 1, device.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, device.name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, device.address.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, device.trusted ? 1 : 0);
        sqlite3_bind_text(stmt, 5, device.last_seen.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, device.fingerprint.c_str(), -1, SQLITE_TRANSIENT);

        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return success;
    }

    std::vector<Device> get_devices() {
        std::vector<Device> devices;
        std::string sql = "SELECT id, name, bluetooth_address, trusted, last_seen, fingerprint FROM devices ORDER BY last_seen DESC;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare statement" << std::endl;
            return devices;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Device device;
            device.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            device.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            device.address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            device.trusted = sqlite3_column_int(stmt, 3) != 0;
            device.last_seen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            device.fingerprint = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            devices.push_back(device);
        }

        sqlite3_finalize(stmt);
        return devices;
    }

    bool add_message(const Message& message) {
        std::string sql = "INSERT INTO messages (id, conversation_id, sender_id, receiver_id, content, timestamp, status) VALUES (?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, message.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, message.conversation_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, message.sender_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, message.receiver_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 5, message.content.data(), message.content.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, message.timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, message.status.c_str(), -1, SQLITE_TRANSIENT);

        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return success;
    }

    std::vector<Message> get_messages(const std::string& conversation_id) {
        std::vector<Message> messages;
        std::string sql = "SELECT id, conversation_id, sender_id, receiver_id, content, timestamp, status FROM messages WHERE conversation_id = ? ORDER BY timestamp ASC;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return messages;
        }

        sqlite3_bind_text(stmt, 1, conversation_id.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Message message;
            message.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            message.conversation_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            message.sender_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            message.receiver_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const void* blob = sqlite3_column_blob(stmt, 4);
            int size = sqlite3_column_bytes(stmt, 4);
            message.content.assign(static_cast<const uint8_t*>(blob), static_cast<const uint8_t*>(blob) + size);
            message.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            message.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            messages.push_back(message);
        }

        sqlite3_finalize(stmt);
        return messages;
    }

    bool add_file(const File& file) {
        std::string sql = "INSERT INTO files (id, sender_id, receiver_id, filename, size, checksum, path, timestamp, status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, file.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, file.sender_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, file.receiver_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, file.filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 5, file.size);
        sqlite3_bind_text(stmt, 6, file.checksum.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, file.path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, file.timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, file.status.c_str(), -1, SQLITE_TRANSIENT);

        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return success;
    }

    bool update_file_status(const std::string& id, const std::string& status) {
        std::string sql = "UPDATE files SET status = ? WHERE id = ?;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);

        bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return success;
    }

    std::vector<File> get_files() {
        std::vector<File> files;
        std::string sql = "SELECT id, sender_id, receiver_id, filename, size, checksum, path, timestamp, status FROM files ORDER BY timestamp DESC;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return files;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            File file;
            file.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            file.sender_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            file.receiver_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            file.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            file.size = sqlite3_column_int64(stmt, 4);
            file.checksum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            file.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            file.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            file.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            files.push_back(file);
        }

        sqlite3_finalize(stmt);
        return files;
    }
};

Database::Database() : pimpl(std::make_unique<Impl>()) {}
Database::~Database() = default;

bool Database::add_device(const Device& device) {
    std::lock_guard<std::mutex> lock(pimpl->mtx);
    return pimpl->add_device(device);
}

std::vector<Device> Database::get_devices() {
    std::lock_guard<std::mutex> lock(pimpl->mtx);
    return pimpl->get_devices();
}

bool Database::add_message(const Message& message) {
    std::lock_guard<std::mutex> lock(pimpl->mtx);
    return pimpl->add_message(message);
}

std::vector<Message> Database::get_messages(const std::string& conversation_id) {
    std::lock_guard<std::mutex> lock(pimpl->mtx);
    return pimpl->get_messages(conversation_id);
}

bool Database::add_file(const File& file) {
    std::lock_guard<std::mutex> lock(pimpl->mtx);
    return pimpl->add_file(file);
}

bool Database::update_file_status(const std::string& id, const std::string& status) {
    std::lock_guard<std::mutex> lock(pimpl->mtx);
    return pimpl->update_file_status(id, status);
}

std::vector<File> Database::get_files() {
    std::lock_guard<std::mutex> lock(pimpl->mtx);
    return pimpl->get_files();
}