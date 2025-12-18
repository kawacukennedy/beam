#include "MainWindow.xaml.h"
#include "WelcomePage.xaml.h"
#include "DeviceDiscoveryPage.xaml.h"
#include "PairingPage.xaml.h"
#include "ChatPage.xaml.h"
#include "FileTransferPage.xaml.h"
#include "SettingsPage.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::BlueBeam::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        NavView().ItemInvoked({ this, &MainWindow::NavView_ItemInvoked });
        ContentFrame().Navigate(xaml_typename<WelcomePage>());
    }

    void MainWindow::NavView_ItemInvoked(NavigationView const& sender, NavigationViewItemInvokedEventArgs const& args)
    {
        auto tag = unbox_value<hstring>(args.InvokedItemContainer().Tag());
        if (tag == L"welcome")
        {
            ContentFrame().Navigate(xaml_typename<WelcomePage>());
        }
        else if (tag == L"discovery")
        {
            ContentFrame().Navigate(xaml_typename<DeviceDiscoveryPage>());
        }
        else if (tag == L"pairing")
        {
            ContentFrame().Navigate(xaml_typename<PairingPage>());
        }
        else if (tag == L"chat")
        {
            ContentFrame().Navigate(xaml_typename<ChatPage>());
        }
        else if (tag == L"transfer")
        {
            ContentFrame().Navigate(xaml_typename<FileTransferPage>());
        }
        else if (tag == L"settings")
        {
            ContentFrame().Navigate(xaml_typename<SettingsPage>());
        }
    }
}