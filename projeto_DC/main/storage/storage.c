#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h> 
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "storage.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/message_buffer.h"
#include "sensor.h"
#include <stdbool.h>
#include "freertos/task.h"
#include "driver/sdspi_host.h"

#define MOUNT_POINT         "/sdcard"
#define DATA_FILE_PATH      MOUNT_POINT "/leitura.txt"
#define LOG_FILE_PATH       MOUNT_POINT "/log.txt"

#define PIN_NUM_MISO 20
#define PIN_NUM_MOSI 19
#define PIN_NUM_CLK  21
#define PIN_NUM_CS   18

static const char *TAG = "Storage";
static sdmmc_card_t *card = NULL;

static sdmmc_host_t host = SDSPI_HOST_DEFAULT();
static SemaphoreHandle_t storage_mutex = NULL; 
static vprintf_like_t previous_vprintf = NULL; 


// Reencaminhar os logs para o cartão SD e para o terminal
static int storage_log_output(const char *format, va_list args)
{
    // true se esta tarefa já tem o mutex
    bool reentrant = (storage_mutex != NULL) &&
                     (xSemaphoreGetMutexHolder(storage_mutex) == xTaskGetCurrentTaskHandle());

    // Só escreve no cartão se não for reentrante e se conseguir o mutex em 50 ms (evitar deadlock)
    if (!reentrant && storage_mutex != NULL &&
        xSemaphoreTake(storage_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {

        FILE *f = fopen(LOG_FILE_PATH, "a");                        // Abrir o ficheiro de logs

        if (f != NULL) {
            va_list sd_args;
            va_copy(sd_args, args);
            vfprintf(f, format, sd_args);                           // Escrever o log no cartão
            va_end(sd_args);
            fclose(f);
        }

        xSemaphoreGive(storage_mutex);                              // Libertar o mutex
    }

    // Manter a saída dos logs no terminal
    if (previous_vprintf != NULL) {
        va_list uart_args;
        va_copy(uart_args, args);
        int result = previous_vprintf(format, uart_args);
        va_end(uart_args);
        return result;
    }

    return 0;
}




// Configuração da montagem do sistema de ficheiros FAT do cartão
esp_err_t init_sd_card(void) {

    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED        // Se estiver ativada
        .format_if_mount_failed = true,             // Se a montagem falhar o cartão pode ser formatado
#else                                               // Se não estiver ativada
        .format_if_mount_failed = false,            // Se a montagem falhar não pode ser formatado
#endif  // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,     
        .allocation_unit_size = 16 * 1024
    };


    spi_bus_config_t bus_cfg = {                    // Configuração dos pinos e dos parâmetros do barramento SPI
        .mosi_io_num = PIN_NUM_MOSI,                
        .miso_io_num = PIN_NUM_MISO,                 
        .sclk_io_num = PIN_NUM_CLK,                 
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };


    
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);   // Inicializa o SPI
    if (ret != ESP_OK) {                                                // Verifica se houve algum erro a inicializar o barramento
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;                                                     // se houver devolve o código do erro
    }   

    // Configuração do dispotisitivo SD no barramento
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();  
    slot_config.gpio_cs = PIN_NUM_CS;                                   
    slot_config.host_id = host.slot;                                    



    // Montagem do sistema de fcheiros
    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
        
    // se ocorrer algum erro, é enviada uma mensagem de erro e este é devolvido
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));    
            }
        spi_bus_free(host.slot);
        return ret;
    
    }


    storage_mutex = xSemaphoreCreateMutex(); // Criar o mutex

    if (storage_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create storage mutex.");
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        card = NULL;
        spi_bus_free(host.slot);
        return ESP_ERR_NO_MEM;
    }




    // Se o ficheiro não existir cria um novo
    struct stat st;
    if (stat(DATA_FILE_PATH, &st) != 0) {
        FILE *f = fopen(DATA_FILE_PATH, "w");
        if (f == NULL)                                          // Caso o ficheiro não tenha sido corretamente criado
        {
            ESP_LOGE(TAG, "Failed to create the data file");
            vSemaphoreDelete(storage_mutex);                    // Liberta o mutex
            storage_mutex = NULL; 
            esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);      // Desmontar o cartão SD 
            card = NULL;
            spi_bus_free(host.slot);                            // Libertar o barramento SPI
            return ESP_FAIL;
        }

                                                                // Caso o ficheiro tenha sido corretamente criado
        fprintf(f, "Temperatura(ºC),Humidade(%%)\n");           // Escreve no cabeçalho
        fclose(f);
        }
    

    // Caso ainda não exista, criar o ficheiro de logs antes de instalar o reencaminhamento
    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to create the log file");

        vSemaphoreDelete(storage_mutex);
        storage_mutex = NULL;

        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        card = NULL;
        spi_bus_free(host.slot);

        return ESP_FAIL;
    }

    fclose(f);

    previous_vprintf = esp_log_set_vprintf(storage_log_output); // Ativar o reencaminhamento automático

    ESP_LOGI(TAG, "System started and SD card mounted successfully.");

    return ESP_OK;
}



void deinit_sd_card(void) {
    ESP_LOGI(TAG, "Safe SD card unmount requested.");

    // Restaurar a saída original
    if (previous_vprintf != NULL) {
        esp_log_set_vprintf(previous_vprintf);
        previous_vprintf = NULL;
    }

    // Esperar que ninguém esteja a escrever
    if (storage_mutex != NULL) {
        xSemaphoreTake(storage_mutex, portMAX_DELAY);
    }

    if (card != NULL) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        card = NULL;
        spi_bus_free(host.slot);
    }

    if (storage_mutex != NULL) {
        xSemaphoreGive(storage_mutex);   // não apagar o mutex
    }
}


void logger_task(void *arg) {
    MessageBufferHandle_t msg_buffer = (MessageBufferHandle_t)arg;
    sample_t sample;

    while (1) {
        size_t received = xMessageBufferReceive(msg_buffer, &sample, sizeof(sample_t),
                                                pdMS_TO_TICKS(100));

        if (received == sizeof(sample_t)) {
            bool written = false;

            // O mutex impede que o deinit desmonte o cartão a meio da escrita
            if (xSemaphoreTake(storage_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                FILE *f = fopen(DATA_FILE_PATH, "a");
                if (f != NULL) {
                    fprintf(f, "%.2f,%.2f\n", sample.temperature, sample.humidity);
                    fclose(f);
                    written = true;
                }
                xSemaphoreGive(storage_mutex);
            }

            // O log fica fora do mutex (o hook também o usa)
            if (written) {
                ESP_LOGI(TAG, "Data successfully recorded in CSV: T=%.2fC, H=%.2f%%",
                         sample.temperature, sample.humidity);
            } else {
                ESP_LOGW(TAG, "Failed to save the sensor reading to the SD card.");
            }
        }

        // Se o cartão foi desmontado (pelo botão ou por outro módulo), a tarefa termina sozinha
        if (card == NULL) {
            ESP_LOGI(TAG, "SD card unmounted. Terminating storage task.");
            vTaskDelete(NULL);
        }
    }
}