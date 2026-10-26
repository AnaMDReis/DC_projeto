#idf_component_register(SRCS "storage.c" "sensor.c" "main.c"
 #                      PRIV_REQUIRES spi_flash
  #                     INCLUDE_DIRS ""
   #                    REQUIRES esp_driver_i2c)

idf_component_register(
    SRCS 
        "main.c" 
        "sd_card.c" 
        "DHT20_sensor.c"   # <--- confirma se é dht20_sensor.c ou DHT20_sensor.c
    INCLUDE_DIRS 
        "."
    REQUIRES 
        esp_driver_i2c
        esp_driver_spi
        fatfs
        esp_timer
)
