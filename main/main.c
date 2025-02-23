#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/ble_store.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "door_lock.c"
#include "freertos/timers.h"

#define TAG "esp_ble"
#define GATT_SVR_SVC_ALERT_UUID 0x1811

static uint8_t own_addr_type;

static uint16_t battery_level_chr_handle;
static uint16_t lock_state_chr_handle;
static uint16_t lock_cmd_chr_handle;
// String: 6f498281b4dd498864456e68c52339ba
static const ble_uuid128_t lock_svc_uuid = BLE_UUID128_INIT(0x64, 0x45, 0x6E, 0x68, 0xC5, 0x23, 0x39, 0xBA, 0x88, 0x49, 0xDD, 0xB4, 0x81, 0x82, 0x49, 0x6F);
/// String: b43f27bf563f08a4cf4e3fb95b0024c0
static const ble_uuid128_t lock_state_chr_uuid = BLE_UUID128_INIT(0xCF, 0x4E, 0x3F, 0xB9, 0x5B, 0x00, 0x24, 0xC0, 0xA4, 0x08, 0x3F, 0x56, 0xBF, 0x27, 0x3F, 0xB4);
// String: e089c541e6045fabb9433d8947b3ded5
static const ble_uuid128_t lock_cmd_chr_uuid = BLE_UUID128_INIT(0xB9, 0x43, 0x3D, 0x89, 0x47, 0xB3, 0xDE, 0xD5, 0xAB, 0x5F, 0x04, 0xE6, 0x41, 0xC5, 0x89, 0xE0);

static const ble_uuid16_t battery_svc_uuid = BLE_UUID16_INIT(0x180F);
static const ble_uuid16_t battery_level_chr_uuid = BLE_UUID16_INIT(0x2A19);

void ble_store_config_init(void);

static int gap_event_cb(struct ble_gap_event *event, void *arg);

void ble_advertise()
{
    int rc;
    struct ble_hs_adv_fields fields = {0};
    const char *name;

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    // fields.uuids16 = (ble_uuid16_t[]){
    //     BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)};
    // fields.num_uuids16 = 1;
    // fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%02x", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_cb, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to start ble advertisement: %02x", rc);
        return;
    }
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ble_gap_adv_stop();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ble_advertise();
        break;
    default:
        break;
    }

    return 0;
}

static void notify_lock_state_update(void)
{
    ble_gatts_chr_updated(lock_state_chr_handle);
}

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        ESP_LOGI(TAG, "received read characteristic command to conn_handle=%d, attr_handle=%d", conn_handle, attr_handle);
        if (attr_handle == lock_state_chr_handle)
        {
            int lock_state = door_cmd_get_lock_state();
            int rc = os_mbuf_append(ctxt->om, &lock_state, 1);
            if (rc != 0)
            {
                ESP_LOGE(TAG, "error read %d", rc);
            }
        }

        break;
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        ESP_LOGI(TAG, "received write characteristic command to conn_handle=%d, attr_handle=%d", conn_handle, attr_handle);
        ESP_LOGI(TAG, "buffer length: %d, value=", OS_MBUF_PKTLEN(ctxt->om));
        ESP_LOG_BUFFER_HEX(TAG, ctxt->om->om_data, OS_MBUF_PKTLEN(ctxt->om));

        if (attr_handle == lock_cmd_chr_handle)
        {
            door_cmd_unlock();
            notify_lock_state_update();
            door_cmd_begin_relock_timer(notify_lock_state_update);
        }

        break;
    case BLE_GATT_ACCESS_OP_READ_DSC:
        break;
    case BLE_GATT_ACCESS_OP_WRITE_DSC:
        break;

    default:
        break;
    }

    return 0;
}

static void gatt_reset_cb(int reason)
{
    printf("Reset callback %d\n", reason);
}

static void gatt_register_cb(struct ble_gatt_register_ctxt *ctxt,
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

static void gatt_sync_cb()
{
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to infer address, %02x", rc);
        return;
    }

    uint8_t addr_val[6] = {0};
    ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    ESP_LOGI(TAG, "device address: %02x:%02x:%02x:%02x:%02x:%02x",
             addr_val[5], addr_val[4], addr_val[3], addr_val[2], addr_val[1], addr_val[0]);

    ble_advertise();
}

void nimble_host_task()
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static const struct ble_gatt_svc_def ble_gatt_svcs[] = {
    // Door Lock Service
    {
        .uuid = &lock_svc_uuid.u,
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .characteristics = (struct ble_gatt_chr_def[]){
            // Lock State Characteristic
            {
                .uuid = &lock_state_chr_uuid.u,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &lock_state_chr_handle,
                .access_cb = gatt_access_cb,
                .descriptors = (struct ble_gatt_dsc_def[]){
                    {
                        .access_cb = gatt_access_cb,
                        .att_flags = BLE_GATT_ACCESS_OP_READ_DSC,
                    },

                    {0},
                },
            },

            // Lock Command Characteristic
            {
                .uuid = &lock_cmd_chr_uuid.u,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &lock_cmd_chr_handle,
                .access_cb = gatt_access_cb,
                .descriptors = (struct ble_gatt_dsc_def[]){
                    {0},
                },
            },

            {0},
        },
    },

    // Battery Service
    {
        .uuid = &battery_svc_uuid.u,
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .characteristics = (struct ble_gatt_chr_def[]){
            // Battery Level Characteristic
            {
                .uuid = &battery_level_chr_uuid.u,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &battery_level_chr_handle,
                .access_cb = gatt_access_cb,
                .descriptors = (struct ble_gatt_dsc_def[]){
                    {0},
                },
            },

            {0},
        },
    },

    {0},
};

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

    ble_hs_cfg.reset_cb = gatt_reset_cb;
    ble_hs_cfg.gatts_register_cb = gatt_register_cb;
    ble_hs_cfg.sync_cb = gatt_sync_cb;
    ble_hs_cfg.sm_io_cap = 3;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc;
    rc = ble_gatts_count_cfg(ble_gatt_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to configure ble: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(ble_gatt_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to register ble services: %d", rc);
        return;
    }

    rc = ble_svc_gap_device_name_set("esp32c3");
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to configure ble: %d", rc);
        return;
    }

    ble_store_config_init();

    door_lock_init();

    nimble_port_freertos_init(nimble_host_task);
}
