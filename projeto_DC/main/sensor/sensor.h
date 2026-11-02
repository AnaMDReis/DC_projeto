#ifndef SENSOR_H
#define SENSOR_H


// Estrutura simplificada contendo apenas as leituras do sensor
typedef struct {
    float temperature;
    float humidity;
} sample_t;

void init_dht20_i2c(void);
void sensor_task(void *arg);

#endif