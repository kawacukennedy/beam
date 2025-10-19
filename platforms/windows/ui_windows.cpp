#include "ui/ui.h"
#include <windows.h>
#include <commdlg.h>
#include <iostream>
#include <memory>
#include <chrono>
#include <string>
#include "database.h"
#include "crypto.h"
#include "bluetooth.h"
#include "messaging.h"
#include "file_transfer.h"
#include "settings.h"

class UI::Impl {
public:
    std::unique_ptr<Database> db;
    std::unique_ptr<Bluetooth> bt;
    std::unique_ptr<Crypto> crypto;
    std::unique_ptr<Messaging> messaging;
    std::unique_ptr<FileTransfer> ft;
    std::unique_ptr<Settings> settings;
    std::string current_device_id;

    Impl() {
        // Onboarding
        showOnboarding();

        db = std::make_unique<Database>();
        bt = std::make_unique<Bluetooth>();
        crypto = std::make_unique<Crypto>();
        messaging = std::make_unique<Messaging>(*crypto);
        ft = std::make_unique<FileTransfer>(*crypto);
        settings = std::make_unique<Settings>();
        WNDCLASS wc = {0};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "BlueBeam";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);

        hwnd = CreateWindow("BlueBeam", "BlueBeam", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, wc.hInstance, this);

