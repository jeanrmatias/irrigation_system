# Sistema de Irrigação Automática (IoT)

Projeto do 2º semestre de Engenharia de Software (FIAP): Edge Computing and Computer Systems.

## Integrantes

- Matheus Marques De Souza
- Jean R. Matias

## O dispositivo

Um irrigador automático de vaso ou canteiro. O **Arduino UNO** mede a umidade do solo e a umidade e temperatura do ar e abre uma válvula (um servo motor) quando a planta precisa de água. O **ESP32** leva esses dados para a internet pela plataforma **FIWARE** (via MQTT) e traz de volta comandos do **dashboard**, como "regar agora" e "trocar de modo". É uma comunicação *machine to machine*: o dashboard manda e o dispositivo executa e confirma, sem ninguém no meio.

```
 Sensores ─► Arduino UNO ──UART (protocolo próprio)──► ESP32 ──MQTT──► FIWARE ──► Dashboard
  Servo  ◄──    (botões, LEDs)    ◄── comandos ────────     ◄──────────    ◄── botão "Regar"
```

| Parte | Pasta | O que faz |
|---|---|---|
| Firmware do Arduino | [`Arduino/`](Arduino) | Lê sensores, decide a rega, controla a válvula, botões e LEDs |
| Firmware do ESP32 | [`ESP32/`](ESP32) | Ponte UART ↔ MQTT (roda no simulador de ESP32 da disciplina) |
| FIWARE | [`FIWARE/`](FIWARE) | Script que cadastra o dispositivo no IoT Agent, no Orion e no STH-Comet |
| Dashboard | [`Dashboard/`](Dashboard) | Painel web com valores, histórico e controle remoto |
| Ferramentas | [`ferramentas/`](ferramentas) | Arduino falso (para testar sem a placa) e testes da lógica |

## Hardware e ligações

| Componente | Pino do Arduino |
|---|---|
| DHT11 (DATA) | D4 |
| Sensor de umidade do solo / nível de água (AO) | A0 |
| Servo (sinal), que faz o papel da válvula | D9 |
| Botão **REGAR** (entre o pino e o GND) | D2 |
| Botão **MODO** (entre o pino e o GND) | D3 |
| LED verde + resistor 220 Ω–1 kΩ | D5 |
| LED vermelho + resistor 220 Ω–1 kΩ | D6 |
| ESP32 | cabo USB (Serial, D0/D1) |

Os botões usam o pull-up interno, então não precisam de resistor. Para servos que puxam mais corrente, use uma fonte externa com o GND em comum.

## Funcionamento

A cada 2 s o Arduino lê os sensores e manda os dados ao ESP32.

**Modo AUTO** (padrão): a válvula abre por 3 s quando:

- o solo está **seco** (leitura < 100), **ou**
- o ar está muito seco (< 30 %) **e** o solo está abaixo do nível médio (< 300).

Entre uma rega automática e outra há uma espera mínima de 30 s. **Se o sensor de solo estiver com erro, a rega automática fica suspensa** por segurança.

**Modo MANUAL**: só rega quando alguém aperta o botão ou manda o comando pelo dashboard.

| Leitura do solo (0–1023, menor = mais seco) | Nível |
|---|---|
| < 100 | SECO |
| 100–299 | BAIXO |
| 300–499 | MÉDIO |
| ≥ 500 | ALTO |

### Interface do usuário

**Botões**

| Botão | Ação |
|---|---|
| REGAR | Abre a válvula por 3 s. Se ela já estiver aberta, **fecha na hora** (serve de "parar"). Funciona nos dois modos. |
| MODO | Alterna entre AUTO e MANUAL. |

**LEDs**

| LED | Como fica | Significado |
|---|---|---|
| Verde | aceso | modo AUTO |
| Verde | pisca devagar | modo MANUAL |
| Verde | pisca rápido | regando agora |
| Vermelho | aceso | **sem comunicação com o ESP32** |
| Vermelho | pisca rápido | falha de sensor (DHT11 ou solo) |
| Vermelho | pisca devagar | ESP32 OK, mas sem conexão com a nuvem (MQTT) |
| Vermelho | apagado | tudo certo |

