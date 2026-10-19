# ESP32: ponte UART ↔ MQTT (simulador)

Este é o simulador **ESP32_mqtt_mock** do Prof. Guilherme Salati
([repositório original](https://github.com/gsalati/Edge-Computing-Computer-Systems/tree/main/ESP32_mqtt_mock)).
Nosso firmware está em **`src/esp32.cpp`**, e o protocolo em `src/protocolo/` é o mesmo do Arduino.

Ele roda no **Linux ou no WSL2** e conversa com o Arduino real pela porta USB.

## Compilar

```bash
sudo apt install cmake build-essential libpaho-mqtt-dev libpaho-mqttpp-dev
cd ESP32
cmake -S . -B build
cmake --build build
```

## Rodar

```bash
./build/ESP32Mock                                   # Arduino em /dev/ttyACM0, broker em localhost
SERIAL_PORTA=/dev/ttyUSB0 ./build/ESP32Mock         # outra porta
MQTT_BROKER=20.30.40.50 ./build/ESP32Mock           # FIWARE em outra máquina
```

| Variável | Padrão | Para quê |
|---|---|---|
| `SERIAL_PORTA` | `/dev/ttyACM0` | porta do Arduino (`ls /dev/ttyACM* /dev/ttyUSB*`) |
| `MQTT_BROKER` | `localhost` | IP do Mosquitto do FIWARE |
| `MQTT_PORTA` | `1883` | porta do broker |

Se der *permission denied* na porta: `sudo usermod -aG dialout $USER` (e reabra o terminal) ou rode com `sudo -E`.

O terminal mostra tudo o que acontece: dados do Arduino, publicações MQTT, comandos e as linhas de log do Arduino (`[Arduino] ...`).

### Windows (WSL2): passar o Arduino para o Linux

No PowerShell **como administrador**:

```powershell
winget install usbipd
usbipd list                              # anote o BUSID do Arduino
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

Para **gravar** o Arduino pela IDE no Windows, desanexe antes (`usbipd detach --busid <BUSID>`) e anexe de novo depois.

## Testar sem o Arduino

**Opção 1 (recomendada):** use o Arduino falso, que fala o protocolo completo:

```bash
python3 ../ferramentas/arduino_fake.py       # mostra: SERIAL_PORTA=/dev/pts/N
SERIAL_PORTA=/dev/pts/N ./build/ESP32Mock    # em outro terminal
```

Teclas do arduino_fake: `r` regar, `m` modo, `d` falha DHT, `s` falha solo, `c` cortar o cabo, `q` sair.

**Opção 2:** a `SerialMock` do professor. Você digita os quadros no teclado:

```bash
cmake -S . -B build_mock -DSERIAL_MOCK=ON && cmake --build build_mock
./build_mock/ESP32Mock
# digite: $D,1,350,553,243,0,A,0*28   (Enter)
```

## O que o firmware faz

| Função | O que faz |
|---|---|
| `lerSerialArduino` | junta os bytes, valida os quadros `$...*CS` e mostra os logs `#` do Arduino |
| `tratarDados` | `$D` → payload UltraLight `s\|..\|u\|..\|t\|..` em `/TEF/irrig001/attrs` (só manda sensores sem erro) |
| `mqtt_callback` | `irrig001@regar\|5` → `$C,REGAR,5` para o Arduino |
| `tratarConfirmacao` | `$A,REGAR,OK` → `irrig001@regar\|OK` em `/TEF/irrig001/cmdexe` |
| `manterMqtt` | reconecta ao broker a cada 5 s se cair (e assina de novo os comandos) |
| `vigiarArduino` | 6 s sem `$D` → publica `l\|0` (Arduino offline) |
| `vigiarComandos` | 5 s sem `$A` → responde `SEM_RESPOSTA` |
| `enviarStatus` | `$S,<mqtt>` a cada 2 s (o Arduino usa isso para saber que o ESP32 está vivo) |

## Alterações feitas no simulador do professor

Todas estão marcadas com `[ALTERADO]` no código:

1. **`mqtt.cpp` → `subscribe()`** assinava sempre `/TEF/exemplo001/cmd`, ignorando o tópico passado. Sem essa correção, nenhum comando para `irrig001` chegaria.
2. **`mqtt.cpp`**: `publish`, `subscribe` e `disconnect` davam *crash* se o `connect()` nunca tivesse funcionado (ponteiro nulo). O `connect()` também ganhou um timeout de 2 s, para não travar 30 s com o broker fora do ar.
3. **`serial.cpp` / `serialMock.cpp` → `readBytes()`**: o laço `i <= length` lia 1 byte a mais que o pedido (risco de estourar o buffer).
4. **`serialMock.cpp`**: no modo texto (`'s'`), qualquer caractere que não fosse hexadecimal era rejeitado, e o Enter não virava `\n`. Agora o modo texto funciona.
5. **`serialMock.cpp`**: quando a entrada do teclado terminava (Ctrl+D), o laço reenviava a última linha para sempre.

O `CMakeLists.txt` ganhou o `src/protocolo/` e a opção `-DSERIAL_MOCK=ON`.
