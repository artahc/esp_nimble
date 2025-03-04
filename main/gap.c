#include "esp_log.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

#include "gap.h"

static const char TAG[] = "gap";

static uint8_t own_addr_type;
static int gap_event_cb(struct ble_gap_event *event, void *arg)
{

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ble_gap_adv_stop();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        start_adv();
        break;
    default:
        break;
    }

    return 0;
}

void adv_init()
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

    rc = ble_svc_gap_device_name_set("esp32c3");
    if (rc != 0)
    {
        ESP_LOGE(TAG, "failed to configure ble: %d", rc);
        return;
    }

    start_adv();
}

void start_adv()
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

void gap_svc_init()
{
    ble_svc_gap_init();
}