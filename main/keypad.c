#include "esp_log.h"
#include "keyboard_button.h"
#include "keypad.h"

const char TAG[] = "keypad";

static keypad_cb_fn cb = NULL;

static keyboard_btn_handle_t kbd_handle = NULL;
static const char keymap[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'},
};

static void keyboard_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_report_t kbd_report, void *user_data)
{
    if (kbd_report.key_pressed_num == 0)
    {
        return;
    }

    for (int i = 0; i < kbd_report.key_pressed_num; i++)
    {
        uint8_t row = kbd_report.key_data[i].input_index;
        uint8_t col = kbd_report.key_data[i].output_index;
        char key = keymap[row][col];
        if (cb != NULL)
        {
            cb(key);
        }
    }
}

void register_keypad_cb(keypad_cb_fn fn)
{
    cb = fn;
}

void keypad_init()
{
    keyboard_btn_config_t cfg = {
        .input_gpio_num = 4,
        .input_gpios = (int[]){3, 4, 10, 20},
        .output_gpio_num = 3,
        .output_gpios = (int[]){5, 6, 7},
        .active_level = 1,
        .debounce_ticks = 2,
        .ticks_interval = 200,
        .enable_power_save = false,
    };
    keyboard_button_create(&cfg, &kbd_handle);
    keyboard_btn_cb_config_t cb_cfg = {
        .event = KBD_EVENT_PRESSED,
        .callback = keyboard_cb,
    };
    keyboard_button_register_cb(kbd_handle, cb_cfg, NULL);
}
