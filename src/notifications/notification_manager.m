#include "notification_manager.h"
#include <stdio.h>

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

// Delegate for NSUserNotificationCenter (optional, for handling responses)
@interface NotificationDelegate : NSObject <NSUserNotificationCenterDelegate>
@end

@implementation NotificationDelegate

// Called when a notification is delivered to a user
- (void)userNotificationCenter:(NSUserNotificationCenter *)center didDeliverNotification:(NSUserNotification *)notification {
    NSLog(@"Notification delivered: %@", notification.title);
}

// Called when a user clicks on a notification
- (void)userNotificationCenter:(NSUserNotificationCenter *)center didActivateNotification:(NSUserNotification *)notification {
    NSLog(@"Notification activated: %@", notification.title);
    // Handle user interaction, e.g., open the app, navigate to a specific chat
}

// Called to determine if a notification should be shown even if the app is frontmost
- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center shouldPresentNotification:(NSUserNotification *)notification {
    return YES;
}

@end

static NotificationDelegate *delegate = nil;

void notification_manager_init(void) {
    NSLog(@"Notification Manager initialized (macOS).");
    // Set up a delegate to handle notification interactions
    if (!delegate) {
        delegate = [[NotificationDelegate alloc] init];
    }
    [NSUserNotificationCenter defaultUserNotificationCenter].delegate = delegate;
}

bool notification_manager_show(NotificationType type, const char* title, const char* message, const char* sound_name) {
    @autoreleasepool {
        NSUserNotification *notification = [[NSUserNotification alloc] init];
        notification.title = [NSString stringWithUTF8String:title];
        notification.informativeText = [NSString stringWithUTF8String:message];
        
        if (sound_name && strlen(sound_name) > 0) {
            notification.soundName = [NSString stringWithUTF8String:sound_name];
        } else {
            notification.soundName = NSUserNotificationDefaultSoundName;
        }

        // Deliver the notification
        [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
        [notification release]; // Release the notification object
        return true;
    }
}

#elif _WIN32
#include <windows.h>

void notification_manager_init(void) {
    printf("Notification Manager initialized (Windows - placeholder).\n");
}

bool notification_manager_show(NotificationType type, const char* title, const char* message, const char* sound_name) {
    printf("Showing notification (Windows - placeholder): Type=%d, Title='%s', Message='%s', Sound='%s'\n",
           type, title, message, sound_name ? sound_name : "default");
    // TODO: Implement WinUI Toast notifications. This is complex and requires C++/WinRT.
    // For now, a simple MessageBox can be used for testing, but it's not a toast notification.
    // MessageBox(NULL, message, title, MB_OK | MB_ICONINFORMATION);
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