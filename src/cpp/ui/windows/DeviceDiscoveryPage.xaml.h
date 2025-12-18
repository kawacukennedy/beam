#pragma once
#include "DeviceDiscoveryPage.xaml.g.h"
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::BlueBeam::implementation
{
    struct Device
    {
        hstring Name;
        hstring Address;
    };

    struct DeviceDiscoveryPage : DeviceDiscoveryPageT<DeviceDiscoveryPage>
    {
        DeviceDiscoveryPage();
        Windows::Foundation::Collections::IObservableVector<Device> Devices() { return m_devices; }
        void RefreshButton_Click(IInspectable const& sender, RoutedEventArgs const& e);
        void PairButton_Click(IInspectable const& sender, RoutedEventArgs const& e);

    private:
        Windows::Foundation::Collections::IObservableVector<Device> m_devices;
    };
}

namespace winrt::BlueBeam::factory_implementation
{
    struct DeviceDiscoveryPage : DeviceDiscoveryPageT<DeviceDiscoveryPage, implementation::DeviceDiscoveryPage>
    {
    };
}