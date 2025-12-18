#include "PairingPage.xaml.h"
// #include "rust_ffi.h"

extern "C" void confirm_pairing(); // Placeholder

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::BlueBeam::implementation
{
    PairingPage::PairingPage()
    {
        InitializeComponent();
        // Set PIN from FFI
        PinDisplay().Text(L"123456"); // Placeholder
    }

    void PairingPage::ConfirmButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        confirm_pairing();
        // Navigate to chat
        Frame frame = dynamic_cast<Frame>(this->Parent());
        if (frame)
        {
            frame.Navigate(xaml_typename<ChatPage>());
        }
    }

    void PairingPage::SetPin(const std::string& pin) {
        std::wstring wpin(pin.begin(), pin.end());
        PinDisplay().Text(L"PIN: " + wpin);
    }
}