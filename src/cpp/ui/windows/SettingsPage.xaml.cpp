#include "SettingsPage.xaml.h"
// #include "rust_ffi.h"

extern "C" void set_dark_mode(bool enabled); // Placeholder
extern "C" void set_notifications(bool enabled); // Placeholder

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::BlueBeam::implementation
{
    SettingsPage::SettingsPage()
    {
        InitializeComponent();
        DarkModeToggle().Toggled([](auto&&, auto&&) { /* handle */ });
        NotificationsToggle().Toggled([](auto&&, auto&&) { /* handle */ });
    }

    void SettingsPage::SecurityButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        // Open security dialog or something
    }
}