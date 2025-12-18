#include "FileTransferPage.xaml.h"
// #include "rust_ffi.h"

extern "C" void pause_transfer(); // Placeholder
extern "C" void resume_transfer(); // Placeholder

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::BlueBeam::implementation
{
    FileTransferPage::FileTransferPage()
    {
        InitializeComponent();
    }

    void FileTransferPage::PauseButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        pause_transfer();
    }

    void FileTransferPage::ResumeButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        resume_transfer();
    }
}