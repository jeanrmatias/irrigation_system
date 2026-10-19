/**
 * @file Arduino.ino
 * @brief Sistema de Irrigação Automática - firmware do Arduino UNO.
 *
 * Integrantes: Matheus Marques De Souza, Jean R. Matias
 * Disciplina: Edge Computing and Computer Systems (FIAP)
 *
 * Este arquivo é o "maestro": ele não toca nenhum instrumento sozinho,
 * só diz quem toca e quando. Cada instrumento está em src/:
 *
 *   src/config.h          pinos, tempos e limites
 *   src/tipos.h           as structs (as "fichas" com o estado)
 *   src/sensores/         DHT11 + solo, com validação
 *   src/valvula/          servo/válvula sem delay
 *   src/botoes/           botões com debounce
 *   src/irrigacao/        regra de decisão do modo AUTO
 *   src/interface/        LEDs (e gancho para display)
 *   src/link/             conversa com o ESP32
 *   src/protocolo/        formato dos quadros (igual no ESP32)
 *
 * Como compilar: abra ESTA pasta (Arduino/) na IDE do Arduino, ou use o
 * PlatformIO com o platformio.ini desta pasta.
 * Bibliotecas: "DHT sensor library" (Adafruit) + "Adafruit Unified Sensor" + "Servo".
 */
#include <avr/wdt.h>

#include "src/config.h"
#include "src/tipos.h"
#include "src/debug.h"
#include "src/sensores/sensores.h"
#include "src/valvula/valvula.h"
#include "src/botoes/botoes.h"
#include "src/irrigacao/irrigacao.h"
#include "src/interface/interface.h"
#include "src/link/link_esp.h"

// A ficha mestre com TODO o estado do sistema.
static EstadoSistema estado;
static Botao botaoRegar;
static Botao botaoModo;

static unsigned long ultimaLeitura = 0;
static unsigned long ultimoEnvio = 0;

// Ações (usadas pelos botões e pelos comandos da nuvem)
static void definirModo(ModoOperacao novo) {
  if (estado.modo == novo) return;
  estado.modo = novo;
  debugLog(novo == MODO_AUTO ? F("Modo: AUTO") : F("Modo: MANUAL"));
}

// Chamada pelo link quando chega um $C do ESP32 (ponteiro para função).
static bool tratarComando(const ProtoQuadro *q) {
  if (protoCampoIgual(q, 0, "REGAR")) {
    long segundos = 0;
    if (!protoCampoInt(q, 1, &segundos) || segundos < 0 || segundos > REGA_MAX_S) {
      return false;
    }
    debugLog(F("Comando da nuvem: REGAR"));
    valvulaAbrir(&estado.valvula, (uint16_t)segundos, millis());
    return true;
  }

  if (protoCampoIgual(q, 0, "MODO")) {
    if (protoCampoIgual(q, 1, "A")) { definirModo(MODO_AUTO);   return true; }
    if (protoCampoIgual(q, 1, "M")) { definirModo(MODO_MANUAL); return true; }
    return false;
  }

  return false;  // comando desconhecido
}

// ------------------------------------------------------------------
void setup() {
  // Guarda o motivo do último reset e desliga o watchdog enquanto inicia.
  uint8_t motivoReset = MCUSR;
  MCUSR = 0;
  wdt_disable();

  LINK_SERIAL.begin(LINK_BAUD);

  estado.modo = MODO_AUTO;
  estado.erros = 0;
  estado.leitura.jaLeu = false;
  estado.leitura.dhtOk = false;
  estado.leitura.soloOk = false;

  sensoresIniciar();
  valvulaIniciar(&estado.valvula);
  botaoIniciar(&botaoRegar, PINO_BTN_REGAR);
  botaoIniciar(&botaoModo, PINO_BTN_MODO);
  interfaceIniciar();
  // &LINK_SERIAL = endereço do objeto Serial. tratarComando = endereço da função.
  linkIniciar(&estado.link, &LINK_SERIAL, tratarComando);

  if (motivoReset & _BV(WDRF)) {
    debugLog(F("ATENCAO: reiniciado pelo WATCHDOG (o loop travou)"));
  }
  debugLog(F("Sistema de irrigacao iniciado"));

  // Watchdog: se o loop() travar por mais de 2 s, o Arduino reinicia sozinho.
  // Analogia: é o "homem-morto" do trem. Se o maquinista parar de apertar
  // o pedal (wdt_reset), o trem freia (reset).
  wdt_enable(WDTO_2S);
}

// ------------------------------------------------------------------
void loop() {
  wdt_reset();                       // "estou vivo"
  unsigned long agora = millis();

  // 1) ENTRADAS: botões e mensagens do ESP32
  botaoAtualizar(&botaoRegar, agora);
  botaoAtualizar(&botaoModo, agora);
  linkProcessarEntrada(&estado.link, agora);
  linkVerificarTimeout(&estado.link, agora);

  // 2) SENSORES: só a cada INTERVALO_LEITURA_MS
  if (agora - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = agora;
    uint8_t errosAntes = estado.erros;

    sensoresLer(&estado.leitura);
    estado.erros = sensoresErros(&estado.leitura);

    if (estado.erros != errosAntes) {
      if (estado.erros & ERRO_DHT)  debugLog(F("ERRO: falha na leitura do DHT11"));
      if (estado.erros & ERRO_SOLO) debugLog(F("ERRO: sensor de solo desconectado"));
      if (estado.erros == 0)        debugLog(F("Sensores OK"));
    }

    // 3) DECISÃO automática (logo depois de ler)
    if (irrigacaoPrecisaRegar(&estado, agora)) {
      debugLogValor(F("AUTO: solo pedindo agua. Leitura = "), estado.leitura.soloBruto);
      valvulaAbrir(&estado.valvula, REGA_PADRAO_S, agora);
    }
  }

  // 4) BOTÕES
  if (botaoFoiPressionado(&botaoRegar)) {
    // Se já está regando, o botão serve de "parar"; se não, rega agora.
    if (estado.valvula.aberta) {
      debugLog(F("Botao REGAR: parando a rega"));
      valvulaFechar(&estado.valvula);
    } else {
      debugLog(F("Botao REGAR: rega manual"));
      valvulaAbrir(&estado.valvula, REGA_PADRAO_S, agora);
    }
  }
  if (botaoFoiPressionado(&botaoModo)) {
    definirModo(estado.modo == MODO_AUTO ? MODO_MANUAL : MODO_AUTO);
  }

  // 5) SAÍDAS
  valvulaAtualizar(&estado.valvula, agora);

  if (estado.leitura.jaLeu && (agora - ultimoEnvio >= INTERVALO_ENVIO_MS)) {
    ultimoEnvio = agora;
    linkEnviarDados(&estado);
  }

  interfaceAtualizar(&estado, agora);
}
