#include "gatt_svc.h"
#include "common.h"
#include "app_config.h"
#include "sensor_task.h"
#include "storage.h"
#include "led_status.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "host/ble_hs.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ECO_GATT_SVC";

static int environmental_sensing_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                            struct ble_gatt_access_ctxt *ctxt, void *arg);
static int config_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg);

// Defenir os UUIDs oficiais
static const ble_uuid16_t ess_svc_uuid = BLE_UUID16_INIT(0x181A);

static uint16_t temp_chr_val_handle;
static const ble_uuid16_t temp_chr_uuid = BLE_UUID16_INIT(0x2A6E); // Temperature

static uint16_t hum_chr_val_handle;
static const ble_uuid16_t hum_chr_uuid = BLE_UUID16_INIT(0x2A6F);  // Humidity

static uint16_t ess_conn_handle = 0;
static bool ess_conn_handle_inited = false;
static bool ess_notify_status = false;

/* 2. Proprietary Configuration Service (RF9) - UUID Base: EOCODE00-... */
static ble_uuid128_t config_svc_uuid = {0};
static uint16_t config_svc_val_handle;

// Características de Configuração Proprietárias (EOCODE01, EOCODE02, EOCODE03, EOCODE04)
static ble_uuid128_t cfg_period_uuid = {0};   // EOCODE01: Periodicidade (Read/Write)
static ble_uuid128_t cfg_ssid_uuid = {0};     // EOCODE02: WiFi SSID (Read/Write)
static ble_uuid128_t cfg_pass_uuid = {0};     // EOCODE03: WiFi Password (Write Only - RNF6)
static ble_uuid128_t cfg_uri_uuid = {0};      // EOCODE04: Telemetry URI (Read/Write)

static uint16_t cfg_period_handle;
static uint16_t cfg_ssid_handle;
static uint16_t cfg_pass_handle;
static uint16_t cfg_uri_handle;


// Constrói dinamicamente os UUIDs de 128 bits baseados no node_id (EOCODE0X).

static void build_proprietary_uuids(uint16_t node_id) {
    // Base UUID template: E0 CODE xx - 0000 - 4000 - 8000 - 00000000 XXXX (Little-endian no NimBLE)
    // Vamos preencher o buffer de 16 bytes para cada característica.
    
    uint8_t base_uuid_template[16] = {
        0xCC, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0x00, 0x00, // Exemplo base
        0x00, 0x40, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00
    };
    
    // Inserir node_id nos últimos 2 bytes (Little Endian)
    base_uuid_template[0] = node_id & 0xFF;
    base_uuid_template[1] = (node_id >> 8) & 0xFF;

    // Serviço de Configuração (EOCODE00)
    config_svc_uuid.u.type = BLE_UUID_TYPE_128;
    memcpy(config_svc_uuid.value, base_uuid_template, 16);
    config_svc_uuid.value[2] = 0x00; // EOCODE00

    // Periodicidade (EOCODE01)
    cfg_period_uuid.u.type = BLE_UUID_TYPE_128;
    memcpy(cfg_period_uuid.value, base_uuid_template, 16);
    cfg_period_uuid.value[2] = 0x01;

    // WiFi SSID (EOCODE02)
    cfg_ssid_uuid.u.type = BLE_UUID_TYPE_128;
    memcpy(cfg_ssid_uuid.value, base_uuid_template, 16);
    cfg_ssid_uuid.value[2] = 0x02;

    // WiFi Password (EOCODE03) - Write Only (RNF6)
    cfg_pass_uuid.u.type = BLE_UUID_TYPE_128;
    memcpy(cfg_pass_uuid.value, base_uuid_template, 16);
    cfg_pass_uuid.value[2] = 0x03;

    // Telemetry URI (EOCODE04)
    cfg_uri_uuid.u.type = BLE_UUID_TYPE_128;
    memcpy(cfg_uri_uuid.value, base_uuid_template, 16);
    cfg_uri_uuid.value[2] = 0x04;
}

// Definição da tabela de serviços GATT
static struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* 1. Environmental Sensing Service (RF8) */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &ess_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &temp_chr_uuid.u,
                .access_cb = environmental_sensing_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &temp_chr_val_handle
            },
            {
                .uuid = &hum_chr_uuid.u,
                .access_cb = environmental_sensing_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &hum_chr_val_handle
            },
            { 0 }
        }
    },
    /* 2. Proprietary Configuration Service (RF9) */
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &config_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &cfg_period_uuid.u,
                .access_cb = config_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .val_handle = &cfg_period_handle
            },
            {
                .uuid = &cfg_ssid_uuid.u,
                .access_cb = config_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .val_handle = &cfg_ssid_handle
            },
            {
                .uuid = &cfg_pass_uuid.u,
                .access_cb = config_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &cfg_pass_handle
            },
            {
                .uuid = &cfg_uri_uuid.u,
                .access_cb = config_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .val_handle = &cfg_uri_handle
            },
            { 0 }
        }
    },
    { 0 }
};

