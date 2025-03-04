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
/// String: b43f27bf563f08a4cf4e3fb95b0024c0
static const ble_uuid128_t lock_state_chr_uuid = BLE_UUID128_INIT(0xCF, 0x4E, 0x3F, 0xB9, 0x5B, 0x00, 0x24, 0xC0, 0xA4, 0x08, 0x3F, 0x56, 0xBF, 0x27, 0x3F, 0xB4);
// String: e089c541e6045fabb9433d8947b3ded5
static const ble_uuid128_t lock_cmd_chr_uuid = BLE_UUID128_INIT(0xB9, 0x43, 0x3D, 0x89, 0x47, 0xB3, 0xDE, 0xD5, 0xAB, 0x5F, 0x04, 0xE6, 0x41, 0xC5, 0x89, 0xE0);

// Battery GATT Service
static const ble_uuid16_t battery_svc_uuid = BLE_UUID16_INIT(0x180F);
static const ble_uuid16_t battery_level_chr_uuid = BLE_UUID16_INIT(0x2A19);

static void notify_lock_state_update(void)
{
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
            uint8_t lock_state = door_cmd_get_lock_state();
            int rc = os_mbuf_append(ctxt->om, &lock_state, sizeof(lock_state));
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

static int battery_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
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
    adv_init();
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
                .access_cb = door_lock_chr_access_cb,
            },

            // Lock Command Characteristic
            {
                .uuid = &lock_cmd_chr_uuid.u,
                .flags = BLE_GATT_CHR_F_WRITE,
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

    ble_hs_cfg.reset_cb = gatt_reset_cb;
    ble_hs_cfg.gatts_register_cb = gatt_register_cb;
    ble_hs_cfg.sync_cb = gatt_sync_cb;
    ble_hs_cfg.sm_io_cap = 3;

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

    ble_svc_gatt_init();
}