#pragma once
#include <memory>

class Database;
class Messaging;
class FileTransfer;
class Bluetooth;
class Settings;

class Database;
class Messaging;
class FileTransfer;
class Bluetooth;
class Settings;

class UILinux {
public:
    UILinux(Database *db, Messaging *messaging, FileTransfer *file_transfer, Bluetooth *bluetooth, Settings *settings);
    ~UILinux();

    void run();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};