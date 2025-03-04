#include "esp_log.h"
#include "nvs_flash.h"

#include "host/ble_store.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nimble/ble.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "driver/gpio.h"

#include "door_lock.h"
#include "gap.h"
#include "gatt.h"

static const char TAG[] = "esp_ble";

void ble_store_config_init(void);

static void nimble_host_task()
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return;
    }

    ble_store_config_init(); // for storing bonding information

    gap_svc_init();

    gatt_svc_init();

    door_lock_init();

    nimble_port_freertos_init(nimble_host_task);
}
