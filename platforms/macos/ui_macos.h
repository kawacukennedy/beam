#pragma once

#include <memory>

class Database;
class Messaging;
class FileTransfer;
class Bluetooth;
class Settings;

class UIMacos {
public:
    UIMacos(Database* db, Messaging* msg, FileTransfer* ft, Bluetooth* bt, Settings* st);
    ~UIMacos();

    void run();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};