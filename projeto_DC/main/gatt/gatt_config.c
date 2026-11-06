#include "gatt_config.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "host/ble_hs.h"

static const char *TAG = "GATT_CONFIG";

#define CFG_FMT_VERSION  0x01
#define CFG_PERIOD_MIN   1
#define CFG_PERIOD_MAX   3600
#define CFG_SSID_MAX     32
#define CFG_PASS_MAX     64
#define CFG_URI_MAX      127

// Declaração dos UUIDs
static ble_uuid128_t cfg_svc_uuid;
static ble_uuid128_t cfg_period_uuid;
static ble_uuid128_t cfg_ssid_uuid;
static ble_uuid128_t cfg_pass_uuid;
static ble_uuid128_t cfg_uri_uuid;

// Handles das características do serviço
static uint16_t cfg_period_handle;
static uint16_t cfg_ssid_handle;
static uint16_t cfg_pass_handle;
static uint16_t cfg_uri_handle;

// Constrói os UUIDs
static void build_uuid(ble_uuid128_t *u, uint16_t node_id, uint8_t idx)
{
    const uint8_t v[16] = {
        (uint8_t)(node_id & 0xFF), (uint8_t)(node_id >> 8), 0x00, 0x00, 0x00, 0x00,  // 00000000XXXX (little endian)
        0x00, 0x80,                                                                  // 8000 
        0x00, 0x40,                                                                  // 4000 
        0x00, 0x00,                                                                  // 0000 
        idx, 0xDE, 0xC0, 0xE0                                                        // E0C0DE0x 
    };
    u->u.type = BLE_UUID_TYPE_128;
    memcpy(u->value, v, sizeof(v));             // copiar os 16 bytes para o UUID
}

static void wipe(void *p, size_t n)
{
    volatile uint8_t *b = (volatile uint8_t *)p;
    while (n--) {
        *b++ = 0;
    }
}


