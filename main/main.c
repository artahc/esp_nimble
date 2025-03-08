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
#include "host/ble_hs.h"
#include "driver/gpio.h"

#include "door_lock.h"
#include "gap.h"
#include "gatt.h"
#include "keypad.h"

static const char TAG[] = "esp_ble";

void ble_store_config_init(void);

static void nimble_host_task()
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void on_reset_cb(int reason)
{
    printf("Reset callback %d\n", reason);
}

static void on_gatts_register_cb(struct ble_gatt_register_ctxt *ctxt,
                                 void *arg)
{
    char buf[BLE_UUID_STR_LEN];
    switch (ctxt->op)
    {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(TAG, "registering characteristic %s with "
                      "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle,
                 ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGI(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        break;
    }
}

static void on_sync_cb()
{
    adv_init();
}

static void nimble_host_stack_init()
{
    ble_hs_cfg.reset_cb = on_reset_cb;
    ble_hs_cfg.gatts_register_cb = on_gatts_register_cb;
    ble_hs_cfg.sync_cb = on_sync_cb;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;

    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_store_config_init(); // for storing bonding information
}

#define KEYPAD_BUFFER_SIZE 6
static char keypad_buffer[KEYPAD_BUFFER_SIZE];
static void keypad_cb(char c)
{
    uint8_t len = strlen(keypad_buffer);
    switch (c)
    {
    case '*':
        memset(keypad_buffer, 0, KEYPAD_BUFFER_SIZE);
        len = strlen(keypad_buffer);
        break;
    case '#':
        door_cmd_unlock();
        break;
    default:
        if (len >= KEYPAD_BUFFER_SIZE)
        {
            for (int i = 1; i < KEYPAD_BUFFER_SIZE; i++)
            {
                keypad_buffer[i - 1] = keypad_buffer[i];
            }
            keypad_buffer[KEYPAD_BUFFER_SIZE - 1] = c;
        }
        else
        {
            keypad_buffer[len] = c;
        }
        break;
    }

    ESP_LOGI(TAG, "keypad buffer[%d]: %s", len, keypad_buffer);
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

    nimble_host_stack_init();

    gap_svc_init();

    gatt_svc_init();

    door_lock_init();

    keypad_init();

    register_keypad_cb(keypad_cb);

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    nimble_port_freertos_init(nimble_host_task);
}
