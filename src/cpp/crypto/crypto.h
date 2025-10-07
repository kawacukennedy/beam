#pragma once
#include <vector>
#include <memory>

class Crypto {
public:
    Crypto();
    ~Crypto();

    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& data);
    // Add other methods

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};