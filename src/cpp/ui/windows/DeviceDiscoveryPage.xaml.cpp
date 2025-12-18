#include "DeviceDiscoveryPage.xaml.h"
// #include "rust_ffi.h"

extern "C" void start_device_discovery(); // Placeholder
extern "C" void get_discovered_devices(Device* devices, int* count); // Placeholder

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::BlueBeam::implementation
{
    DeviceDiscoveryPage::DeviceDiscoveryPage()
    {
        InitializeComponent();
        m_devices = single_threaded_observable_vector<Device>();
        start_device_discovery();
        // Load devices
        Device devs[10];
        int count = 0;
        get_discovered_devices(devs, &count);
        for (int i = 0; i < count; ++i)
        {
            m_devices.Append(devs[i]);
        }
    }

    void DeviceDiscoveryPage::RefreshButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        m_devices.Clear();
        start_device_discovery();
        // Reload
    }

    void DeviceDiscoveryPage::PairButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        // Navigate to pairing
        Frame frame = dynamic_cast<Frame>(this->Parent());
        if (frame)
        {
            frame.Navigate(xaml_typename<PairingPage>());
        }
    }
}