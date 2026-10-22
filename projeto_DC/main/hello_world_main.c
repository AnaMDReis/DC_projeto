#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "DHT20";

/* ---- I2C configuration ---- */
#define I2C_MASTER_SCL_IO       7
#define I2C_MASTER_SDA_IO       6
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      100000      // 100 kHz (seguro também para o TC74)
#define I2C_MASTER_TIMEOUT_MS   1000

/* ---- DHT20 specifics ---- */
#define DHT20_ADDR              0x38
#define DHT20_CMD_STATUS        0x71

/* ---- Inicialização do bus + dispositivo ---- */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle,
                            i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,  // já há pull-ups externas de 4,7 kΩ
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = DHT20_ADDR,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_cfg, dev_handle));
}

/* ---- CRC-8 (poly 0x31, init 0xFF) ---- */
static uint8_t dht20_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* ---- Lê o byte de estado ---- */
static esp_err_t dht20_read_status(i2c_master_dev_handle_t dev, uint8_t *status)
{
    uint8_t cmd = DHT20_CMD_STATUS;
    return i2c_master_transmit_receive(dev, &cmd, 1, status, 1, I2C_MASTER_TIMEOUT_MS);
}

/* ---- Arranque do sensor ---- */
static esp_err_t dht20_init(i2c_master_dev_handle_t dev)
{
    vTaskDelay(pdMS_TO_TICKS(100));             // >= 100 ms após power-on

    uint8_t status;
    esp_err_t err = dht20_read_status(dev, &status);
    if (err != ESP_OK) return err;

    if ((status & 0x18) != 0x18) {
        ESP_LOGW(TAG, "Status = 0x%02X, sensor nao calibrado (ver seccao 7.4)", status);
        // Se necessário, aqui entra a (re)inicialização dos registos descrita na secção 7.4.
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* ---- Uma leitura completa ---- */
static esp_err_t dht20_read(i2c_master_dev_handle_t dev, float *temp, float *hum)
{
    // 1) Disparar medição
    const uint8_t trigger[3] = {0xAC, 0x33, 0x00};
    esp_err_t err = i2c_master_transmit(dev, trigger, sizeof(trigger), I2C_MASTER_TIMEOUT_MS);
    if (err != ESP_OK) return err;

    // 2) Esperar fim da conversão (~80 ms)
    vTaskDelay(pdMS_TO_TICKS(80));

    // 3) Ler 7 bytes; repetir enquanto busy (bit 7) = 1
    uint8_t data[7];
    for (int tries = 0; tries < 5; tries++) {
        err = i2c_master_receive(dev, data, sizeof(data), I2C_MASTER_TIMEOUT_MS);
        if (err != ESP_OK) return err;
        if (!(data[0] & 0x80)) break;
        vTaskDelay(pdMS_TO_TICKS(10));
        if (tries == 4) return ESP_ERR_TIMEOUT;
    }

    // 4) CRC sobre os 6 primeiros bytes
    if (dht20_crc8(data, 6) != data[6]) {
        return ESP_ERR_INVALID_CRC;
    }

    // 5) Extrair os 20 bits de cada grandeza
    uint32_t raw_hum  = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    // 6) Converter
    *hum  = (raw_hum  / 1048576.0f) * 100.0f;
    *temp = (raw_temp / 1048576.0f) * 200.0f - 50.0f;
    return ESP_OK;
}

/* ---- Task ---- */
static void dht20_task(void *pvParameters)
{
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)pvParameters;
    float temp, hum;

    while (1) {
        esp_err_t err = dht20_read(dev, &temp, &hum);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperatura: %.2f C, Humidade: %.2f %%", temp, hum);
        } else {
            ESP_LOGE(TAG, "Erro na leitura: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dht20_handle;

    i2c_master_init(&bus_handle, &dht20_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");

    ESP_ERROR_CHECK(dht20_init(dht20_handle));

    xTaskCreate(dht20_task, "dht20_task", 4096, dht20_handle, 5, NULL);
}