### Tratamento de erros (requisitos 3.3)

| Requisito | Como foi feito |
|---|---|
| Temporização não-bloqueante | Tudo com `millis()`. O único `delay()` (no simulador do ESP32) tem justificativa no código. |
| Watchdog | `wdt_enable(WDTO_2S)`: se o `loop()` travar por 2 s, o Arduino reinicia e avisa no log. |
| Falha de sensor | DHT11: leitura `NaN` ou fora da faixa = erro. Solo: teste de fio solto com o pull-up interno. Os dois aparecem no LED, no protocolo e no dashboard. |
| Perda de comunicação | Os dois lados mandam um "batimento" a cada 2 s. 6 s sem nada = offline, mostrado no LED vermelho (Arduino), no log e no dashboard (ESP32). |
| Debounce | Não-bloqueante, 50 ms, em `src/botoes/`. |
| Funções, structs e ponteiros | Estado em structs (`src/tipos.h`), funções recebendo ponteiros, `Stream*` para a serial e ponteiro para função no tratador de comandos. |

## Protocolo de comunicação Arduino ↔ ESP32

- **Meio:** UART a 9600 baud, 8N1. O simulador do ESP32 abre a porta USB do Arduino.
- **Formato:** texto, uma mensagem por linha, inspirado no NMEA (GPS):

```
$<TIPO>,<campo1>,<campo2>,...*<CS>\n
```

| Parte | Significado |
|---|---|
| `$` | início do quadro |
| `TIPO` | uma letra (tabela abaixo) |
| `*CS` | checksum: XOR de todos os caracteres entre `$` e `*`, em 2 dígitos hexadecimais |
| `\n` | fim do quadro |

Um quadro com checksum errado é descartado. Qualquer coisa fora de um quadro é ignorada. Por isso o Arduino pode mandar linhas de log começando com `#` pela mesma serial: o ESP32 só as exibe no terminal.

| Tipo | Direção | Campos | Quando |
|---|---|---|---|
| `D` | Arduino → ESP32 | `seq, solo, umidAr×10, temp×10, válvula(0/1), modo(A/M), erros` | a cada 2 s (também é o "batimento" do Arduino) |
| `A` | Arduino → ESP32 | `COMANDO, OK\|ERRO` | resposta a cada `C` recebido |
| `S` | ESP32 → Arduino | `mqtt(0/1)` | a cada 2 s (batimento do ESP32 + estado da nuvem) |
| `C` | ESP32 → Arduino | `REGAR, segundos` (0 = fecha) ou `MODO, A\|M` | quando chega um comando da nuvem |

`erros` é um mapa de bits: `1` = falha no DHT11, `2` = sensor de solo desconectado (`3` = os dois).
Os valores com casa decimal vão multiplicados por 10, porque o `printf` do UNO não formata `float`: 24,3 °C vira `243`.

**Exemplos**

```
$D,42,350,553,243,0,A,0*1F     Arduino: solo 350, ar 55,3 %, 24,3 °C, válvula fechada, AUTO, sem erros
$S,1*4E                        ESP32: estou vivo e conectado ao MQTT
$C,REGAR,5*35                  ESP32: abra a válvula por 5 s
$A,REGAR,OK*06                 Arduino: feito
```

O mesmo código do protocolo (`protocolo.h/.cpp`) é usado nos dois lados, sem nenhuma dependência do Arduino.

## Dados enviados por MQTT (FIWARE, UltraLight 2.0)

| | |
|---|---|
| Broker | Mosquitto do FIWARE, porta 1883 |
| API key / device | `TEF` / `irrig001` |
| Entidade no Orion | `urn:ngsi-ld:Irrigacao:001` (tipo `Irrigacao`, service `smart`, path `/`) |
| Publica dados em | `/TEF/irrig001/attrs` |
| Recebe comandos em | `/TEF/irrig001/cmd` (payload `irrig001@<comando>\|<valor>`) |
| Responde comandos em | `/TEF/irrig001/cmdexe` (payload `irrig001@<comando>\|<resultado>`) |

