#pragma once
#include "SettingsPage.xaml.h.g"

namespace winrt::BlueBeam::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage();
        void LoadSettings();
        void SaveButton_Click(IInspectable const& sender, RoutedEventArgs const& e);
    };
}

namespace winrt::BlueBeam::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}