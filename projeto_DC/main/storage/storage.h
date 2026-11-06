#ifndef STORAGE_H
#define STORAGE_H

esp_err_t init_sd_card(void);
void deinit_sd_card(void);
void logger_task(void *arg);

#endif