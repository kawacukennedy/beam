#pragma once

#include <memory>

class UI {
public:
    UI();
    ~UI();

    void run();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};