static int read_payload(struct os_mbuf *om, uint8_t *buf, uint16_t max, uint16_t *len)
{
    uint16_t pkt = OS_MBUF_PKTLEN(om);
    if (pkt < 1 || pkt > max) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (ble_hs_mbuf_to_flat(om, buf, max, len) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (buf[0] != CFG_FMT_VERSION) {
        return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    }
    return 0;
}

static int esp_to_att(esp_err_t e)
{
    if (e == ESP_OK) {
        return 0;
    }
    return (e == ESP_ERR_INVALID_ARG) ? BLE_ATT_ERR_VALUE_NOT_ALLOWED : BLE_ATT_ERR_UNLIKELY;
}

// Escrita de uma característica de texto: valida, copia, chama o setter e limpa o buffer
static int write_text(struct os_mbuf *om, uint16_t max_chars,
                      esp_err_t (*setter)(const char *), const char *name, bool secret)
{
    uint8_t buf[1 + CFG_URI_MAX + 1];
    uint16_t len = 0;

    int rc = read_payload(om, buf, 1 + max_chars, &len);
    if (rc != 0) {
        wipe(buf, sizeof(buf));
        return rc;
    }

    char text[CFG_URI_MAX + 1];
    uint16_t n = len - 1;
    memcpy(text, &buf[1], n);
    text[n] = '\0';

    esp_err_t e = setter(text);

    // RNF6: nunca registar o valor, apenas a dimensão 
    ESP_LOGI(TAG, "BLE write %s: %u bytes%s", name, n, secret ? " (protected)" : "");

    wipe(buf, sizeof(buf));
    wipe(text, sizeof(text));
    return esp_to_att(e);
}

static int config_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    app_config_t cfg;
    int rc = 0;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: {
        app_config_get(&cfg);
        const uint8_t ver = CFG_FMT_VERSION;

        if (attr_handle == cfg_period_handle) {
            uint16_t p = (uint16_t)cfg.sample_period_s;
            rc = os_mbuf_append(ctxt->om, &ver, 1);
            if (rc == 0) rc = os_mbuf_append(ctxt->om, &p, sizeof(p));
        } else if (attr_handle == cfg_ssid_handle) {
            rc = os_mbuf_append(ctxt->om, &ver, 1);
            if (rc == 0) rc = os_mbuf_append(ctxt->om, cfg.wifi_ssid, strlen(cfg.wifi_ssid));
        } else if (attr_handle == cfg_uri_handle) {
            rc = os_mbuf_append(ctxt->om, &ver, 1);
            if (rc == 0) rc = os_mbuf_append(ctxt->om, cfg.telemetry_uri, strlen(cfg.telemetry_uri));
        } else {
            wipe(&cfg, sizeof(cfg));
            return BLE_ATT_ERR_UNLIKELY;
        }
        wipe(&cfg, sizeof(cfg));                        // a struct pode conter a password
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (attr_handle == cfg_period_handle) {
            uint8_t buf[3];
            uint16_t len = 0;
            if (OS_MBUF_PKTLEN(ctxt->om) != sizeof(buf)) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            rc = read_payload(ctxt->om, buf, sizeof(buf), &len);
            if (rc != 0) {
                return rc;
            }
            uint16_t period = (uint16_t)(buf[1] | (buf[2] << 8));
            if (period < CFG_PERIOD_MIN || period > CFG_PERIOD_MAX) {
                return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
            }
            ESP_LOGI(TAG, "BLE period write: %u s", period);
            return esp_to_att(app_config_set_sample_period(period));
        }
        if (attr_handle == cfg_ssid_handle) {
            return write_text(ctxt->om, CFG_SSID_MAX, app_config_set_wifi_ssid, "SSID", false);
        }
        if (attr_handle == cfg_pass_handle) {
            return write_text(ctxt->om, CFG_PASS_MAX, app_config_set_wifi_password, "password", true);
        }
        if (attr_handle == cfg_uri_handle) {
            return write_text(ctxt->om, CFG_URI_MAX, app_config_set_telemetry_uri, "URI", false);
        }
        return BLE_ATT_ERR_UNLIKELY;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}


#define CFG_RW_FLAGS (BLE_GATT_CHR_F_READ  | BLE_GATT_CHR_F_READ_ENC | \
                      BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN)
#define CFG_WO_FLAGS (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN)

static const struct ble_gatt_svc_def cfg_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &cfg_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid = &cfg_period_uuid.u, .access_cb = config_chr_access,
              .flags = CFG_RW_FLAGS, .val_handle = &cfg_period_handle },
            { .uuid = &cfg_ssid_uuid.u,   .access_cb = config_chr_access,
              .flags = CFG_RW_FLAGS, .val_handle = &cfg_ssid_handle },
            { .uuid = &cfg_pass_uuid.u,   .access_cb = config_chr_access,
              .flags = CFG_WO_FLAGS, .val_handle = &cfg_pass_handle },   
            { .uuid = &cfg_uri_uuid.u,    .access_cb = config_chr_access,
              .flags = CFG_RW_FLAGS, .val_handle = &cfg_uri_handle },
            { 0 }
        },
    },
    { 0 }
};

int gatt_config_init(void)
{
    app_config_t cfg;
    app_config_get(&cfg);
    uint16_t node_id = (uint16_t)cfg.node_id;
    wipe(&cfg, sizeof(cfg));

    build_uuid(&cfg_svc_uuid,    node_id, 0x00);
    build_uuid(&cfg_period_uuid, node_id, 0x01);
    build_uuid(&cfg_ssid_uuid,   node_id, 0x02);
    build_uuid(&cfg_pass_uuid,   node_id, 0x03);
    build_uuid(&cfg_uri_uuid,    node_id, 0x04);

    int rc = ble_gatts_count_cfg(cfg_svcs);
    if (rc != 0) {
        return rc;
    }
    return ble_gatts_add_svcs(cfg_svcs);
}