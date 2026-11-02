#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "esp_err.h"

#define CONFIG_PERIOD_MIN_S      1
#define CONFIG_PERIOD_MAX_S      3600
#define CONFIG_PERIOD_DEFAULT_S  10

// Lê o ficheiro JSON. Em caso de erro, mantém o valor por omissão.
esp_err_t config_load(const char *path);

uint32_t config_get_sample_period_s(void);

#endif