// Aceder aos Callbacks

static int environmental_sensing_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    app_config_t cfg;
    app_config_get(&cfg);

    // Exemplo simulado de última leitura (deve integrar com sensor_task ou variáveis globais)
    float temperature = 23.5f; 
    float humidity = 60.0f;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (attr_handle == temp_chr_val_handle) {
                int16_t temp_val = (int16_t)(temperature * 100); // Formato padrão ESS (centi-graus)
                int rc = os_mbuf_append(ctxt->om, &temp_val, sizeof(temp_val));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            } else if (attr_handle == hum_chr_val_handle) {
                uint16_t hum_val = (uint16_t)(humidity * 100); // Formato padrão ESS (centi-% )
                int rc = os_mbuf_append(ctxt->om, &hum_val, sizeof(hum_val));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            break;
        default:
            break;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int config_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    app_config_t cfg;
    app_config_get(&cfg);
    int rc = 0;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (attr_handle == cfg_period_handle) {
                rc = os_mbuf_append(ctxt->om, &cfg.sample_period_s, sizeof(cfg.sample_period_s));
            } else if (attr_handle == cfg_ssid_handle) {
                rc = os_mbuf_append(ctxt->om, cfg.wifi_ssid, strlen(cfg.wifi_ssid));
            } else if (attr_handle == cfg_uri_handle) {
                rc = os_mbuf_append(ctxt->om, cfg.telemetry_uri, strlen(cfg.telemetry_uri));
            }
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR: {
            uint16_t len = ctxt->om->om_len;
            char buf[128] = {0};
            if (len >= sizeof(buf)) len = sizeof(buf) - 1;
            memcpy(buf, ctxt->om->om_data, len);
            buf[len] = '\0';

            if (attr_handle == cfg_period_handle) {
                uint32_t new_period = *(uint32_t *)buf;
                ESP_LOGI(TAG, "BLE Config: Nova periodicidade recebida: %lu", new_period);
                // Atualizar lógica de configuração (RF9 / RF10)
            } else if (attr_handle == cfg_ssid_handle) {
                ESP_LOGI(TAG, "BLE Config: Novo SSID recebido");
            } else if (attr_handle == cfg_pass_handle) {
                // RNF6: Password escrita mas nunca lida de volta
                ESP_LOGI(TAG, "BLE Config: Nova Password recebida com sucesso (Protegida)");
            } else if (attr_handle == cfg_uri_handle) {
                ESP_LOGI(TAG, "BLE Config: Novo URI de telemetria recebido: %s", buf);
            }
            storage_write_log("INFO", "Parâmetro atualizado via GATT BLE.");
            return 0;
        }
        default:
            break;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

void send_environmental_notification(float temp, float hum) {
    if (ess_notify_status && ess_conn_handle_inited) {
        uint16_t temp_val = (int16_t)(temp * 100);
        // Envia notificação da temperatura
        ble_gatts_chr_updated(temp_chr_val_handle);
        ESP_LOGI(TAG, "Notificação BLE enviada (Temp / Hum)!");
    }
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];
    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD(TAG, "Serviço registcdo: %s, handle=%d", ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
            break;
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD(TAG, "Característica registada: %s, def_handle=%d, val_handle=%d", ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), ctxt->chr.def_handle, ctxt->chr.val_handle);
            break;
        default:
            break;
    }
}

void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    if (event->subscribe.attr_handle == temp_chr_val_handle || event->subscribe.attr_handle == hum_chr_val_handle) {
        ess_conn_handle = event->subscribe.conn_handle;
        ess_conn_handle_inited = true;
        ess_notify_status = event->subscribe.cur_notify;
        ESP_LOGI(TAG, "Subscrição ESS atualizada: notify=%d", ess_notify_status);
    }
}

int gatt_svc_init(void) {
    int rc = 0;

    // Obter o node_id configurado para construir os UUIDs proprietários dinamicamente (RF9)
    app_config_t cfg;
    app_config_get(&cfg);
    build_proprietary_uuids(cfg.node_id);

    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) return rc;

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) return rc;

    return 0;
}