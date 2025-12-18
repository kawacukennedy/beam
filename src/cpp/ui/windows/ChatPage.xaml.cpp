#include "ChatPage.xaml.h"
// #include "rust_ffi.h"

extern "C" void send_message(const char* msg); // Placeholder

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::BlueBeam::implementation
{
    ChatPage::ChatPage()
    {
        InitializeComponent();
        m_messages = single_threaded_observable_vector<Message>();
        // Load messages
    }

    void ChatPage::SendButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        auto msg = MessageComposer().Text();
        if (!msg.empty())
        {
            send_message(to_string(msg).c_str());
            m_messages.Append({ msg, true });
            MessageComposer().Text(L"");
        }
    }
}