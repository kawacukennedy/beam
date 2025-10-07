#pragma once
#include <string>

class FileTransfer {
public:
    FileTransfer();
    ~FileTransfer();

    void send_file(const std::string& path);
    // Add other methods
};