#ifndef STORAGE_MODULE_H
#define STORAGE_MODULE_H



esp_err_t init_sd_card(void);
void deinit_sd_card(void);
void init_boot_button(void);
void logger_task(void *arg);
void write_operation_log(const char *message);

#endif