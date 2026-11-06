#include "led.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "LED";
static led_strip_handle_t led_strip = NULL;

#define LED_RGB_GPIO 8

// Inicializa o periférico do LED RGB no GPIO 8 através do periférico RMT.
void led_status_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_RGB_GPIO,
        .max_leds = 1, // Apenas 1 LED RGB na placa
    };
    
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };

    // Inicializa o dispositivo RMT para o LED RGB
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err == ESP_OK) {
        led_strip_clear(led_strip);
        // Define o estado inicial como NORMAL (Verde)
        led_status_set(ESTADO_NORMAL);
    } else {
        ESP_LOGE(TAG, "Failed to initialize the RGB LED: %s", esp_err_to_name(err));
    }
}

// Estado do LED RGB de acordo com o estado do sistema
void led_status_set(system_status_t status) {
    if (led_strip == NULL) {
        return;
    }

    // Nota: Os valores RGB estão ajustados para brilho moderado (max: 255)
    switch (status) {
        case ESTADO_NORMAL:
            // Verde 
            led_strip_set_pixel(led_strip, 0, 0, 10, 0);
            ESP_LOGI(TAG, "LED status: NORMAL (Green)");
            break;

        case ESTADO_ERRO:
            // Vermelho 
            led_strip_set_pixel(led_strip, 0, 10, 0, 0);
            ESP_LOGW(TAG, "LED status: ERROR (Red)");
            break;

        case ESTADO_TRANSMISSAO:
            // Amarelo -> Vermelho + Verde 
            led_strip_set_pixel(led_strip, 0, 10, 10, 0);
            ESP_LOGI(TAG, "LED status: TRANSMISSION (Yellow)");
            break;

        case ESTADO_PROVISIONAMENTO:
            // Azul 
            led_strip_set_pixel(led_strip, 0, 0, 0, 10);
            ESP_LOGI(TAG, "LED status: PROVISIONING (Blue)");
            break;

        default:
            led_strip_clear(led_strip);
            break;
    }

    // Atualiza o hardware com a nova cor
    led_strip_refresh(led_strip);
}