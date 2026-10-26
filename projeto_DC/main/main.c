#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sensor.h"
#include "storage.h"

static const char *TAG = "MAIN";

// Pinos
#define I2C_SDA_PIN 2
#define I2C_SCL_PIN 1
#define SPI_MOSI_PIN 19
#define SPI_MISO_PIN 20
#define SPI_SCK_PIN  18
#define SPI_CS_PIN   10

void sensor_task(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1000); // 1 segundo
    uint32_t counter = 0;

    while (1) {
        counter++;
        if (dht20_read_and_buffer(counter) != ESP_OK) {
            ESP_LOGW(TAG, "Erro ou aviso na leitura/buffer do DHT20");
        }

        // xTaskDelayUntil mantendo o período preciso das aulas
        xTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void sd_task(void *pvParameters)
{
    dht20_sample_t sample;
    char line[128];

    while (1) {
        // Bloqueia à espera que o buffer do módulo dht20 tenha uma amostra
        if (dht20_get_sample_from_buffer(&sample, UINT32_MAX)) {
            snprintf(line, sizeof(line), "%lld,%u,%.2f,%.2f",
                     sample.timestamp_ms, sample.counter, sample.temperature, sample.humidity);
            
            sd_card_write_line("/dht20.csv", line);
            ESP_LOGI(TAG, "Gravado no SD: %s", line);
        }
    }
}

void app_main(void)
{
    dht20_init(I2C_SDA_PIN, I2C_SCL_PIN);
    sd_card_init(SPI_MOSI_PIN, SPI_MISO_PIN, SPI_SCK_PIN, SPI_CS_PIN);

    xTaskCreate(sensor_task, "sensor_task", 3072, NULL, 5, NULL);
    xTaskCreate(sd_task, "sd_task", 4096, NULL, 3, NULL);
}