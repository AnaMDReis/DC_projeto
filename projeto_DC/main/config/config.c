#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "cJSON.h"
#include "config.h"

static const char *TAG = "config";
static volatile uint32_t s_period_s = CONFIG_PERIOD_DEFAULT_S;

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
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }
    size_t n = fread(buf, 1, size, f);
    buf[n] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "JSON inválido, a usar valores por omissão");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *sampling = cJSON_GetObjectItem(root, "sampling");
    cJSON *p = cJSON_GetObjectItem(sampling, "period_s");
    if (cJSON_IsNumber(p) && p->valueint >= CONFIG_PERIOD_MIN_S && p->valueint <= CONFIG_PERIOD_MAX_S) {
        s_period_s = (uint32_t)p->valueint;
    } else {
        ESP_LOGW(TAG, "period_s ausente ou fora de [%d, %d], a usar %d s",
                 CONFIG_PERIOD_MIN_S, CONFIG_PERIOD_MAX_S, CONFIG_PERIOD_DEFAULT_S);
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Período de amostragem: %lu s", (unsigned long)s_period_s);
    return ESP_OK;
}

uint32_t config_get_sample_period_s(void) {
    return s_period_s;
}