#pragma once

#ifdef __APPLE__
#include "ui/macos/ui_macos.h"
#elif defined(_WIN32)
#include "ui/windows/ui_windows.h"
#elif defined(__linux__)
#include "ui/linux/ui_linux.h"
#endif