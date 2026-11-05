#pragma once

/*
 * gat_config - Serviço GATT proprietário de configuração  [RF9 / RF10]
 *
 * UUID do serviço:       E0C0DE00-0000-4000-8000-00000000XXXX  (XXXX = node_id em hex)
 * Características:       E0C0DE01 período (R/W), E0C0DE02 SSID (R/W),
 *                        E0C0DE03 password (só escrita), E0C0DE04 URI (R/W)
 *
 * Formato dos valores (versão 1): [1 byte versão = 0x01][payload]
 *   período : uint16 little-endian, segundos
 *   texto   : bytes UTF-8, sem '\0'
 *
 * Todas as características exigem ligação cifrada e autenticada (passkey).
 * O módulo só conhece o módulo de configuração (app_config.h).
 */

int gatt_config_init(void);
