#pragma once
#include "WelcomePage.xaml.g.h"

namespace winrt::BlueBeam::implementation
{
    struct WelcomePage : WelcomePageT<WelcomePage>
    {
        WelcomePage();
        void PermissionButton_Click(IInspectable const& sender, RoutedEventArgs const& e);
    };
}

namespace winrt::BlueBeam::factory_implementation
{
    struct WelcomePage : WelcomePageT<WelcomePage, implementation::WelcomePage>
    {
    };
}