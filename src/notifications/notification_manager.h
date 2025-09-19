#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Enum for notification types
typedef enum {
    NOTIFICATION_TYPE_MESSAGE,
    NOTIFICATION_TYPE_FILE_TRANSFER_COMPLETE,
    NOTIFICATION_TYPE_FILE_TRANSFER_FAILED,
    NOTIFICATION_TYPE_ERROR,
    // ... other types
} NotificationType;

// Function to initialize the notification manager
void notification_manager_init(void);

// Function to show a notification
bool notification_manager_show(NotificationType type, const char* title, const char* message, const char* sound_name);

#ifdef __cplusplus
}
#endif

#endif // NOTIFICATION_MANAGER_H
