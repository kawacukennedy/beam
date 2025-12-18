#pragma once

#include "../ui.h"

class WindowsUI : public UI {
public:
    WindowsUI();
    ~WindowsUI();
    void run() override;
};