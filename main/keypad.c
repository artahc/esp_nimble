#include "string.h"
#include "esp_log.h"
#include "keyboard_button.h"
#include "keypad.h"
#include "door_lock.h"

#define KEYPAD_BUFFER_SIZE 6
#define KEYPAD_BUFFER_TIMER_DURATION pdMS_TO_TICKS(3000)

const char TAG[] = "keypad";

static keyboard_btn_handle_t kbd_handle = NULL;

static const char keymap[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'},
};

static TimerHandle_t long_press_timer[4][3];

static keypad_cb_fn cb = NULL;
static TimerHandle_t keypad_buffer_timer = NULL;
static char keypad_buffer[KEYPAD_BUFFER_SIZE];

static void clear_buffer()
{
    memset(keypad_buffer, 0, KEYPAD_BUFFER_SIZE);
    ESP_LOGI(TAG, "cleared keypad buffer");
}

static void keypad_buffer_timer_cb(TimerHandle_t xTimer)
{
    clear_buffer();
}

static void begin_keypad_buffer_timer()
{
    if (keypad_buffer_timer != NULL)
    {
        xTimerReset(keypad_buffer_timer, 0);
        return;
    }

    keypad_buffer_timer = xTimerCreate("keypad_buffer_timer", KEYPAD_BUFFER_TIMER_DURATION, pdFALSE, (void *)0, keypad_buffer_timer_cb);
    xTimerStart(keypad_buffer_timer, 0);
}

static void keyboard_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_report_t kbd_report, void *user_data)
{

    for (int i = 0; i < kbd_report.key_pressed_num; i++)
    {
        uint8_t row = kbd_report.key_data[i].input_index;
        uint8_t col = kbd_report.key_data[i].output_index;
        TimerHandle_t t = long_press_timer[row][col];

        ESP_LOGI(TAG, "pressed: (row=%d, col=%d) %c", row, col, keymap[row][col]);

        xTimerReset(t, 0);
    }

    for (int i = 0; i < kbd_report.key_release_num; i++)
    {
        uint8_t row = kbd_report.key_release_data[i].input_index;
        uint8_t col = kbd_report.key_release_data[i].output_index;
        char key = keymap[row][col];

        TimerHandle_t t = long_press_timer[row][col];
        TickType_t currentTick = xTaskGetTickCount();
        TickType_t expiryTick = xTimerGetExpiryTime(long_press_timer[row][col]);

        if (xTimerIsTimerActive(t))
        {
            switch (key)
            {
            case '*':
                clear_buffer();
                break;
            case '#':
                door_cmd_unlock();
                break;
            default:
                uint8_t len = strlen(keypad_buffer);
                if (len >= KEYPAD_BUFFER_SIZE)
                {
                    for (int i = 1; i < KEYPAD_BUFFER_SIZE; i++)
                    {
                        keypad_buffer[i - 1] = keypad_buffer[i];
                    }
                    keypad_buffer[KEYPAD_BUFFER_SIZE - 1] = key;
                }
                else
                {
                    keypad_buffer[len] = key;
                }

                begin_keypad_buffer_timer();

                ESP_LOGI(TAG, "keypad buffer[%d]: %s", len, keypad_buffer);

                break;
            }

            xTimerStop(t, 0);
            // ESP_LOGI(TAG, "timer cancelled for key=%c, current tick = %lu, expiry tick = %lu", keymap[row][col], currentTick, expiryTick);
        }
    }
}

static void long_press_timer_cb(TimerHandle_t t)
{
    uint8_t row;
    uint8_t col;

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (t == long_press_timer[i][j])
            {
                row = i;
                col = j;
                break;
            }
        }
    }

    char key = keymap[row][col];
    switch (key)
    {
    case '*':
        break;
    case '#':
        door_cmd_lock();
        break;
    default:
        break;
    }

    ESP_LOGI(TAG, "long pressed (row=%d, col=%d) %c", row, col, keymap[row][col]);
}

void register_keypad_cb(keypad_cb_fn fn)
{
    cb = fn;
}

void keypad_init()
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            long_press_timer[i][j] = xTimerCreate("", pdMS_TO_TICKS(1000), pdFALSE, "", long_press_timer_cb);
        }
    }

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
