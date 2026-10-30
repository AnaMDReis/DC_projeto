#idf_component_register(SRCS "storage.c" "sensor.c" "main.c"
 #                      PRIV_REQUIRES spi_flash
  #                     INCLUDE_DIRS ""
   #                    REQUIRES esp_driver_i2c)

idf_component_register(
    SRCS 
        "main.c" 
        "sensor/sensor.c" 
        "storage/storage.c"
        "led/led.c"
    INCLUDE_DIRS 
        "."
        "sensor"
        "storage"
        "led"
    REQUIRES 
        esp_driver_i2c       # Para <driver/i2c_master.h>
        esp_driver_spi       # Para <driver/spi_master.h>
        esp_driver_gpio      # Para <driver/gpio.h>
        fatfs                # Para <esp_vfs_fat.h> e <sdmmc_cmd.h>
        freertos             # Para <freertos/FreeRTOS.h> e <freertos/message_buffer.h>
        log                  # Para <esp_log.h>
)