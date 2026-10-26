#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "storage.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "sensor.h"

#define MOUNT_POINT         "/sdcard"
#define DATA_FILE_PATH      MOUNT_POINT "/dht20.csv"
#define LOG_FILE_PATH       MOUNT_POINT "/oper_log.log"

#define PIN_NUM_MISO 20
#define PIN_NUM_MOSI 19
#define PIN_NUM_CLK  21
#define PIN_NUM_CS   18
#define BOOT_BUTTON_GPIO 9

static const char *TAG = "storage_module";
static sdmmc_card_t *card = NULL;

esp_err_t init_sd_card(void) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    if (spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA) != ESP_OK) return ESP_FAIL;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    if (esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card) != ESP_OK) {
        spi_bus_free(host.slot);
        return ESP_FAIL;
    }

    struct stat st;
    if (stat(DATA_FILE_PATH, &st) != 0) {
        FILE *f = fopen(DATA_FILE_PATH, "w");
        if (f) {
            fprintf(f, "temp_C,hum_pct\n");
            fclose(f);
        }
    }

    write_operation_log("Sistema iniciado e cartao SD montado com sucesso.");
    return ESP_OK;
}

void deinit_sd_card(void) {
    write_operation_log("Desmontagem segura do cartao solicitada.");
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    spi_bus_free(SPI2_HOST);
}

void init_boot_button(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
}

void write_operation_log(const char *message) {
    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (f) {
        time_t now = time(NULL);
        fprintf(f, "[LOG] Timestamp=%ld: %s\n", (long)now, message);
        fclose(f);
    }
}

void logger_task(void *arg) {
    MessageBufferHandle_t msg_buffer = (MessageBufferHandle_t)arg;
    sample_t sample;

    while (1) {
        size_t received = xMessageBufferReceive(msg_buffer, &sample, sizeof(sample_t), pdMS_TO_TICKS(100));
        
        if (received == sizeof(sample_t)) {
            // Escreve apenas os dados de temperatura e humidade no ficheiro CSV de leituras
            FILE *f = fopen(DATA_FILE_PATH, "a");
            if (f) {
                fprintf(f, "%.2f,%.2f\n", sample.temperature, sample.humidity);
                fclose(f);
                ESP_LOGI(TAG, "Dado registado em CSV: T=%.2fC, H=%.2f%%", sample.temperature, sample.humidity);
            }
        } else {
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                ESP_LOGI(TAG, "Botão BOOT premido. A encerrar operações de armazenamento...");
                deinit_sd_card();
                vTaskDelete(NULL);
            }
        }
    }
}