#include "gatt_ess.h"

#include <math.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"

static const char *TAG = "GATT_ESS";

/* Valores "desconhecido" do GATT Specification Supplement */
#define ESS_TEMP_UNKNOWN ((int16_t)0x8000)
#define ESS_HUM_UNKNOWN  ((uint16_t)0xFFFF)

static const ble_uuid16_t ess_svc_uuid  = BLE_UUID16_INIT(0x181A);
static const ble_uuid16_t temp_chr_uuid = BLE_UUID16_INIT(0x2A6E); /* Temperature */
static const ble_uuid16_t hum_chr_uuid  = BLE_UUID16_INIT(0x2A6F); /* Humidity    */

static uint16_t temp_chr_val_handle;
static uint16_t hum_chr_val_handle;

/* Última amostra válida, protegida por mutex (as funções de acesso nunca tocam no sensor) */
static SemaphoreHandle_t s_mutex;
static int16_t  s_temp_centi = ESS_TEMP_UNKNOWN; /* 0,01 °C */
static uint16_t s_hum_centi  = ESS_HUM_UNKNOWN;  /* 0,01 %  */

static int ess_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def ess_defs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &ess_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &temp_chr_uuid.u,
                .access_cb  = ess_chr_access,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &temp_chr_val_handle,
            },
            {
                .uuid       = &hum_chr_uuid.u,
                .access_cb  = ess_chr_access,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &hum_chr_val_handle,
            },
            { 0 }
        },
    },
    { 0 }
};

/* Corre na tarefa do host: só lê memória, sem I2C nem I/O lento. */
static int ess_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    int16_t  temp;
    uint16_t hum;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    temp = s_temp_centi;
    hum  = s_hum_centi;
    xSemaphoreGive(s_mutex);

    int rc;
    if (attr_handle == temp_chr_val_handle) {
        ESP_LOGI(TAG, "Leitura BLE Temperature: %d (x0,01 C)", temp);
        rc = os_mbuf_append(ctxt->om, &temp, sizeof(temp)); /* ESP32-C6 é little-endian */
    } else if (attr_handle == hum_chr_val_handle) {
        ESP_LOGI(TAG, "Leitura BLE Humidity: %u (x0,01 %%)", hum);
        rc = os_mbuf_append(ctxt->om, &hum, sizeof(hum));
    } else {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

void gatt_ess_update(float temperature_c, float humidity_pct)
{
    if (s_mutex == NULL) {
        return;
    }

    long t = lroundf(temperature_c * 100.0f);
    long h = lroundf(humidity_pct * 100.0f);
    if (t < -32767) t = -32767;   /* 0x8000 está reservado a "desconhecido" */
    if (t >  32767) t =  32767;
    if (h <      0) h =      0;
    if (h >  65534) h =  65534;   /* 0xFFFF está reservado a "desconhecido" */

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "Mutex ocupado: amostra BLE descartada");
        return;
    }
    s_temp_centi = (int16_t)t;
    s_hum_centi  = (uint16_t)h;
    xSemaphoreGive(s_mutex);   /* libertar ANTES de notificar */

    /* Apenas agenda o envio; a pilha invoca ess_chr_access na tarefa do host */
    ble_gatts_chr_updated(temp_chr_val_handle);
    ble_gatts_chr_updated(hum_chr_val_handle);
    ESP_LOGI(TAG, "Notificação BLE: temp=%ld hum=%ld (x0,01)", t, h);
}

void gatt_ess_invalidate(void)
{
    if (s_mutex == NULL) {
        return;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_temp_centi = ESS_TEMP_UNKNOWN;
        s_hum_centi  = ESS_HUM_UNKNOWN;
        xSemaphoreGive(s_mutex);
    }
}

void gatt_ess_on_subscribe(const struct ble_gap_event *event)
{
    if (event->subscribe.attr_handle == temp_chr_val_handle ||
        event->subscribe.attr_handle == hum_chr_val_handle) {
        ESP_LOGI(TAG, "Subscrição ESS: attr=%d notify=%d",
                 event->subscribe.attr_handle, event->subscribe.cur_notify);
    }
}

int gatt_ess_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return BLE_HS_ENOMEM;
    }

    int rc = ble_gatts_count_cfg(ess_defs);
    if (rc != 0) {
        return rc;
    }
    return ble_gatts_add_svcs(ess_defs);
}
