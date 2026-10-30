# Guia para entender o código

Um mapa para ler o projeto sem se perder. A ideia é ir por camadas, como descascar uma cebola: primeiro o "maestro", depois cada instrumento.

## Ordem de leitura sugerida

1. **`Arduino/Arduino.ino`**: o maestro. Só o `loop()` já conta a história inteira: *entradas → sensores → decisão → botões → saídas*.
2. **`Arduino/src/config.h`**: o painel de botões do forno. Todo número "mágico" mora aqui.
3. **`Arduino/src/tipos.h`**: as fichas (structs) com o estado do sistema.
4. **`Arduino/src/irrigacao/irrigacao.cpp`**: a regra de negócio. São só `if`s, é a parte mais fácil.
5. **`Arduino/src/valvula/` e `botoes/`**: como fazer as coisas "sem `delay`".
6. **`Arduino/src/protocolo/`**: como duas placas conversam sem se confundir.
7. **`ESP32/src/esp32.cpp`**: o tradutor entre o Arduino e a nuvem.
8. **`Dashboard/`**: a janela para o mundo.

## Conceitos de C++ e onde eles aparecem

| Conceito | Analogia | Onde ver |
|---|---|---|
| `#define` / constantes | Os botões do forno: você regula, a receita não muda | `config.h` |
| `struct` | Uma ficha cadastral: junta tudo de uma "coisa" | `tipos.h` (`Valvula`, `Botao`, `EstadoSistema`) |
| Ponteiro (`Valvula *v`) | Passar o **endereço** da casa em vez de construir uma cópia da casa. Quem recebe pode entrar e mudar os móveis | `valvulaAbrir(&estado.valvula, ...)` |
| `->` | "Entra na casa por esse endereço e pega o campo X" | `v->aberta = true;` |
| `const` no ponteiro | Endereço com placa de "só visitar, não mexer" | `irrigacaoPrecisaRegar(const EstadoSistema *estado, ...)` |
| Ponteiro para função | O garçom (link) recebe o pedido e entrega para a cozinha (a função) que você indicou | `TratadorComando` em `link_esp.h` |
| `Stream *` | Uma tomada universal: Serial, SoftwareSerial... qualquer um que "encaixe" | `linkIniciar(&estado.link, &LINK_SERIAL, ...)` |
| `static` (global num `.cpp`) | Cômodo trancado: só aquele arquivo entra | `static Servo servo;` em `valvula.cpp` |
| `.h` × `.cpp` | O `.h` é o cardápio (o que existe); o `.cpp` é a cozinha (como é feito) | todos os módulos |
| `#ifndef X_H` | O porteiro que não deixa a mesma pessoa entrar duas vezes | topo de todo `.h` |
| Bits (`erros \|= ERRO_DHT`) | Um painel de luzinhas: cada bit é uma luz de alerta independente | `sensores.cpp`, `tipos.h` |
| `F("texto")` | Guarda o texto no "porão" (flash) em vez da "sala" (RAM, só 2 KB no UNO) | `debug.h` |

## Como funciona "sem `delay`"

`delay(3000)` é ficar parado olhando o relógio: nesse tempo ninguém atende o botão nem o ESP32.

Com `millis()`, você anota a hora em que começou e, a cada volta do `loop()`, só pergunta *"já deu o tempo?"*:

```cpp
if (agora - v->instanteAbertura >= v->duracaoMs) fecharValvula();
```

É como pôr um bolo no forno e anotar a hora: você continua fazendo outras coisas e, de vez em quando, olha o relógio.

## Os "vigias" do sistema

| Vigia | Quem vigia | O que acontece |
|---|---|---|
| Watchdog (hardware) | o `loop()` do Arduino | travou por 2 s → o Arduino reinicia sozinho |
| Timeout do link (Arduino) | o ESP32 | 6 s sem `$S` → LED vermelho aceso |
| Timeout do link (ESP32) | o Arduino | 6 s sem `$D` → publica `arduinoOnline = 0` |
| Timeout de comando (ESP32) | a resposta `$A` | 5 s → responde `SEM_RESPOSTA` para o FIWARE |
| Checksum | cada mensagem | não confere → mensagem descartada |
| Dashboard | o `TimeInstant` do Orion | 15 s sem dados novos → aviso na tela |

## Caminho de um comando (M2M)

```
[Dashboard] botão Regar
   └─ PATCH /v2/entities/.../attrs {"regar": {"type":"command","value":"5"}}   (HTTP)
[Orion] → [IoT Agent UL]
   └─ publica "irrig001@regar|5" em /TEF/irrig001/cmd                           (MQTT)
[ESP32] mqtt_callback
   └─ envia "$C,REGAR,5*35"                                                      (UART)
[Arduino] tratarComando → valvulaAbrir → servo 90°
   └─ responde "$A,REGAR,OK*06"                                                  (UART)
[ESP32] tratarConfirmacao
   └─ publica "irrig001@regar|OK" em /TEF/irrig001/cmdexe                       (MQTT)
[IoT Agent] → [Orion] regar_status = OK  → [Dashboard] "Último comando: confirmado"
```
