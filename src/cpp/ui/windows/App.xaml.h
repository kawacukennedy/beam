#pragma once
#include "App.xaml.g.h"

namespace winrt::BlueBeam::implementation
{
    struct App : AppT<App>
    {
        App();
        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
    };
}

namespace winrt::BlueBeam::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}