#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "sensor.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/message_buffer.h"
#include "config/config.h"

// Configuração I2C para o DHT20
#define I2C_MASTER_SCL_IO           7
#define I2C_MASTER_SDA_IO           6
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000
#define DHT20_SENSOR_ADDR           0x38


static const char *TAG = "Sensor";
static i2c_master_dev_handle_t dht20_dev_handle = NULL;
static i2c_master_bus_handle_t i2c_bus_handle = NULL;

void init_dht20_i2c(void) {
    i2c_master_bus_config_t i2c_bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DHT20_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dht20_dev_handle));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Sensor DHT20 inicializado via I2C.");
}

static uint8_t calc_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc = (crc << 1);
        }
    }
    return crc;
}

void sensor_task(void *arg) {
    MessageBufferHandle_t msg_buffer = (MessageBufferHandle_t)arg;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        if (xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(config_get_sample_period_s() * 1000)) == pdFALSE) {
            ESP_LOGW(TAG, "Período da tarefa de sensor excedido!");
        }

        uint8_t cmd[3] = {0xAC, 0x33, 0x00};
        if (i2c_master_transmit(dht20_dev_handle, cmd, sizeof(cmd), 100) != ESP_OK) {
            ESP_LOGW(TAG, "Erro I2C ao enviar comando para DHT20");
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(80));

        uint8_t rx_buf[7] = {0};
        if (i2c_master_receive(dht20_dev_handle, rx_buf, 7, 100) != ESP_OK || (rx_buf[0] & 0x80)) {
            ESP_LOGW(TAG, "Erro ou sensor ocupado na leitura do DHT20");
            continue;
        }

        if (calc_crc8(rx_buf, 6) != rx_buf[6]) {
            ESP_LOGW(TAG, "Erro de CRC no DHT20");
            continue;
        }

        uint32_t raw_hum = ((uint32_t)rx_buf[1] << 12) | ((uint32_t)rx_buf[2] << 4) | ((uint32_t)rx_buf[3] >> 4);
        uint32_t raw_temp = (((uint32_t)rx_buf[3] & 0x0F) << 16) | ((uint32_t)rx_buf[4] << 8) | (uint32_t)rx_buf[5];

        // Apenas temperatura e humidade na estrutura
        sample_t sample = {
            .humidity = ((float)raw_hum / 1048576.0f) * 100.0f,
            .temperature = ((float)raw_temp / 1048576.0f) * 200.0f - 50.0f
        };

        size_t sent = xMessageBufferSend(msg_buffer, &sample, sizeof(sample_t), 0);
        if (sent != sizeof(sample_t)) {
            ESP_LOGW(TAG, "Message Buffer cheio! Amostra descartada.");
        }
    }
}