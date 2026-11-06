#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"
#include "config.h"

static const char *TAG = "config";

static app_config_t s_cfg = {
    .config_version = 1,
    .node_id = 1000,
    .sample_period_s = CONFIG_PERIOD_DEFAULT_S,
    .wifi_ssid = "",
    .wifi_password = "",
    .telemetry_uri = "mqtt://localhost:1883"
};

esp_err_t config_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "Ficheiro %s não encontrado, a usar valores por omissão", path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t n = fread(buf, 1, size, f);
    buf[n] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "JSON inválido, a usar valores por omissão");
        return ESP_ERR_INVALID_ARG;
    }

    // config_version
    cJSON *ver = cJSON_GetObjectItem(root, "config_version");
    if (cJSON_IsNumber(ver)) {
        s_cfg.config_version = (uint32_t)ver->valueint;
    }

    // node_id
    cJSON *nid = cJSON_GetObjectItem(root, "node_id");
    if (cJSON_IsNumber(nid)) {
        s_cfg.node_id = (uint16_t)nid->valueint;
    }

    // sampling.period_s
    cJSON *sampling = cJSON_GetObjectItem(root, "sampling");
    if (cJSON_IsObject(sampling)) {
        cJSON *p = cJSON_GetObjectItem(sampling, "period_s");
        if (cJSON_IsNumber(p) && p->valueint >= CONFIG_PERIOD_MIN_S && p->valueint <= CONFIG_PERIOD_MAX_S) {
            s_cfg.sample_period_s = (uint32_t)p->valueint;
        } else {
            ESP_LOGW(TAG, "period_s inválido ou fora de limites, a usar %d s", CONFIG_PERIOD_DEFAULT_S);
        }
    }

    // wifi (ssid e password)
    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (cJSON_IsObject(wifi)) {
        cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
        if (cJSON_IsString(ssid) && (ssid->valuestring != NULL)) {
            strncpy(s_cfg.wifi_ssid, ssid->valuestring, CFG_SSID_MAX_LEN);
            s_cfg.wifi_ssid[CFG_SSID_MAX_LEN] = '\0';
        }

        cJSON *pass = cJSON_GetObjectItem(wifi, "password");
        if (cJSON_IsString(pass) && (pass->valuestring != NULL)) {
            strncpy(s_cfg.wifi_password, pass->valuestring, CFG_PASS_MAX_LEN);
            s_cfg.wifi_password[CFG_PASS_MAX_LEN] = '\0';
        }
    }

    // telemetry (uri)
    cJSON *tele = cJSON_GetObjectItem(root, "telemetry");
    if (cJSON_IsObject(tele)) {
        cJSON *uri = cJSON_GetObjectItem(tele, "uri");
        if (cJSON_IsString(uri) && (uri->valuestring != NULL)) {
            strncpy(s_cfg.telemetry_uri, uri->valuestring, CFG_URI_MAX_LEN);
            s_cfg.telemetry_uri[CFG_URI_MAX_LEN] = '\0';
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Configuração carregada. Node ID: %u, Período: %lu s", s_cfg.node_id, (unsigned long)s_cfg.sample_period_s);
    return ESP_OK;
}

void app_config_get(app_config_t *out_cfg) {
    if (out_cfg) {
        *out_cfg = s_cfg;
    }
}

uint32_t config_get_sample_period_s(void) {
    return s_cfg.sample_period_s;
}

esp_err_t app_config_set_sample_period(uint16_t period) {
    if (period < CONFIG_PERIOD_MIN_S || period > CONFIG_PERIOD_MAX_S) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg.sample_period_s = period;
    return ESP_OK;
}

esp_err_t app_config_set_wifi_ssid(const char *ssid) {
    if (!ssid) return ESP_ERR_INVALID_ARG;
    strncpy(s_cfg.wifi_ssid, ssid, CFG_SSID_MAX_LEN);
    s_cfg.wifi_ssid[CFG_SSID_MAX_LEN] = '\0';
    return ESP_OK;
}

esp_err_t app_config_set_wifi_password(const char *pass) {
    if (!pass) return ESP_ERR_INVALID_ARG;
    strncpy(s_cfg.wifi_password, pass, CFG_PASS_MAX_LEN);
    s_cfg.wifi_password[CFG_PASS_MAX_LEN] = '\0';
    return ESP_OK;
}

esp_err_t app_config_set_telemetry_uri(const char *uri) {
    if (!uri) return ESP_ERR_INVALID_ARG;
    strncpy(s_cfg.telemetry_uri, uri, CFG_URI_MAX_LEN);
    s_cfg.telemetry_uri[CFG_URI_MAX_LEN] = '\0';
    return ESP_OK;
}