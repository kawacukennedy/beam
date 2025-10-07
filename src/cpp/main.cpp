#include <iostream>
#include "database/database.h"
#include "crypto/crypto.h"
#include "bluetooth/bluetooth.h"
#include "ui/ui.h"

int main(int argc, char* argv[]) {
    std::cout << "BlueBeam starting..." << std::endl;

    Database db;
    Crypto crypto;
    Bluetooth bluetooth;
    UI ui;

    ui.run();

    return 0;
}