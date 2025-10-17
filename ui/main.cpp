#include <iostream>
#include "database/database.h"
#include "crypto/crypto.h"
#include "bluetooth/bluetooth.h"
#include "messaging/messaging.h"
#include "file_transfer/file_transfer.h"
#include "settings/settings.h"
#include "auto_update/auto_update.h"
#include "ui/ui.h"
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "BlueBeam starting..." << std::endl;

    Database db;
    Crypto crypto;
    Bluetooth bluetooth;
    Messaging messaging(crypto);
    FileTransfer file_transfer(crypto);
    Settings settings;
    AutoUpdate auto_update;
    UI ui;

    // Integrate with bluetooth
    bluetooth.set_receive_callback([&messaging, &file_transfer](const std::string& device_id, const std::vector<uint8_t>& data) {
        if (data.size() >= 4 && data[0] == 'F' && data[1] == 'T' && data[2] == 'A' && data[3] == 'P') {
            file_transfer.receive_packet(device_id, data);
        } else {
            messaging.receive_data(device_id, data);
        }
    });

    messaging.set_bluetooth_sender([&bluetooth](const std::string& device_id, const std::vector<uint8_t>& data) {
        return bluetooth.send_data(device_id, data);
    });

    messaging.set_message_callback([&db](const std::string& id, const std::string& conversation_id,
                                      const std::string& sender_id, const std::string& receiver_id,
                                      const std::vector<uint8_t>& content, MessageStatus status) {
        std::cout << "Received message: " << id << " from " << sender_id << std::endl;
        Message msg{id, conversation_id, sender_id, receiver_id, content, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()), 
                    status == MessageStatus::SENT ? "sent" : status == MessageStatus::DELIVERED ? "delivered" : status == MessageStatus::READ ? "read" : "unknown"};
        db.add_message(msg);
    });

    file_transfer.set_data_sender([&bluetooth](const std::string& device_id, const std::vector<uint8_t>& data) {
        return bluetooth.send_data(device_id, data);
    });

    // Start the UI main loop
    ui.run();

    return 0;
}