#include "WelcomePage.xaml.h"
// #include "rust_ffi.h" // Placeholder for FFI

extern "C" void request_bluetooth_permission(); // Placeholder

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::BlueBeam::implementation
{
    WelcomePage::WelcomePage()
    {
        InitializeComponent();
    }

    void WelcomePage::PermissionButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        request_bluetooth_permission();
        // Navigate to discovery
        Frame frame = dynamic_cast<Frame>(this->Parent());
        if (frame)
        {
            frame.Navigate(xaml_typename<DeviceDiscoveryPage>());
        }
    }
}