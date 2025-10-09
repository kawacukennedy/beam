#include "ui/ui.h"
#include <windows.h>
#include <iostream>

class UI::Impl {
public:
    Impl() {
        WNDCLASS wc = {0};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "BlueBeam";
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClass(&wc);

        hwnd = CreateWindow("BlueBeam", "BlueBeam", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, wc.hInstance, NULL);

        // Create basic UI elements
        CreateWindow("BUTTON", "Scan Devices", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, 100, 30, hwnd, (HMENU)1, wc.hInstance, NULL);
        CreateWindow("BUTTON", "Connect", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 120, 10, 100, 30, hwnd, (HMENU)2, wc.hInstance, NULL);
        CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE, 10, 50, 760, 400, hwnd, (HMENU)3, wc.hInstance, NULL);
        CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 10, 460, 600, 30, hwnd, (HMENU)4, wc.hInstance, NULL);
        CreateWindow("BUTTON", "Send", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 620, 460, 100, 30, hwnd, (HMENU)5, wc.hInstance, NULL);
        CreateWindow("BUTTON", "Send File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 730, 460, 100, 30, hwnd, (HMENU)6, wc.hInstance, NULL);
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
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case 1: // Scan
                        std::cout << "Scan devices" << std::endl;
                        break;
                    case 2: // Connect
                        std::cout << "Connect" << std::endl;
                        break;
                    case 5: // Send
                        std::cout << "Send message" << std::endl;
                        break;
                    case 6: // Send File
                        std::cout << "Send file" << std::endl;
                        break;
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
};

UI::UI() : pimpl(std::make_unique<Impl>()) {}
UI::~UI() = default;

void UI::run() {
    pimpl->run();
}