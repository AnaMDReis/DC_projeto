#ifndef GAT_ESS_H
#define GAT_ESS_H

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

#endif