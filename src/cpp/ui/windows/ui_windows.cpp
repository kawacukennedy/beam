#include "ui_windows.h"
#include "App.xaml.h"

WindowsUI::WindowsUI() {}

WindowsUI::~WindowsUI() {}

void WindowsUI::run() {
    winrt::init_apartment();
    auto app = winrt::make<winrt::BlueBeam::implementation::App>();
    winrt::Windows::ApplicationModel::Core::CoreApplication::Run(app);
}