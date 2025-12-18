#pragma once
#include "FileTransferPage.xaml.g.h"

namespace winrt::BlueBeam::implementation
{
    struct FileTransferPage : FileTransferPageT<FileTransferPage>
    {
        FileTransferPage();
        void PauseButton_Click(IInspectable const& sender, RoutedEventArgs const& e);
        void ResumeButton_Click(IInspectable const& sender, RoutedEventArgs const& e);
    };
}

namespace winrt::BlueBeam::factory_implementation
{
    struct FileTransferPage : FileTransferPageT<FileTransferPage, implementation::FileTransferPage>
    {
    };
}