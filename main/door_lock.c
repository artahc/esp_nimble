#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "portmacro.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "door_lock.h"

#define GPIO_OUTPUT_DOOR_BOLT 0
#define GPIO_INPUT_DPI 1
#define GPIO_OUTPUT_ALL (1ULL << GPIO_OUTPUT_DOOR_BOLT)
#define GPIO_INPUT_ALL (1ULL << GPIO_INPUT_DPI)

static const char *TAG = "door_lock";
static TimerHandle_t relock_timer;
static QueueHandle_t dpi_event_queue;

static door_state_t door_state = {0};
static door_state_cb_fn door_state_cb = NULL;

static void notify_door_state_cb();

// Door Lock
door_state_t door_cmd_get_door_state()
{
    ESP_LOGI(TAG, "door_bolt_state=%d, dpi_state=%d", door_state.door_bolt_state, door_state.dpi_state);
    return door_state;
}

void door_cmd_unlock()
{
    ESP_LOGI(TAG, "unlock");
    gpio_set_level(GPIO_OUTPUT_DOOR_BOLT, 1);
    door_state.door_bolt_state = 1;
    notify_door_state_cb();
}

void door_cmd_lock()
{
    ESP_LOGI(TAG, "lock");
    gpio_set_level(GPIO_OUTPUT_DOOR_BOLT, 0);
    door_state.door_bolt_state = 0;
    notify_door_state_cb();
}

void register_door_lock_state_cb(door_state_cb_fn cb)
{
    door_state_cb = cb;
}

static void notify_door_state_cb()
{
    ESP_LOGI(TAG, "door_state: door_bolt_state=%d, dpi_state=%d", door_state.door_bolt_state, door_state.dpi_state);
    if (door_state_cb != NULL)
    {
        door_state_cb(door_state);
    }
}

// Relock
uint8_t door_cmd_get_relock_duration()
{
    return 0;
}

static void relock_timer_cb(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "end relock timer");
    door_cmd_lock();
}

void door_cmd_begin_relock_timer()
{
    uint8_t dur = door_cmd_get_relock_duration();
    if (dur == 0)
    {
        door_cmd_lock();
        return;
    }

    ESP_LOGI(TAG, "begin relock timer: relock_duration=%d", dur);

    if (relock_timer != NULL)
    {
        if (xTimerIsTimerActive(relock_timer))
        {
            xTimerStop(relock_timer, 0);
        }
        xTimerDelete(relock_timer, 0);
        relock_timer = NULL;
    }

    relock_timer = xTimerCreate("relock_timer", pdMS_TO_TICKS(dur * 1000), pdFALSE, (void *)0, relock_timer_cb);
    xTimerStart(relock_timer, 0);
}

// DPI
static void IRAM_ATTR dpi_isr_handler(void *arg)
{
    uint8_t gpio_num = (uint8_t)arg;
    xQueueSendFromISR(dpi_event_queue, &gpio_num, NULL);
}

static void dpi_event_handler(void *arg)
{
    uint8_t gpio;
    while (true)
    {
        if (xQueueReceive(dpi_event_queue, &gpio, portMAX_DELAY))
        {
            door_state.dpi_state = gpio_get_level(gpio);
            notify_door_state_cb();
        }

        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

// Initialization
void door_lock_init()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_ALL;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_INPUT_ALL;
    io_conf.pull_down_en = 1;
    gpio_config(&io_conf);

    door_state.door_bolt_state = gpio_get_level(GPIO_OUTPUT_DOOR_BOLT);
    door_state.dpi_state = gpio_get_level(GPIO_INPUT_DPI);

    dpi_event_queue = xQueueCreate(10, sizeof(uint8_t));
    xTaskCreate(dpi_event_handler, "dpi_event_handler", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_INPUT_DPI, dpi_isr_handler, (void *)GPIO_INPUT_DPI);
}