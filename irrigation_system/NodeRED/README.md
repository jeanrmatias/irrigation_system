# Dashboard no Node-RED

Painel do sistema de irrigação feito no **Node-RED** com o **Dashboard 2.0** (`@flowfuse/node-red-dashboard`).
Ele funciona sem nenhum hardware: um **simulador** faz o papel do Arduino e do ESP32, com as mesmas regras do firmware.

## Como rodar

Precisa só do Node.js (18 ou mais novo).

```bash
cd NodeRED
npm install
npm start
```

- Dashboard: <http://localhost:1880/dashboard/irrigacao>
- Editor do Node-RED (para ver e mexer no flow): <http://localhost:1880>

## O que tem na tela

| Bloco | O que mostra |
|---|---|
| Umidade do solo | Tanque com a leitura do solo e o nível (SECO, BAIXO, MÉDIO, ALTO) |
| Ar | Umidade do ar e temperatura |
| Estado do sistema | Avisos, válvula, modo, conexão Arduino e ESP32, sensores, último dado e regas |
| Históricos | Gráficos dos últimos 10 min. O do solo mostra as linhas de "seco" (100) e "médio" (300) |
| Controle remoto | Tempo de rega, **Regar agora**, **Parar**, chave AUTO/MANUAL e a resposta que voltou do dispositivo |
| Simulação | Liga e desliga o Arduino e o ESP32, simula falha no DHT11 e no sensor de solo, e "seca o solo" para mostrar a rega automática |

Os avisos aparecem na tela e como notificação:

- Arduino offline;
- sem dados novos há mais de 15 s;
- sensor de solo desconectado (a rega automática fica suspensa);
- falha no DHT11;
- comando sem resposta ou com erro.

## Como o flow funciona

```
tick (1 s) ──► Simulador Arduino + ESP32 ──► Decodificar UltraLight ──► widgets
                     ▲                              ▲
 botões / chave ──► Montar comando ─────────────────┘ (resposta do comando)
```

O simulador fala **o mesmo "idioma" do ESP32 de verdade**:

- publica `s|350|u|55.3|t|24.3|v|0|m|A|e|0|l|1` em `/TEF/irrig001/attrs`;
- recebe `irrig001@regar|5` em `/TEF/irrig001/cmd`;
- responde `irrig001@regar|OK` em `/TEF/irrig001/cmdexe`.

Ele também usa as regras de `Arduino/src/config.h` e `irrigacao.cpp`:

- rega por 3 s quando o solo fica abaixo de 100, ou quando o ar fica abaixo de 30 % com o solo abaixo de 300;
- espera 30 s entre uma rega automática e outra;
- não rega sozinho se o sensor de solo falhar;
- considera o Arduino offline depois de 6 s sem dados;
- responde `ERRO_VALOR` e `ARDUINO_OFFLINE` como o ESP32.

Por isso, a parte do dashboard (decodificador e widgets) não muda nada quando os dados passam a vir do dispositivo real.

## Usando com o ESP32 e o FIWARE de verdade

No editor do Node-RED (<http://localhost:1880>):

1. Desative o nó **tick (1 s)**: dois cliques, depois *Enabled* → desligado. Assim o simulador para.
2. Ative os três nós MQTT: **dados do ESP32**, **respostas do ESP32** e **comandos para o ESP32**.
3. Abra o broker **FIWARE Mosquitto** e troque `localhost` pelo IP da máquina do FIWARE.
4. Clique em **Deploy**.

## Arquivos

| Arquivo | O que é |
|---|---|
| `flows.json` | O flow (simulador, decodificador e dashboard) |
| `package.json` | Node-RED e Dashboard 2.0 nas versões testadas |

O Node-RED cria outros arquivos nesta pasta ao rodar (`settings.js`, `.config.*.json`, `node_modules/`). Eles estão no `.gitignore`.
