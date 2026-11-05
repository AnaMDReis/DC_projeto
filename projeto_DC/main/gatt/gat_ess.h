#pragma once

/*
 * gat_ess - Environmental Sensing Service (0x181A)  [RF8]
 *
 * Características: Temperature (0x2A6E) e Humidity (0x2A6F), leitura + notificação.
 * Este módulo NÃO depende do sensor, da rede, do storage ou da configuração (RNF7):
 * quem tiver amostras novas chama gat_ess_update().
 */

struct ble_gap_event;

/* Regista o serviço na pilha NimBLE. Chamar antes de arrancar o host. */
int gatt_ess_init(void);

/* Guarda a última amostra válida e notifica os clientes subscritos.
 * Pode ser chamada de qualquer tarefa (nunca bloqueia a tarefa do host). */
void gatt_ess_update(float temperature_c, float humidity_pct);

/* Volta ao valor "desconhecido" (0x8000 / 0xFFFF), p.ex. se o sensor falhar de vez. */
void gatt_ess_invalidate(void);

/* Opcional: apenas regista nos logs as subscrições do CCCD. */
void gatt_ess_on_subscribe(const struct ble_gap_event *event);
