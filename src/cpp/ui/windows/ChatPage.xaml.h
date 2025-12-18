#pragma once
#include "ChatPage.xaml.g.h"
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::BlueBeam::implementation
{
    struct Message
    {
        hstring Content;
        bool IsSent;
    };

    struct ChatPage : ChatPageT<ChatPage>
    {
        ChatPage();
        Windows::Foundation::Collections::IObservableVector<Message> Messages() { return m_messages; }
        void SendButton_Click(IInspectable const& sender, RoutedEventArgs const& e);

    private:
        Windows::Foundation::Collections::IObservableVector<Message> m_messages;
    };
}

namespace winrt::BlueBeam::factory_implementation
{
    struct ChatPage : ChatPageT<ChatPage, implementation::ChatPage>
    {
    };
}