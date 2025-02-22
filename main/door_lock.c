#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "portmacro.h"
#include <driver/gpio.h>
#include "sdkconfig.h"
#include "door_lock.h"

#define GPIO_OUTPUT_DOOR_BOLT 0
#define GPIO_OUTPUT_ALL (1ULL << GPIO_OUTPUT_DOOR_BOLT)

static const char *TAG = "door_lock";

QueueHandle_t door_lock_queue_handle;
static SemaphoreHandle_t door_lock_semaphore;

static void door_lock_task(void *arg)
{
    uint8_t value;
    while (1)
    {
        if (xQueueReceive(door_lock_queue_handle, &value, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(door_lock_semaphore, portMAX_DELAY) == pdTRUE)
            {
                ESP_LOGI(TAG, "Handling queue task, value=%d", value);

                door_cmd_unlock();
                vTaskDelay(1000 / portTICK_PERIOD_MS);

                ESP_LOGI(TAG, "Completed queue task value=%d", value);

                xSemaphoreGive(door_lock_semaphore);
            }
        }
    }
}

uint8_t get_auto_relock_duration()
{
    return 5;
}

void door_cmd_unlock()
{
    gpio_set_level(GPIO_OUTPUT_DOOR_BOLT, 1);
    vTaskDelay((get_auto_relock_duration() * 1000) / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_OUTPUT_DOOR_BOLT, 0);
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

    door_lock_queue_handle = xQueueCreate(5, sizeof(uint8_t));
    door_lock_semaphore = xSemaphoreCreateMutex();

    xTaskCreate(door_lock_task, "door_lock_task", 4096, NULL, 3, NULL);
}