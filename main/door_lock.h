#include <stdint.h>

// typedef void (*relock_callback_fn)(void);
typedef void (*door_state_cb_fn)(int);

// Door Lock
void door_cmd_unlock();
void door_cmd_lock();
uint8_t door_cmd_get_door_lock_state();
void register_door_lock_state_cb(door_state_cb_fn);

// DPI
uint8_t door_cmd_get_dpi_state();

// Relock
void door_cmd_begin_relock_timer();
uint8_t door_cmd_get_relock_duration();

void door_lock_init();
