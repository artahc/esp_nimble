#include "esp_log.h"
#include "host/ble_hs.h"
#include "services/gatt/ble_svc_gatt.h"
#include "host/ble_uuid.h"

#include "door_lock.h"
#include "gap.h"

static const char TAG[] = "gatt";

static uint16_t battery_level_chr_handle;
static uint16_t lock_state_chr_handle;
static uint16_t lock_cmd_chr_handle;

// Door Lock GATT Service
// String: 6f498281b4dd498864456e68c52339ba
static const ble_uuid128_t lock_svc_uuid = BLE_UUID128_INIT(0x64, 0x45, 0x6E, 0x68, 0xC5, 0x23, 0x39, 0xBA, 0x88, 0x49, 0xDD, 0xB4, 0x81, 0x82, 0x49, 0x6F);

// Lock State Characteristic
// String: b43f27bf563f08a4cf4e3fb95b0024c0
static const ble_uuid128_t lock_state_chr_uuid = BLE_UUID128_INIT(0xCF, 0x4E, 0x3F, 0xB9, 0x5B, 0x00, 0x24, 0xC0, 0xA4, 0x08, 0x3F, 0x56, 0xBF, 0x27, 0x3F, 0xB4);

// Lock Command Characteristic
// String: e089c541e6045fabb9433d8947b3ded5
static const ble_uuid128_t lock_cmd_chr_uuid = BLE_UUID128_INIT(0xB9, 0x43, 0x3D, 0x89, 0x47, 0xB3, 0xDE, 0xD5, 0xAB, 0x5F, 0x04, 0xE6, 0x41, 0xC5, 0x89, 0xE0);

// Battery GATT Service
static const ble_uuid16_t battery_svc_uuid = BLE_UUID16_INIT(0x180F);
static const ble_uuid16_t battery_level_chr_uuid = BLE_UUID16_INIT(0x2A19);

static void door_state_update_cb(door_state_t door_state)
{
    if (door_state.door_bolt_state == 1 && door_state.dpi_state == 1)
    {
        door_cmd_begin_relock_timer();
    }
    ble_gatts_chr_updated(lock_state_chr_handle);
}

static int door_lock_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        ESP_LOGI(TAG, "received read characteristic command to conn_handle=%d, attr_handle=%d", conn_handle, attr_handle);
        if (attr_handle == lock_state_chr_handle)
        {
            door_state_t door_state = door_cmd_get_door_state();
            uint8_t buffer[sizeof(door_state)];
            memcpy(buffer, &door_state, sizeof(door_state));


            int rc = os_mbuf_append(ctxt->om, &buffer, sizeof(buffer));
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

static int battery_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    return 0;
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
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_AUTHEN,
                .val_handle = &lock_state_chr_handle,
                .access_cb = door_lock_chr_access_cb,
            },

            // Lock Command Characteristic
            {
                .uuid = &lock_cmd_chr_uuid.u,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_AUTHEN,
                .val_handle = &lock_cmd_chr_handle,
                .access_cb = door_lock_chr_access_cb,
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
                .access_cb = battery_access_cb,
                .descriptors = (struct ble_gatt_dsc_def[]){
                    {0},
                },
            },

            {0},
        },
    },

    {0},
};

void gatt_svc_init()
{

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

    register_door_lock_state_cb(door_state_update_cb);

    ble_svc_gatt_init();
}