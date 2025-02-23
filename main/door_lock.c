#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "portmacro.h"
#include <driver/gpio.h>
#include "sdkconfig.h"
#include "door_lock.h"
#include "cmd.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

#define GPIO_OUTPUT_DOOR_BOLT 0
#define GPIO_OUTPUT_ALL (1ULL << GPIO_OUTPUT_DOOR_BOLT)

static const char *TAG = "door_lock";
static TimerHandle_t relock_timer;
static relock_callback_fn relock_cb = NULL;

static int bolt_level = 0;

uint8_t door_cmd_get_relock_duration()
{
    return 5;
}

uint8_t door_cmd_get_lock_state()
{
    ESP_LOGI(TAG, "bolt_level=%d", bolt_level);
    return bolt_level;
}

void door_cmd_unlock()
{
    ESP_LOGI(TAG, "unlock");
    gpio_set_level(GPIO_OUTPUT_DOOR_BOLT, 1);
    bolt_level = 1;
}

void door_cmd_lock()
{
    ESP_LOGI(TAG, "lock");
    gpio_set_level(GPIO_OUTPUT_DOOR_BOLT, 0);
    bolt_level = 0;
}

static void auto_relock_timer_cb(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "end auto relock timer");
    door_cmd_lock();

    if (relock_cb != NULL)
    {
        relock_cb();
    }
}

void door_cmd_begin_relock_timer(relock_callback_fn cb)
{
    uint8_t dur = door_cmd_get_relock_duration();
    ESP_LOGI(TAG, "begin auto relock timer: relock_duration=%d", dur);

    if (relock_timer != NULL)
    {
        if (xTimerIsTimerActive(relock_timer))
        {
            xTimerStop(relock_timer, 0);
        }
        xTimerDelete(relock_timer, 0);
        relock_timer = NULL;
    }

    relock_cb = cb;
    relock_timer = xTimerCreate("relock_timer", pdMS_TO_TICKS(dur * 1000), pdFALSE, (void *)0, auto_relock_timer_cb);
    xTimerStart(relock_timer, 0);
}

void door_lock_init()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_ALL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // door_lock_queue_handle = xQueueCreate(5, sizeof(uint8_t));
    // door_lock_semaphore = xSemaphoreCreateMutex();
    // xTaskCreate(door_lock_task, "door_lock_task", 4096, NULL, 3, NULL);
}