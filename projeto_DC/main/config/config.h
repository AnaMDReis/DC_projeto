#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define CONFIG_PERIOD_MIN_S      1
#define CONFIG_PERIOD_MAX_S      3600
#define CONFIG_PERIOD_DEFAULT_S  10

#define CFG_SSID_MAX_LEN 32
#define CFG_PASS_MAX_LEN 64
#define CFG_URI_MAX_LEN  127

typedef struct {
    uint32_t config_version;
    uint16_t node_id;
    uint32_t sample_period_s;
    char wifi_ssid[CFG_SSID_MAX_LEN + 1];
    char wifi_password[CFG_PASS_MAX_LEN + 1];
    char telemetry_uri[CFG_URI_MAX_LEN + 1];
} app_config_t;

// Lê o ficheiro JSON no arranque
esp_err_t config_load(const char *path);

// Interface pública utilizada pelo gatt_config.c e pelo resto da aplicação
void app_config_get(app_config_t *out_cfg);
uint32_t config_get_sample_period_s(void);

esp_err_t app_config_set_sample_period(uint16_t period);
esp_err_t app_config_set_wifi_ssid(const char *ssid);
esp_err_t app_config_set_wifi_password(const char *pass);
esp_err_t app_config_set_telemetry_uri(const char *uri);

#endif