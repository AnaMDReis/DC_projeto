### Semana 1 e 2 (19/10 a 30/10)

**Requisitos trabalhados.** RF1, RF3, RF4

**Trabalho realizado.** Implementação da leitura I2C do sensor DHT20 em sensor.c e envio das amostras por Message Buffer para a tarefa de escrita no microSD (storage.c). Desenvolvimento do controlo do LED RGB (led.c), mapeando as cores dos estados do sistema (Verde: Normal, Vermelho: Erro, Amarelo: Transmissão, Azul: Provisionamento). 

**Decisões tomadas.** Adoção de um Message Buffer do FreeRTOS para desacoplar a aquisição da escrita no cartão.