        // Create basic UI elements
        CreateWindow("BUTTON", "Scan Devices", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, 100, 30, hwnd, (HMENU)1, wc.hInstance, NULL);
        CreateWindow("BUTTON", "Connect", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 120, 10, 100, 30, hwnd, (HMENU)2, wc.hInstance, NULL);
        CreateWindow("BUTTON", "Settings", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 230, 10, 80, 30, hwnd, (HMENU)7, wc.hInstance, NULL);
        device_list = CreateWindow("LISTBOX", "", WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOTIFY, 10, 50, 200, 400, hwnd, (HMENU)7, wc.hInstance, NULL);
        chat_view = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY, 220, 50, 560, 400, hwnd, (HMENU)3, wc.hInstance, NULL);
        message_entry = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 220, 460, 400, 30, hwnd, (HMENU)4, wc.hInstance, NULL);
        CreateWindow("BUTTON", "Send", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 630, 460, 70, 30, hwnd, (HMENU)5, wc.hInstance, NULL);
        CreateWindow("BUTTON", "Send File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 710, 460, 70, 30, hwnd, (HMENU)6, wc.hInstance, NULL);

        progress_bar = CreateWindow(PROGRESS_CLASS, "", WS_VISIBLE | WS_CHILD, 10, 500, 760, 20, hwnd, (HMENU)8, wc.hInstance, NULL);

        // Set up callbacks
        bt->set_receive_callback([this](const std::string& device_id, const std::vector<uint8_t>& data) {
            messaging->receive_data(device_id, data);
            ft->receive_packet(device_id, data);
        });
        bt->set_disconnect_callback([this](const std::string& device_id) {
            if (device_id == current_device_id) {
                current_device_id.clear();
                MessageBox(hwnd, "Device disconnected", "BlueBeam", MB_OK | MB_ICONWARNING);
            }
        });
        messaging->set_bluetooth_sender([this](const std::string& device_id, const std::vector<uint8_t>& data) {
            return bt->send_data(device_id, data);
        });
        messaging->set_message_callback([this](const std::string& id, const std::string& conversation_id,
                                              const std::string& sender_id, const std::string& receiver_id,
                                              const std::vector<uint8_t>& content, MessageStatus status) {
            if (conversation_id == current_device_id) {
                std::string msg(content.begin(), content.end());
                std::string display = sender_id + ": " + msg + "\r\n";
                char chat_buffer[4096];
                GetWindowText(chat_view, chat_buffer, sizeof(chat_buffer));
                std::string chat = chat_buffer;
                chat += display;
                SetWindowText(chat_view, chat.c_str());

                // Save to database
                Message db_msg{id, conversation_id, sender_id, receiver_id, content, std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()), status == MessageStatus::SENT ? "sent" : "received"};
                db->add_message(db_msg);
            }
        });
        ft->set_data_sender([this](const std::string& device_id, const std::vector<uint8_t>& data) {
            return bt->send_data(device_id, data);
        });
        ft->set_incoming_file_callback([this](const std::string& filename, uint64_t size, auto response) {
            // Simple accept for demo
            response(true, "/tmp/" + filename);
        });
    }

    void showOnboarding() {
        // Simple message box for onboarding
        MessageBox(NULL, "Welcome to BlueBeam!\n\n1. Click Scan Devices to find nearby Bluetooth devices.\n2. Select a device and click Connect.\n3. Start chatting or sending files.\n\nEnjoy secure Bluetooth communication!", "BlueBeam", MB_OK | MB_ICONINFORMATION);
    }

    void showSettingsDialog() {
        if (settings_hwnd) {
            SetFocus(settings_hwnd);
            return;
        }

        // Register settings window class
        WNDCLASS wc = {0};
        wc.lpfnWndProc = SettingsProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "BlueBeamSettings";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);

        // Create settings dialog
        settings_hwnd = CreateWindow("BlueBeamSettings", "Settings", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, hwnd, NULL, GetModuleHandle(NULL), NULL);

        CreateWindow("STATIC", "User Name:", WS_VISIBLE | WS_CHILD, 20, 20, 80, 20, settings_hwnd, NULL, GetModuleHandle(NULL), NULL);
        name_edit = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 110, 20, 200, 20, settings_hwnd, (HMENU)101, GetModuleHandle(NULL), NULL);

        CreateWindow("BUTTON", "Save", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 150, 60, 80, 30, settings_hwnd, (HMENU)102, GetModuleHandle(NULL), NULL);
        CreateWindow("BUTTON", "Cancel", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 250, 60, 80, 30, settings_hwnd, (HMENU)103, GetModuleHandle(NULL), NULL);

        // Load current settings
        std::string name = settings->get_user_name();
        SetWindowText(name_edit, name.c_str());

        ShowWindow(settings_hwnd, SW_SHOW);
    }

    void loadMessageHistory() {
        if (current_device_id.empty()) return;

        auto messages = db->get_messages(current_device_id);
        std::string chat_text;
        for (const auto& msg : messages) {
            std::string display = msg.sender_id + ": " + std::string(msg.content.begin(), msg.content.end()) + "\r\n";
            chat_text += display;
        }
        SetWindowText(chat_view, chat_text.c_str());
    }

    void run() {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

private:
    static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        UI::Impl* impl = reinterpret_cast<UI::Impl*>(GetWindowLongPtr(GetParent(hwnd), GWLP_USERDATA));
        switch (msg) {
            case WM_COMMAND:
                if (impl) {
                    switch (LOWORD(wParam)) {
                        case 102: // Save
                        {
                            char buffer[256];
                            GetWindowText(impl->name_edit, buffer, sizeof(buffer));
                            impl->settings->set_user_name(buffer);
                            impl->settings->save();
                            DestroyWindow(hwnd);
                        }
                        break;
                        case 103: // Cancel
                            DestroyWindow(hwnd);
                            break;
                    }
                }
                break;
            case WM_DESTROY:
                impl->settings_hwnd = NULL;
                break;
            default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        return 0;
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        UI::Impl* impl = reinterpret_cast<UI::Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        switch (msg) {
            case WM_CREATE:
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT*)lParam)->lpCreateParams);
                break;
            case WM_COMMAND:
                if (impl) {
                    switch (LOWORD(wParam)) {
                    case 1: // Scan
                        impl->bt->scan();
                        SendMessage(impl->device_list, LB_RESETCONTENT, 0, 0);
                        auto devices = impl->bt->get_discovered_devices();
                        for (const auto& dev : devices) {
                            SendMessage(impl->device_list, LB_ADDSTRING, 0, (LPARAM)dev.c_str());
                        }
                        break;
                    case 2: // Connect
                        {
                            int sel = SendMessage(impl->device_list, LB_GETCURSEL, 0, 0);
                            if (sel != LB_ERR) {
                                char buffer[256];
                                SendMessage(impl->device_list, LB_GETTEXT, sel, (LPARAM)buffer);
                                std::string name = buffer;
                                std::string device_id = impl->bt->get_device_id_from_name(name);
                        if (impl->bt->connect(device_id)) {
                            impl->current_device_id = device_id;
                            impl->loadMessageHistory();
                            // Update status
                        }
                            }
                        }
                        break;
                    case 5: // Send
                        {
                            char buffer[1024];
                            GetWindowText(impl->message_entry, buffer, sizeof(buffer));
                            if (strlen(buffer) > 0 && !impl->current_device_id.empty()) {
                                std::string msg = buffer;
                                std::vector<uint8_t> content(msg.begin(), msg.end());
                                std::string conversation_id = impl->current_device_id; // Simple
                                std::string id = "msg_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                                impl->messaging->send_message(id, conversation_id, "self", impl->current_device_id, content, MessageStatus::SENT);
                                SetWindowText(impl->message_entry, "");
                                // Append to chat
                                char chat_buffer[4096];
                                GetWindowText(impl->chat_view, chat_buffer, sizeof(chat_buffer));
                                std::string chat = chat_buffer;
                                chat += "You: " + msg + "\r\n";
                                SetWindowText(impl->chat_view, chat.c_str());
                            }
                        }
                        break;
                        case 6: // Send File
                        {
                            OPENFILENAME ofn;
                            char szFile[260];
                            ZeroMemory(&ofn, sizeof(ofn));
                            ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = impl->hwnd;
                            ofn.lpstrFile = szFile;
                            ofn.lpstrFile[0] = '\0';
                            ofn.nMaxFile = sizeof(szFile);
                            ofn.lpstrFilter = "All Files\0*.*\0";
                            ofn.nFilterIndex = 1;
                            ofn.lpstrFileTitle = NULL;
                            ofn.nMaxFileTitle = 0;
                            ofn.lpstrInitialDir = NULL;
                            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                            if (GetOpenFileName(&ofn) == TRUE) {
                                std::string path = szFile;
                                if (!impl->current_device_id.empty()) {
                                    SendMessage(impl->progress_bar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
                                    SendMessage(impl->progress_bar, PBM_SETPOS, 0, 0);
                                    impl->ft->send_file(path, impl->current_device_id,
                                        [impl](uint64_t sent, uint64_t total) {
                                            int percent = total > 0 ? (sent * 100) / total : 0;
                                            SendMessage(impl->progress_bar, PBM_SETPOS, percent, 0);
                                        },
                                        [impl](bool success, const std::string& error) {
                                            SendMessage(impl->progress_bar, PBM_SETPOS, 100, 0);
                                            if (success) {
                                                MessageBox(impl->hwnd, "File sent successfully!", "BlueBeam", MB_OK | MB_ICONINFORMATION);
                                            } else {
                                                MessageBox(impl->hwnd, ("File send failed: " + error).c_str(), "BlueBeam", MB_OK | MB_ICONERROR);
                                            }
                                        });
                                }
                            }
                        }
                        break;
                        case 7: // Settings
                            impl->showSettingsDialog();
                            break;
                    }
                }
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        return 0;
    }

    HWND hwnd;
    HWND device_list;
    HWND chat_view;
    HWND message_entry;
    HWND settings_hwnd;
    HWND name_edit;
    HWND progress_bar;
};

UI::UI() : pimpl(std::make_unique<Impl>()) {}
UI::~UI() = default;

void UI::run() {
    pimpl->run();
}