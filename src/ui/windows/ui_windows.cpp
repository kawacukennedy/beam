#include <windows.h>
#include "ui.h"
#include <iostream>
#include <vector>
#include <string>
#include <commdlg.h>
#include "../database/database.h"
#include "../bluetooth/bluetooth.h"
#include "../crypto/crypto.h"
#include "../messaging/messaging.h"
#include "../file_transfer/file_transfer.h"

static Database* db = nullptr;
static Bluetooth* bt = nullptr;
static Crypto* crypto = nullptr;
static Messaging* messaging = nullptr;
static FileTransfer* ft = nullptr;

static std::string selected_device_id;
static std::string current_conversation_id;
static bool first_run = true;

static HWND main_hwnd;
static HWND status_label;
static HWND scan_button;
static HWND device_list;
static HWND chat_edit;
static HWND message_edit;
static HWND progress_bar;
static HWND progress_window;

void ShowSplashScreen(HWND hwnd) {
    // Create splash window
    HWND splash = CreateWindow("STATIC", "",
                               WS_POPUP | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
                               hwnd, NULL, NULL, NULL);
    // Load and display splash image
    // For now, just show text
    SetWindowText(splash, "BlueBeam");
    ShowWindow(splash, SW_SHOW);
    Sleep(300);
    DestroyWindow(splash);
}

void RequestBluetoothPermissions(HWND hwnd) {
    int result = MessageBox(hwnd, "BlueBeam needs Bluetooth access to communicate with nearby devices.\n\nAllow access?", "Bluetooth Permissions", MB_YESNO);
    if (result == IDYES) {
        ShowProfileSetup(hwnd);
    } else {
        PostQuitMessage(0);
    }
}

void ShowProfileSetup(HWND hwnd) {
    DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_PROFILE_SETUP), hwnd, ProfileSetupProc, 0);
}

INT_PTR CALLBACK ProfileSetupProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char name[256];
                GetWindowText(GetDlgItem(hDlg, IDC_NAME), name, sizeof(name));
                // Save profile
                ShowTutorialOverlay(hDlg);
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

void ShowTutorialOverlay(HWND hwnd) {
    MessageBox(hwnd, "Welcome to BlueBeam!\n\nUse the menu to discover devices and start chatting.", "Tutorial", MB_OK);
    first_run = false;
    CreateMainWindow();
}

