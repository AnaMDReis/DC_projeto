### Semana 2 (07/11 a 13/11)
**Requisitos trabalhados:** RF7, RF8, RF9

**Trabalho realizado.** Implementação do módulo de configuração, que lê o ficheiro JSON do microSD (`config.c`, `config.json`), desenvolvimento do servidor GATT BLE e escrita do ADR 003 em `docs/adr/`.

**Decisões tomadas.** O módulo ESS recebe as amostras por `gatt_ess_update()` em vez de ler o sensor e cada serviço tem a sua função de inicialização. As características proprietárias exigem ligação cifrada e autenticada, a password não tem leitura e os valores levam um byte de versão de formato. Registado em `docs/adr/ADR_003.md`.

**Dificuldades.**
- As alterações recebidas por BLE só ficam em memória; falta gravá-las no microSD: pendente.
- Enviar as medições do sensor para o BLE: pendente.
- Período de amostragem fixo a 1000 ms no `sensor_task`, em vez de vir do JSON: resolvido

**Verificações efetuadas.** Projeto compilado sem erros. Ainda não foram feitos testes funcionais em hardware nem medições.

**Divisão de trabalho.** 125305: módulo de configuração, JSON e atualização da tabela de estados e requisitos. 126541: módulos GATT e ADR 003.

**Utilização de ferramentas de geração assistida.** Utilização de um assistente para auxílio de escrita e revisão do código.

**Trabalho previsto para a semana seguinte.** Gravar a configuração no microSD e testar os módulos no ESP.
