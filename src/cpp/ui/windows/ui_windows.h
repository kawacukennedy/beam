#pragma once

#include "../ui.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.ApplicationModel.Core.h>

class WindowsUI : public UI {
public:
    WindowsUI();
    ~WindowsUI();
    void run() override;
};