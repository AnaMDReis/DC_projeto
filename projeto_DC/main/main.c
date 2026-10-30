#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"
#include "esp_log.h"

#include "sensor/sensor.h"
#include "storage/storage.h"

static const char *TAG = "main";

// Tamanho do MessageBuffer em bytes (suporta várias estruturas sample_t)
#define MSG_BUFFER_SIZE_BYTES (sizeof(sample_t) * 10)

void app_main(void)
{
    ESP_LOGI(TAG, "A iniciar a aplicacao...");

    // 1. Criar o MessageBuffer do FreeRTOS
    MessageBufferHandle_t msg_buffer = xMessageBufferCreate(MSG_BUFFER_SIZE_BYTES);
    if (msg_buffer == NULL) {
        ESP_LOGE(TAG, "Falha ao criar o MessageBuffer!");
        return;
    }

    // 2. Inicializar o Botao BOOT (para descarte seguro do SD se necessario)
    init_boot_button();

    // 3. Inicializar o cartao microSD
    if (init_sd_card() != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar/montar o cartao SD. A continuar sem SD...");
    }

    // 4. Inicializar o barramento I2C e o sensor DHT20
    init_dht20_i2c();

    // 5. Criar as tarefas FreeRTOS passando o ponteiro do MessageBuffer como argumento
    // Tarefa do Sensor (Prioridade 5 - superior para garantir amostragem rigorosa)
    xTaskCreate(sensor_task, "sensor_task", 4096, (void *)msg_buffer, 5, NULL);

    // Tarefa de Logger do SD (Prioridade 3)
    xTaskCreate(logger_task, "logger_task", 4096, (void *)msg_buffer, 3, NULL);

    ESP_LOGI(TAG, "Tarefas criadas com sucesso.");
}