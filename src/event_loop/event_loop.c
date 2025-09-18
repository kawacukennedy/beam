#include "event_loop.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>

static CFRunLoopRef mainRunLoop = NULL;
static pthread_t mainThreadId; // To store the ID of the thread that runs the event loop

void event_loop_init(void) {
    printf("Event Loop initialized (macOS).\n");
    mainThreadId = pthread_self(); // Store the ID of the thread that calls init
}

void event_loop_run(void) {
    printf("Event Loop running (macOS CFRunLoop)...");
    mainRunLoop = CFRunLoopGetCurrent();
    CFRunLoopRun();
    printf("Event Loop stopped (macOS).\n");
}

void event_loop_stop(void) {
    printf("Event Loop stop requested (macOS).\n");
    if (mainRunLoop) {
        CFRunLoopStop(mainRunLoop);
    }
}

void event_loop_post_event(void (*callback)(void*), void* data) {
    if (mainRunLoop) {
        CFRunLoopPerformBlock(mainRunLoop, kCFRunLoopDefaultMode, ^{ // Use kCFRunLoopDefaultMode
            if (callback) {
                callback(data);
            }
        });
        CFRunLoopWakeUp(mainRunLoop);
    } else {
        fprintf(stderr, "Error: macOS Event Loop not running, cannot post event.\n");
    }
}

#elif _WIN32
#include <windows.h>

static bool running = false;
static HWND eventLoopWindow = NULL; // Handle to the hidden window

// Custom message for posting callbacks
#define WM_USER_CALLBACK (WM_USER + 1)

// Structure to hold callback and data
typedef struct {
    void (*callback)(void*);
    void* data;
} CallbackData;

// Window procedure to handle custom messages
LRESULT CALLBACK EventLoopWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_USER_CALLBACK) {
        CallbackData* cbData = (CallbackData*)lParam;
        if (cbData && cbData->callback) {
            cbData->callback(cbData->data);
        }
        free(cbData); // Free the allocated memory
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void event_loop_init(void) {
    printf("Event Loop initialized (Windows).\n");

    // Register a window class for the hidden window
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = EventLoopWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "EventLoopWindowClass";

    if (!RegisterClassEx(&wc)) {
        fprintf(stderr, "Failed to register EventLoopWindowClass: %lu\n", GetLastError());
        return;
    }

    // Create the hidden window
    eventLoopWindow = CreateWindowEx(
        0, "EventLoopWindowClass", "", WS_OVERLAPPEDWINDOW, // Use WS_OVERLAPPEDWINDOW for simplicity, can be 0 for hidden
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        HWND_MESSAGE, // Message-only window
        NULL, GetModuleHandle(NULL), NULL);

    if (!eventLoopWindow) {
        fprintf(stderr, "Failed to create event loop window: %lu\n", GetLastError());
    }
}

void event_loop_run(void) {
    printf("Event Loop running (Windows Message Pump)...");
    running = true;
    MSG msg;
    while (running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    printf("Event Loop stopped (Windows).\n");
}

void event_loop_stop(void) {
    printf("Event Loop stop requested (Windows).\n");
    running = false;
    if (eventLoopWindow) {
        PostMessage(eventLoopWindow, WM_QUIT, 0, 0); // Post WM_QUIT to the hidden window
    }
}

void event_loop_post_event(void (*callback)(void*), void* data) {
    if (eventLoopWindow) {
        CallbackData* cbData = (CallbackData*)malloc(sizeof(CallbackData));
        if (!cbData) {
            fprintf(stderr, "Failed to allocate CallbackData for event.\n");
            return;
        }
        cbData->callback = callback;
        cbData->data = data;
        PostMessage(eventLoopWindow, WM_USER_CALLBACK, 0, (LPARAM)cbData);
    } else {
        fprintf(stderr, "Error: Windows Event Loop window not created, cannot post event.\n");
    }
}


#elif __linux__
#include <glib.h>

static GMainLoop *mainLoop = NULL;

void event_loop_init(void) {
    printf("Event Loop initialized (Linux).\n");
    if (!g_thread_supported()) {
        g_thread_init(NULL);
    }
    // g_type_init(); // Deprecated in GLib 2.36, not needed for newer versions
}

void event_loop_run(void) {
    printf("Event Loop running (Linux GMainLoop)...");
    mainLoop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(mainLoop);
    g_main_loop_unref(mainLoop);
    mainLoop = NULL;
    printf("Event Loop stopped (Linux).\n");
}

void event_loop_stop(void) {
    printf("Event Loop stop requested (Linux).\n");
    if (mainLoop) {
        g_main_loop_quit(mainLoop);
    }
}

// Structure to hold callback and data for GLib idle source
typedef struct {
    void (*callback)(void*);
    void* data;
} GCallbackData;

static gboolean g_idle_callback_wrapper(gpointer user_data) {
    GCallbackData* cbData = (GCallbackData*)user_data;
    if (cbData && cbData->callback) {
        cbData->callback(cbData->data);
    }
    g_free(cbData); // Free the allocated memory
    return G_SOURCE_REMOVE; // Remove this source after execution
}

void event_loop_post_event(void (*callback)(void*), void* data) {
    if (mainLoop) {
        GCallbackData* cbData = g_new(GCallbackData, 1);
        cbData->callback = callback;
        cbData->data = data;
        g_idle_add(g_idle_callback_wrapper, cbData);
    } else {
        fprintf(stderr, "Error: Linux Event Loop not running, cannot post event.\n");
    }
}

#else
// Generic / Fallback implementation
static bool running = false;

void event_loop_init(void) {
    printf("Event Loop initialized (Generic Fallback).\n");
}

void event_loop_run(void) {
    printf("Event Loop running (Generic Fallback - busy loop, not recommended)...");
    running = true;
    while (running) {
        // In a real application, this would be a blocking call or a sleep
        // For now, a simple busy loop.
    }
    printf("Event Loop stopped (Generic Fallback).\n");
}

void event_loop_stop(void) {
    printf("Event Loop stop requested (Generic Fallback).\n");
    running = false;
}

void event_loop_post_event(void (*callback)(void*), void* data) {
    fprintf(stderr, "Warning: Generic Event Loop post_event is a dummy. Not thread-safe.\n");
    if (running && callback) {
        callback(data);
    }
}

#endif