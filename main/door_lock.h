#include <stdint.h>

typedef void (*relock_callback_fn)(void);

void door_lock_init();

void door_cmd_unlock();

void door_cmd_lock();

uint8_t door_cmd_get_lock_state();

void door_cmd_begin_relock_timer();

uint8_t door_cmd_get_relock_duration();
