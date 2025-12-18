#pragma once

#include "../ui.h"

class LinuxUI : public UI {
public:
    LinuxUI();
    ~LinuxUI();
    void run() override;
};