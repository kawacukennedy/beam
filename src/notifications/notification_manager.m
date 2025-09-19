#include "notification_manager.h"
#include <stdio.h>

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

// Delegate for UNUserNotificationCenter
@interface UserNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation UserNotificationDelegate

// Called when a notification is delivered to a foreground app
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler {
    NSLog(@"Notification delivered (foreground): %@", notification.request.content.title);
    completionHandler(UNNotificationPresentationOptionAlert + UNNotificationPresentationOptionSound);
}

// Called when the user interacts with a notification
- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    didReceiveNotificationResponse:(UNNotificationResponse *)response
             withCompletionHandler:(void (^)(void))completionHandler {
    NSLog(@"Notification activated: %@", response.notification.request.content.title);
    // Handle user interaction, e.g., open the app, navigate to a specific chat
    completionHandler();
}

@end

static UserNotificationDelegate *delegate __attribute__((objc_precise_lifetime)) = nil;

void notification_manager_init(void) {
    NSLog(@"Notification Manager initialized (macOS - UserNotifications.framework).");
    // Request authorization for notifications
    [[UNUserNotificationCenter currentNotificationCenter] requestAuthorizationWithOptions:UNAuthorizationOptionAlert + UNAuthorizationOptionSound + UNAuthorizationOptionBadge
                                                                       completionHandler:^(BOOL granted, NSError *_Nullable error) {
        if (granted) {
            NSLog(@"Notification authorization granted.");
        } else {
            NSLog(@"Notification authorization denied: %@", error.localizedDescription);
        }
    }];

    // Set up a delegate to handle notification interactions
    if (!delegate) {
        delegate = [[UserNotificationDelegate alloc] init];
    }
    [UNUserNotificationCenter currentNotificationCenter].delegate = delegate;
}

bool notification_manager_show(NotificationType type, const char* title, const char* message, const char* sound_name) {
    @autoreleasepool {
        UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
        content.title = [NSString stringWithUTF8String:title];
        content.body = [NSString stringWithUTF8String:message];
        
        if (sound_name && strlen(sound_name) > 0) {
            content.sound = [UNNotificationSound soundNamed:[NSString stringWithUTF8String:sound_name]];
        } else {
            content.sound = UNNotificationSound.defaultSound;
        }

        // Create a unique request identifier
        NSString *requestIdentifier = [NSString stringWithFormat:@"BlueBeamNative.notification.%f", [[NSDate date] timeIntervalSince1970]];

        UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:requestIdentifier
                                                                              content:content trigger:nil]; // nil trigger for immediate delivery

        // Schedule the notification
        [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:request withCompletionHandler:^(NSError *_Nullable error) {
            if (error) {
                NSLog(@"Error scheduling notification: %@", error.localizedDescription);
            } else {
                NSLog(@"Notification scheduled successfully.");
            }
        }];
        
        [content release];
        [request release];
        return true;
    }
}

#elif _WIN32
#include <windows.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.Data.Xml.Dom.h>

// For Toast notifications, typically requires a Desktop Bridge or packaging
// This is a simplified placeholder and won't work without proper WinRT setup and app packaging.
// For a full implementation, refer to Microsoft's documentation on WinUI 3 Toast Notifications.

void notification_manager_init(void) {
    printf("Notification Manager initialized (Windows - WinUI Toast placeholder).\n");
    // In a real WinUI 3 app, activation and COM initialization would happen here.
}

bool notification_manager_show(NotificationType type, const char* title, const char* message, const char* sound_name) {
    printf("Showing notification (Windows - WinUI Toast placeholder): Type=%d, Title='%s', Message='%s', Sound='%s'\n",
           type, title, message, sound_name ? sound_name : "default");

    // Fallback to MessageBox for demonstration/testing without full WinRT setup
    MessageBoxA(NULL, message, title, MB_OK | MB_ICONINFORMATION);

    // Example of how WinRT Toast notification might be structured (conceptual)
    /*
    using namespace winrt::Windows::UI::Notifications;
    using namespace winrt::Windows::Data::Xml::Dom;

    ToastNotifier notifier = ToastNotificationManager::CreateToastNotifier();

    XmlDocument toastXml;
    toastXml.LoadXml(L"<toast><visual><binding template=\"ToastGeneric\"><text>" + winrt::to_hstring(title) + L"</text><text>" + winrt::to_hstring(message) + L"</text></binding></visual></toast>");

    ToastNotification toast{toastXml};
    notifier.Show(toast);
    */

    return true;
}


#elif __linux__
#include <libnotify/notify.h>

void notification_manager_init(void) {
    printf("Notification Manager initialized (Linux).\n");
    if (!notify_init("BlueBeamNative")) {
        fprintf(stderr, "Failed to initialize libnotify.\n");
    }
}

bool notification_manager_show(NotificationType type, const char* title, const char* message, const char* sound_name) {
    if (!notify_is_initted()) {
        fprintf(stderr, "libnotify not initialized. Cannot show notification.\n");
        return false;
    }

    NotifyNotification *notification = notify_notification_new(title, message, NULL); // NULL for icon
    if (sound_name && strlen(sound_name) > 0) {
        // libnotify doesn't directly support sound names like macOS/Windows.
        // You might need to play the sound separately or use a custom hint if supported by the daemon.
    }

    notify_notification_set_timeout(notification, 5000); // 5 seconds timeout
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);

    gboolean success = notify_notification_show(notification, NULL); // NULL for error
    g_object_unref(G_OBJECT(notification));

    if (!success) {
        fprintf(stderr, "Failed to show notification.\n");
    }
    return success;
}

#else
// Generic / Fallback implementation
void notification_manager_init(void) {
    printf("Notification Manager initialized (Generic Fallback).\n");
}

bool notification_manager_show(NotificationType type, const char* title, const char* message, const char* sound_name) {
    printf("Showing notification (Generic Fallback): Type=%d, Title='%s', Message='%s', Sound='%s'\n",
           type, title, message, sound_name ? sound_name : "default");
    return true;
}

#endif