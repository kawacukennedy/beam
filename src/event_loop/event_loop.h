#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

// Function to initialize the event loop
void event_loop_init(void);

// Function to run the event loop (blocks until exit)
void event_loop_run(void);

// Function to stop the event loop
void event_loop_stop(void);

// Function to post an event to the loop (thread-safe)
void event_loop_post_event(void (*callback)(void*), void* data);

#endif // EVENT_LOOP_H
