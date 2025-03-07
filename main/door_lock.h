#include <stdint.h>

typedef struct
{
    uint8_t door_bolt_state; // 0=locked, 1=unlocked
    uint8_t dpi_state;       // 0=open, 1=closed
} door_state_t;

typedef void (*door_state_cb_fn)(door_state_t);
door_state_t door_cmd_get_door_state();

// Door Lock
void door_cmd_unlock();
void door_cmd_lock();
void register_door_lock_state_cb(door_state_cb_fn);

// Relock
void door_cmd_begin_relock_timer();
uint8_t door_cmd_get_relock_duration();

void door_lock_init();
