#pragma once
#include <string>

class Messaging {
public:
    Messaging();
    ~Messaging();

    void send_message(const std::string& msg);
    // Add other methods
};