void CreateMainWindow() {
    // Create main window
    main_hwnd = CreateWindow("BlueBeamWindow", "BlueBeam",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
                             NULL, NULL, GetModuleHandle(NULL), NULL);
    if (main_hwnd) {
        ShowWindow(main_hwnd, SW_SHOW);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // Initialize backends
            db = new Database();
            bt = new Bluetooth();
            crypto = new Crypto();
            messaging = new Messaging(*crypto);
            ft = new FileTransfer();

            // Set up callbacks
            bt->set_receive_callback([](const std::string& device_id, const std::vector<uint8_t>& data) {
                messaging->receive_data(device_id, data);
                ft->receive_packet(device_id, data);
            });
            messaging->set_bluetooth_sender([](const std::string& device_id, const std::vector<uint8_t>& data) {
                return bt->send_data(device_id, data);
            });
            messaging->set_message_callback([](const std::string& id, const std::string& conversation_id,
                                              const std::string& sender_id, const std::string& receiver_id,
                                              const std::vector<uint8_t>& content, MessageStatus status) {
                OnMessageReceived(id, conversation_id, sender_id, receiver_id, content, status);
            });
            ft->set_data_sender([](const std::string& device_id, const std::vector<uint8_t>& data) {
                return bt->send_data(device_id, data);
            });

            // First time use flow
            if (first_run) {
                ShowSplashScreen(hwnd);
                RequestBluetoothPermissions(hwnd);
            } else {
                CreateMainWindow();
            }

            // Create controls
            status_label = CreateWindow("STATIC", "Welcome to BlueBeam",
                                        WS_VISIBLE | WS_CHILD | SS_LEFT,
                                        20, 20, 760, 30, hwnd, (HMENU)2, NULL, NULL);

            scan_button = CreateWindow("BUTTON", "Scan Devices",
                                      WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                      20, 60, 120, 32, hwnd, (HMENU)1, NULL, NULL);

            device_list = CreateWindow("LISTBOX", "",
                                      WS_VISIBLE | WS_CHILD | WS_VSCROLL | LBS_NOTIFY,
                                      20, 100, 760, 400, hwnd, (HMENU)3, NULL, NULL);

            chat_edit = CreateWindow("EDIT", "",
                                    WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                    20, 100, 760, 400, hwnd, (HMENU)4, NULL, NULL);
            ShowWindow(chat_edit, SW_HIDE);

            message_edit = CreateWindow("EDIT", "",
                                       WS_VISIBLE | WS_CHILD | WS_BORDER,
                                       20, 520, 600, 30, hwnd, (HMENU)5, NULL, NULL);
            ShowWindow(message_edit, SW_HIDE);

            CreateWindow("BUTTON", "Send",
                        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        640, 520, 60, 30, hwnd, (HMENU)6, NULL, NULL);

            CreateWindow("BUTTON", "File",
                        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        720, 520, 60, 30, hwnd, (HMENU)7, NULL, NULL);

            // Progress window
            progress_window = CreateWindow("STATIC", "File Transfer",
                                          WS_OVERLAPPEDWINDOW,
                                          CW_USEDEFAULT, CW_USEDEFAULT, 300, 100,
                                          hwnd, NULL, NULL, NULL);
            progress_bar = CreateWindow(PROGRESS_CLASS, "",
                                       WS_VISIBLE | WS_CHILD,
                                       20, 20, 260, 20, progress_window, NULL, NULL, NULL);
            ShowWindow(progress_window, SW_HIDE);
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 1: // Scan button
                    SetWindowText(status_label, "Scanning for Bluetooth devices...");
                    bt->scan();
                    SendMessage(device_list, LB_RESETCONTENT, 0, 0);
                    SendMessage(device_list, LB_ADDSTRING, 0, (LPARAM)"Device 1 (AA:BB:CC)");
                    SendMessage(device_list, LB_ADDSTRING, 0, (LPARAM)"Device 2 (DD:EE:FF)");
                    SetWindowText(status_label, "Scan complete");
                    break;
                case 3: // Device list
                    if (HIWORD(wParam) == LBN_SELCHANGE) {
                        int index = SendMessage(device_list, LB_GETCURSEL, 0, 0);
                        if (index != LB_ERR) {
                            char buffer[256];
                            SendMessage(device_list, LB_GETTEXT, index, (LPARAM)buffer);
                            selected_device_id = buffer;
                            current_conversation_id = "conv_" + selected_device_id;

                            // Pairing flow
                            if (bt->connect(selected_device_id)) {
                                MessageBox(hwnd, "Device paired successfully!", "Success", MB_OK);
                                // Switch to chat view
                                ShowWindow(status_label, SW_HIDE);
                                ShowWindow(scan_button, SW_HIDE);
                                ShowWindow(device_list, SW_HIDE);
                                ShowWindow(chat_edit, SW_SHOW);
                                ShowWindow(message_edit, SW_SHOW);
                            } else {
                                MessageBox(hwnd, "Pairing failed. Try again.", "Error", MB_OK);
                            }
                        }
                    }
                    break;
                case 6: // Send button
                    {
                        char buffer[1024];
                        GetWindowText(message_edit, buffer, sizeof(buffer));
                        if (strlen(buffer) > 0) {
                            // Encrypt and send
                            std::vector<uint8_t> content(buffer, buffer + strlen(buffer));
                            messaging->send_message("msg_id", current_conversation_id, "sender", selected_device_id, content, MessageStatus::SENT);

                            // Add to chat as sent bubble
                            std::string chat_text = "You: " + std::string(buffer) + "\r\n";
                            int len = GetWindowTextLength(chat_edit);
                            SendMessage(chat_edit, EM_SETSEL, len, len);
                            SendMessage(chat_edit, EM_REPLACESEL, 0, (LPARAM)chat_text.c_str());
                            SetWindowText(message_edit, "");
                        }
                    }
                    break;
                case 7: // File button
                    {
                        OPENFILENAME ofn = {};
                        char szFile[260] = {};
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = hwnd;
                        ofn.lpstrFile = szFile;
                        ofn.nMaxFile = sizeof(szFile);
                        ofn.lpstrFilter = "All Files\0*.*\0";
                        ofn.nFilterIndex = 1;
                        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                        if (GetOpenFileName(&ofn)) {
                            // Preview modal
                            if (MessageBox(hwnd, "Send this file?", "Confirm", MB_YESNO) == IDYES) {
                                ShowWindow(progress_window, SW_SHOW);
                                ft->send_file(szFile, selected_device_id,
                                              [](uint64_t sent, uint64_t total) {
                                                  SendMessage(progress_bar, PBM_SETPOS, (WPARAM)((sent * 100) / total), 0);
                                              },
                                              [](bool success, const std::string& error) {
                                                  ShowWindow(progress_window, SW_HIDE);
                                                  if (success) {
                                                      MessageBox(NULL, "File sent successfully!", "Success", MB_OK);
                                                  } else {
                                                      MessageBox(NULL, ("File transfer failed: " + error).c_str(), "Error", MB_OK);
                                                  }
                                              });
                            }
                        }
                    }
                    break;
            }
            break;

        case WM_DESTROY:
            delete db;
            delete bt;
            delete crypto;
            delete messaging;
            delete ft;
            PostQuitMessage(0);
            return 0;

        case WM_USER + 1: // Custom message for disconnection
            OnDeviceDisconnected((const char*)lParam);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void OnMessageReceived(const std::string& id, const std::string& conversation_id,
                       const std::string& sender_id, const std::string& receiver_id,
                       const std::vector<uint8_t>& content, MessageStatus status) {
    if (conversation_id == current_conversation_id) {
        std::string msg(content.begin(), content.end());
        std::string chat_text = sender_id + ": " + msg + "\r\n";
        int len = GetWindowTextLength(chat_edit);
        SendMessage(chat_edit, EM_SETSEL, len, len);
        SendMessage(chat_edit, EM_REPLACESEL, 0, (LPARAM)chat_text.c_str());
    }
}

void OnDeviceDisconnected(const std::string& device_id) {
    if (device_id == selected_device_id) {
        MessageBox(main_hwnd, "Device disconnected. Attempting to reconnect...", "Disconnected", MB_OK);
        for (int i = 0; i < 3; ++i) {
            if (bt->connect(device_id)) {
                MessageBox(main_hwnd, "Reconnected!", "Success", MB_OK);
                return;
            }
            Sleep(1000);
        }
        MessageBox(main_hwnd, "Failed to reconnect. Device marked offline.", "Error", MB_OK);
    }
}

class UI::Impl {
public:
    HINSTANCE hInstance;
    HWND hwnd;

    Impl() : hInstance(GetModuleHandle(NULL)), hwnd(NULL) {
        // Register window class
        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = "BlueBeamWindow";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);

        // Create window
        hwnd = CreateWindowEx(
            0,
            "BlueBeamWindow",
            "BlueBeam",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
            NULL, NULL, hInstance, NULL
        );

        main_hwnd = hwnd;
        if (hwnd) {
            ShowWindow(hwnd, SW_SHOW);
        }
    }

    ~Impl() {
        if (hwnd) DestroyWindow(hwnd);
    }

    void run() {
        MSG msg = {};
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

UI::UI() : pimpl(std::make_unique<Impl>()) {}
UI::~UI() = default;

void UI::run() {
    pimpl->run();
}