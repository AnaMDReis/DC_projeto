#ifndef GAT_ESS_H
#define GAT_ESS_H

struct ble_gap_event;

int gatt_ess_init(void);
void gatt_ess_update(float temperature_c, float humidity_pct);
void gatt_ess_invalidate(void);
void gatt_ess_on_subscribe(const struct ble_gap_event *event);

#endif