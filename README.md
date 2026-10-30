### Semana 1 (19/10 a 30/10)
**Requisitos trabalhados:** RF1, RF3, RF4, RNF1, RNF7

**Trabalho realizado.** Implementação da leitura I2C do DHT20 com verificação de CRC (`main/sensor/sensor.c`), que envia as amostras por Message Buffer para a tarefa de gravação no microSD (`main/storage/storage.c`). O armazenamento monta o cartão por SPI, grava as leituras em `leitura.txt` e reencaminha o log para `log.txt`. Desenvolvimento do controlo do LED RGB via RMT, com as cores do RF3. Escrita dos ADR 001 e 002 em `docs/adr/`.

**Decisões tomadas.** O mecanismo de comunicação da aquisição (Message Buffer) foi atribuído por sorteio, pelo que não constitui decisão do grupo. Decidimos que o envio da amostra é não bloqueante (timeout 0), para que um atraso na gravação no cartão nunca atrase a aquisição. Cartão por SPI com mutex e reencaminhamento de logs com timeout de 50 ms, registado em `docs/adr/ADR_002.md`. Comunicação com o sensor por I²C, registada em `docs/adr/ADR_001.md`. 

**Dificuldades.**
- Deadlock possível por reentrância no hook de logs (erros emitidos pelo sistema de ficheiros durante a escrita): resolvido, o hook deteta se a tarefa já detém o mutex e passa a esperar no máximo 50 ms.
- Desmontagem do cartão com outra tarefa a escrever: resolvido, o `deinit_sd_card` adquire o mutex antes de desmontar e não o apaga.
- Período de amostragem fixo a 1000 ms no `sensor_task`, em vez de vir do JSON (RF1, RF7): pendente.

**Verificações efetuadas.** Projeto compilado sem erros. Ainda não foram feitos testes funcionais em hardware nem medições.

**Divisão de trabalho.** 125305: módulo de armazenamento em microSD (`storage.c`), ADR 001 e 002 e tabela de estado dos requisitos. 126541: aquisição do sensor DHT20 (`sensor.c`), controlo do LED RGB (`led_status.c`) e criação do diagrama ADR.

**Utilização de ferramentas de geração assistida.** Utilização de um assistente para revisão de código 

**Trabalho previsto para a semana seguinte.** Ler o período de amostragem do JSON (módulo de configuração). Acrescentar timestamp e versão de formato aos ficheiros. Preparar a tarefa de rede e a interface BLE.