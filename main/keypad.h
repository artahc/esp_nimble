typedef void (*keypad_cb_fn)(char);

void keypad_init();

void register_keypad_cb(keypad_cb_fn);