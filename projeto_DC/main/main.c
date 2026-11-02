#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"

#include "led_status.h"
#include "storage.h"
#include "sensor.h"
#include "config.h"

static const char *TAG = "main";

// Cada mensagem ocupa sizeof(sample_t) + sizeof(size_t) de cabeçalho do Message Buffer
#define MSG_BUFFER_DEPTH   10
#define MSG_BUFFER_BYTES   (MSG_BUFFER_DEPTH * (sizeof(sample_t) + sizeof(size_t)))

void app_main(void) {
    // 1. LED primeiro, para sinalizar o estado desde o arranque
    led_status_init();

    // 2. Montar o cartão SD (necessário antes de ler a configuração)
    esp_err_t sd_ret = init_sd_card();
    if (sd_ret != ESP_OK) {
        ESP_LOGE(TAG, "Cartão SD indisponível (%s)", esp_err_to_name(sd_ret));
        led_status_set(ESTADO_ERRO);
    }

    // 3. Ler a configuração. Se o cartão ou o ficheiro faltarem, usa o valor por omissão
    config_load("/sdcard/config.json");

    // 4. Message Buffer entre aquisição e gravação
    MessageBufferHandle_t msg_buffer = xMessageBufferCreate(MSG_BUFFER_BYTES);
    if (msg_buffer == NULL) {
        ESP_LOGE(TAG, "Falha ao criar o Message Buffer");
        led_status_set(ESTADO_ERRO);
        return;
    }

    // 5. Sensor
    init_dht20_i2c();
    xTaskCreate(sensor_task, "sensor_task", 4096, msg_buffer, 5, NULL);

    // 6. Gravação no cartão (só se o cartão estiver montado)
    if (sd_ret == ESP_OK) {
        xTaskCreate(logger_task, "logger_task", 4096, msg_buffer, 4, NULL);
    }

    ESP_LOGI(TAG, "Sistema iniciado");
}