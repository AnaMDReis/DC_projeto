#ifndef LED_H
#define LED_H

// Estados do sistema definidos no requisito RF3
typedef enum {
    ESTADO_NORMAL,           // Verde
    ESTADO_ERRO,             // Vermelho
    ESTADO_TRANSMISSAO,      // Amarelo
    ESTADO_PROVISIONAMENTO   // Azul
} system_status_t;


void led_status_init(void);
void led_status_set(system_status_t status);

#endif 