Exemplo de publicação: `s|350|u|55.3|t|24.3|v|0|m|A|e|0|l|1`

| Chave UL | Atributo no Orion | Tipo | Significado |
|---|---|---|---|
| `s` | `umidadeSolo` | Integer | leitura bruta 0–1023 (não é enviada se o sensor de solo falhar) |
| `u` | `umidadeAr` | Number | % (não é enviada se o DHT11 falhar) |
| `t` | `temperatura` | Number | °C (não é enviada se o DHT11 falhar) |
| `v` | `valvula` | Integer | 1 = aberta |
| `m` | `modo` | Text | `A` ou `M` |
| `e` | `erros` | Integer | mapa de bits (ver protocolo) |
| `l` | `arduinoOnline` | Integer | 1 = ESP32 está recebendo dados do Arduino |

| Comando | Valor | Efeito | Respostas possíveis |
|---|---|---|---|
| `regar` | 0–30 (segundos) | abre a válvula (0 = fecha) | `OK`, `ERRO`, `ERRO_VALOR`, `ARDUINO_OFFLINE`, `SEM_RESPOSTA` |
| `definirModo` | `A` ou `M` | troca o modo | idem |

## Dashboard

```bash
python3 Dashboard/dashboard.py --fiware <IP_DO_FIWARE>     # ou --demo, sem FIWARE
```

Abra `http://localhost:8050`. Não precisa instalar nada, só ter Python 3.

- **Cartões:** umidade do solo (com o nível), umidade do ar, temperatura, válvula, modo e conexão Arduino ↔ ESP32.
- **Gráficos:** histórico do solo (com as linhas de "seco" e "médio") e do ar (umidade e temperatura). Os dados vêm do STH-Comet.
- **Controle remoto:** "Regar" por N segundos, "Parar", e AUTO/MANUAL. Embaixo de cada controle aparece a confirmação que voltou do dispositivo.
- **Avisos:** faixa vermelha ou laranja quando o Arduino está offline, quando um sensor falha ou quando não chegam dados novos há mais de 15 s.
- `--demo` mostra dados simulados. Serve para testar o painel sem o FIWARE (e de plano B na apresentação).

## Como rodar tudo

1. **FIWARE:** suba o docker-compose da disciplina e cadastre o dispositivo: `python3 FIWARE/provisionar.py --host <IP>`. Detalhes em [`FIWARE/README.md`](FIWARE/README.md).
2. **Arduino:** abra a pasta `Arduino/` na IDE do Arduino (ou no PlatformIO), instale *DHT sensor library*, *Adafruit Unified Sensor* e *Servo* e grave na placa.
3. **ESP32 (simulador, no Linux ou WSL2):** veja [`ESP32/README.md`](ESP32/README.md).
   ```bash
   cd ESP32 && cmake -S . -B build && cmake --build build
   MQTT_BROKER=<IP_DO_FIWARE> ./build/ESP32Mock
   ```
4. **Dashboard:** `python3 Dashboard/dashboard.py --fiware <IP>`.

**Sem o Arduino?** Rode `python3 ferramentas/arduino_fake.py`. Ele cria uma porta serial virtual que fala o mesmo protocolo, e você roda o ESP32 com `SERIAL_PORTA=<porta que ele mostrar>`.

## Testes

```bash
# lógica do protocolo e da irrigação, no computador
g++ -std=c++17 -Wall -I Arduino/src ferramentas/testes_host.cpp \
    Arduino/src/protocolo/protocolo.cpp Arduino/src/irrigacao/irrigacao.cpp -o /tmp/testes && /tmp/testes
```

## Licença

Projeto educacional, livre para uso e modificação. O simulador de ESP32 é do Prof. Guilherme Salati (ver `ESP32/licenses`). O Chart.js tem licença MIT (`Dashboard/vendor`).
