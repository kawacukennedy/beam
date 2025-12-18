#pragma once
#include "PairingPage.xaml.g.h"

namespace winrt::BlueBeam::implementation
{
    struct PairingPage : PairingPageT<PairingPage>
    {
        PairingPage();
        void ConfirmButton_Click(IInspectable const& sender, RoutedEventArgs const& e);
    };
}

namespace winrt::BlueBeam::factory_implementation
{
    struct PairingPage : PairingPageT<PairingPage, implementation::PairingPage>
    {
    };
}