/**
 * @file esp32.cpp
 * @brief Firmware do ESP32 (rodando no simulador ESP32_mqtt_mock do professor).
 *
 * Integrantes: Matheus Marques De Souza, Jean R. Matias
 *
 * O ESP32 é uma PONTE (gateway) machine-to-machine:
 *
 *   Arduino --UART/protocolo--> ESP32 --MQTT/UltraLight--> FIWARE (IoT Agent -> Orion)
 *   Arduino <--UART/protocolo-- ESP32 <--MQTT/UltraLight-- FIWARE (comandos do dashboard)
 *
 * Ele não decide nada sobre irrigação: só traduz de um "idioma" para o outro
 * e vigia se os dois lados estão vivos.
 *
 * Configuração por variáveis de ambiente (não precisa recompilar):
 *   SERIAL_PORTA  porta do Arduino     (padrão /dev/ttyACM0)
 *   MQTT_BROKER   endereço do broker   (padrão localhost)
 *   MQTT_PORTA    porta do broker      (padrão 1883)
 *
 * Exemplo:  SERIAL_PORTA=/dev/ttyUSB0 MQTT_BROKER=192.168.0.10 ./build/ESP32Mock
 */
#include "esp32.h"
#include "protocolo/protocolo.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// ======================= CONFIGURAÇÃO FIWARE =======================
// Precisam bater com o que o FIWARE/provisionar.py cadastra.
static const char *APIKEY    = "TEF";
static const char *DEVICE_ID = "irrig001";

// ======================= TEMPOS (ms) =======================
static const unsigned long INTERVALO_STATUS_MS       = 2000;  // $S para o Arduino
static const unsigned long TIMEOUT_ARDUINO_MS        = 6000;  // sem $D = Arduino offline
static const unsigned long INTERVALO_RECONEXAO_MS    = 5000;  // tenta MQTT de novo
static const unsigned long TIMEOUT_RESPOSTA_CMD_MS   = 5000;  // espera o $A do Arduino
static const unsigned long INTERVALO_REENVIO_LINK_MS = 30000; // republica "l" periodicamente

// ======================= OBJETOS =======================
#ifdef USAR_SERIAL_MOCK
// Sem Arduino: você digita os quadros no terminal (modo texto).
SerialMock Serial;
#else
// Com Arduino (ou com o ferramentas/arduino_fake.py) numa porta serial.
SerialCom Serial;
#endif

ArduinoMQTT MQTT;

// ======================= ESTADO DA PONTE =======================
struct EstadoPonte {
  bool mqttOnline = false;
  bool arduinoOnline = false;
  unsigned long ultimoQuadroArduino = 0;
  unsigned long ultimoStatusEnviado = 0;
  unsigned long ultimaTentativaMqtt = 0;
  unsigned long ultimoLinkPublicado = 0;
  bool primeiraTentativaMqtt = true;

  // Comandos da nuvem esperando a confirmação ($A) do Arduino (um por tipo).
  // Analogia: a senha de espera do restaurante. Cada pedido tem a sua, então
  // a resposta do "REGAR" nunca é entregue para quem pediu "MODO".
  struct Pendente {
    const char *nomeArduino;   // como o Arduino chama: "REGAR"
    const char *nomeNuvem;     // como o FIWARE chama: "regar"
    bool esperando;
    unsigned long desde;
  } pendentes[2] = {
    {"REGAR", "regar", false, 0},
    {"MODO", "definirModo", false, 0},
  };

  unsigned long quadrosRecebidos = 0;
  unsigned long publicacoes = 0;
};

static EstadoPonte ponte;
static ProtoParser parser;

// Linhas de log do Arduino ("# ...") são juntadas aqui para mostrar no terminal.
static std::string linhaLog;

// Tópicos montados no setup()
static char topicoAttrs[64];
static char topicoCmd[64];
static char topicoCmdExe[64];

// ======================= FUNÇÕES AUXILIARES =======================
static const char *envOu(const char *nome, const char *padrao) {
  const char *v = getenv(nome);
  return (v != nullptr && *v != '\0') ? v : padrao;
}

static void logar(const char *tag, const std::string &msg) {
  unsigned long t = millis();
  printf("[%6lu.%03lu] %-8s %s\n", t / 1000, t % 1000, tag, msg.c_str());
  fflush(stdout);
}

// Envia "$<conteudo>*CS\n" para o Arduino.
static void enviarParaArduino(const char *conteudo) {
  char quadro[PROTO_TAM_MAX + 6];
  size_t tam = protoMontar(quadro, sizeof(quadro), conteudo);
  if (tam == 0) return;
  Serial.writeBytes(quadro, (int)tam);
}

static bool publicar(const char *topico, const std::string &payload) {
  if (!ponte.mqttOnline) return false;
  bool ok = MQTT.publish(topico, payload.c_str());
  if (ok) {
    ponte.publicacoes++;
  } else {
    // Publicação falhou: provavelmente o broker caiu. O loop reconecta.
    ponte.mqttOnline = false;
    logar("MQTT", "falha ao publicar, vou reconectar");
  }
  return ok;
}

// Avisa o FIWARE se o Arduino está conectado (atributo "l" = arduinoOnline).
static void publicarLink(unsigned long agora) {
  std::string payload = std::string("l|") + (ponte.arduinoOnline ? "1" : "0");
  if (publicar(topicoAttrs, payload)) {
    ponte.ultimoLinkPublicado = agora;
  }
}

// Responde ao IoT Agent o resultado de um comando: "<device>@<comando>|<resultado>"
static void responderComandoNuvem(const char *comando, const char *resultado) {
  std::string payload = std::string(DEVICE_ID) + "@" + comando + "|" + resultado;
  publicar(topicoCmdExe, payload);
  logar("NUVEM<-", std::string("resultado do comando: ") + payload);
}

// ======================= ARDUINO -> NUVEM =======================
// $D,<seq>,<solo>,<umid x10>,<temp x10>,<valvula>,<modo>,<erros>
static void tratarDados(const ProtoQuadro *q, unsigned long agora) {
  long seq, solo, umid, temp, valvula, erros;
  if (q->numCampos != 7 ||
      !protoCampoInt(q, 0, &seq) || !protoCampoInt(q, 1, &solo) ||
      !protoCampoInt(q, 2, &umid) || !protoCampoInt(q, 3, &temp) ||
      !protoCampoInt(q, 4, &valvula) || !protoCampoInt(q, 6, &erros)) {
    logar("ARDUINO", "quadro $D com campos invalidos (ignorado)");
    return;
  }
  const char *modo = q->campos[5];

  const long ERRO_DHT = 0x01, ERRO_SOLO = 0x02;   // mesmos bits do Arduino (tipos.h)

  // Monta o payload UltraLight 2.0: "chave|valor|chave|valor..."
  // Só manda o valor de um sensor se ele estiver OK, para o gráfico do
  // dashboard não ganhar zeros falsos quando um sensor falha.
  char buf[160];
  std::string ul;
  if (!(erros & ERRO_SOLO)) {
    snprintf(buf, sizeof(buf), "s|%ld|", solo);
    ul += buf;
  }
  if (!(erros & ERRO_DHT)) {
    snprintf(buf, sizeof(buf), "u|%.1f|t|%.1f|", umid / 10.0, temp / 10.0);
    ul += buf;
  }
  snprintf(buf, sizeof(buf), "v|%ld|m|%s|e|%ld|l|1", valvula, modo, erros);
  ul += buf;

  logar("ARDUINO", "dados #" + std::to_string(seq) + " -> " + ul);
  if (publicar(topicoAttrs, ul)) {
    ponte.ultimoLinkPublicado = agora;      // o "l|1" já foi junto
  }
}

// $A,<COMANDO>,<OK|ERRO>
static EstadoPonte::Pendente *buscarPendente(const char *nomeArduino) {
  for (auto &p : ponte.pendentes) {
    if (strcmp(p.nomeArduino, nomeArduino) == 0) return &p;
  }
  return nullptr;
}

static void tratarConfirmacao(const ProtoQuadro *q) {
  if (q->numCampos < 2) return;
  logar("ARDUINO", std::string("confirmou ") + q->campos[0] + " = " + q->campos[1]);
  EstadoPonte::Pendente *p = buscarPendente(q->campos[0]);
  if (p != nullptr && p->esperando) {
    p->esperando = false;
    responderComandoNuvem(p->nomeNuvem, q->campos[1]);
  }
}

static void tratarQuadroArduino(const ProtoQuadro *q, unsigned long agora) {
  ponte.quadrosRecebidos++;
  ponte.ultimoQuadroArduino = agora;
  if (!ponte.arduinoOnline) {
    ponte.arduinoOnline = true;
    logar("ARDUINO", "ONLINE");
    publicarLink(agora);
  }

  switch (q->tipo) {
    case 'D': tratarDados(q, agora); break;
    case 'A': tratarConfirmacao(q); break;
    default:  logar("ARDUINO", std::string("tipo desconhecido: ") + q->tipo); break;
  }
}

static void lerSerialArduino(unsigned long agora) {
  char buffer[256];
  int disponivel = Serial.available();
  if (disponivel <= 0) return;
  if (disponivel > (int)sizeof(buffer)) disponivel = sizeof(buffer);

  int lidos = Serial.readBytes(buffer, disponivel);
  for (int i = 0; i < lidos; i++) {
    char c = buffer[i];

    // 1) Protocolo
    ProtoQuadro quadro;
    if (protoReceberByte(&parser, c, &quadro)) {
      tratarQuadroArduino(&quadro, agora);
    }

    // 2) Linhas de log "# ..." do Arduino (só para mostrar no terminal)
    if (c == '\n' || c == '\r') {
      if (!linhaLog.empty() && linhaLog[0] == '#') {
        logar("[Arduino]", linhaLog.substr(1));
      }
      linhaLog.clear();
    } else if (linhaLog.size() < 120) {
      linhaLog += c;
    }
  }
}

// ======================= NUVEM -> ARDUINO =======================
// O IoT Agent manda em /TEF/irrig001/cmd o payload "irrig001@regar|5"
void mqtt_callback(const char *topic, uint8_t *payload, unsigned int length) {
  std::string msg((char *)payload, length);
  logar("NUVEM->", std::string(topic) + " | " + msg);

  size_t arroba = msg.find('@');
  size_t barra = msg.find('|');
  if (arroba == std::string::npos || barra == std::string::npos || barra < arroba) {
    logar("NUVEM->", "formato de comando invalido");
    return;
  }
  std::string comando = msg.substr(arroba + 1, barra - arroba - 1);
  std::string valor = msg.substr(barra + 1);

  // Traduz do "idioma FIWARE" para o "idioma do nosso protocolo"
  std::string conteudo;
  const char *nomeArduino = nullptr;
  if (comando == "regar") {
    char *fim = nullptr;
    long segundos = valor.empty() ? 3 : strtol(valor.c_str(), &fim, 10);
    if ((fim != nullptr && *fim != '\0') || segundos < 0 || segundos > 30) {
      responderComandoNuvem(comando.c_str(), "ERRO_VALOR");
      return;
    }
    conteudo = "C,REGAR," + std::to_string(segundos);
    nomeArduino = "REGAR";
  } else if (comando == "definirModo") {
    // aceita "A", "M", "AUTO", "MANUAL" (maiúsculo ou minúsculo)
    char letra = valor.empty() ? '?' : (char)toupper((unsigned char)valor[0]);
    if (letra != 'A' && letra != 'M') {
      responderComandoNuvem(comando.c_str(), "ERRO_VALOR");
      return;
    }
    conteudo = std::string("C,MODO,") + letra;
    nomeArduino = "MODO";
  } else {
    responderComandoNuvem(comando.c_str(), "DESCONHECIDO");
    return;
  }

  if (!ponte.arduinoOnline) {
    responderComandoNuvem(comando.c_str(), "ARDUINO_OFFLINE");
    return;
  }

  EstadoPonte::Pendente *p = buscarPendente(nomeArduino);
  if (p->esperando) {
    // Já tinha um igual esperando resposta: o novo substitui o antigo.
    responderComandoNuvem(p->nomeNuvem, "SUBSTITUIDO");
  }
  enviarParaArduino(conteudo.c_str());
  p->esperando = true;
  p->desde = millis();
  logar("ARDUINO<-", "$" + conteudo);
}

// ======================= VIGIAS (watchdogs lógicos) =======================
static void manterMqtt(unsigned long agora) {
  if (ponte.mqttOnline && !MQTT.connected()) {
    ponte.mqttOnline = false;
    logar("MQTT", "conexao perdida");
  }
  if (ponte.mqttOnline) return;

  bool horaDeTentar = ponte.primeiraTentativaMqtt ||
                      (agora - ponte.ultimaTentativaMqtt >= INTERVALO_RECONEXAO_MS);
  if (!horaDeTentar) return;
  ponte.primeiraTentativaMqtt = false;
  ponte.ultimaTentativaMqtt = agora;

  // O ID do cliente MQTT precisa ser único no broker.
  std::string clienteId = std::string("esp32_") + DEVICE_ID;
  // Atenção: connect() espera a resposta do broker (até 2 s, ver mqtt.cpp).
  // No ESP32 real isso também acontece; por isso só tentamos a cada 5 s.
  if (MQTT.connect(clienteId.c_str()) && MQTT.subscribe(topicoCmd)) {
    ponte.mqttOnline = true;
    logar("MQTT", std::string("conectado. Assinando ") + topicoCmd);
    publicarLink(agora);
  } else {
    logar("MQTT", "broker indisponivel, tento de novo em 5 s");
  }
}

static void vigiarArduino(unsigned long agora) {
  if (ponte.arduinoOnline && (agora - ponte.ultimoQuadroArduino > TIMEOUT_ARDUINO_MS)) {
    ponte.arduinoOnline = false;
    logar("ARDUINO", "OFFLINE (sem dados ha 6 s)");
    publicarLink(agora);
  }
  // Com o Arduino offline, ninguém está mandando "l|1"; reforça o "l|0" de tempos em tempos.
  if (!ponte.arduinoOnline && ponte.mqttOnline &&
      agora - ponte.ultimoLinkPublicado >= INTERVALO_REENVIO_LINK_MS) {
    publicarLink(agora);
  }
}

static void vigiarComandos(void) {
  // Usa um millis() novo: o comando pode ter chegado (no MQTT.loop) DEPOIS do
  // 'agora' deste loop. Com unsigned long, "agora - desde" com agora < desde
  // dá um número gigante e o comando expiraria na hora.
  unsigned long agora = millis();
  for (auto &p : ponte.pendentes) {
    if (p.esperando && (agora - p.desde > TIMEOUT_RESPOSTA_CMD_MS)) {
      p.esperando = false;
      responderComandoNuvem(p.nomeNuvem, "SEM_RESPOSTA");
    }
  }
}

static void enviarStatus(unsigned long agora) {
  if (agora - ponte.ultimoStatusEnviado < INTERVALO_STATUS_MS) return;
  ponte.ultimoStatusEnviado = agora;
  enviarParaArduino(ponte.mqttOnline ? "S,1" : "S,0");
}

// ======================= SETUP / LOOP =======================
void setup() {
  const char *broker = envOu("MQTT_BROKER", "localhost");
  int porta = atoi(envOu("MQTT_PORTA", "1883"));

  snprintf(topicoAttrs, sizeof(topicoAttrs), "/%s/%s/attrs", APIKEY, DEVICE_ID);
  snprintf(topicoCmd, sizeof(topicoCmd), "/%s/%s/cmd", APIKEY, DEVICE_ID);
  snprintf(topicoCmdExe, sizeof(topicoCmdExe), "/%s/%s/cmdexe", APIKEY, DEVICE_ID);

  protoIniciar(&parser);

  printf("==============================================\n");
  printf(" ESP32 (simulado) - Ponte Irrigacao <-> FIWARE\n");
  printf(" Broker: %s:%d   Device: %s   ApiKey: %s\n", broker, porta, DEVICE_ID, APIKEY);
  printf(" Publica em: %s\n", topicoAttrs);
  printf(" Comandos:   %s\n", topicoCmd);
  printf("==============================================\n");

#ifdef USAR_SERIAL_MOCK
  Serial.begin('s');   // modo texto: digite quadros como $D,1,450,553,243,0,A,0*XX
#else
  const char *portaSerial = envOu("SERIAL_PORTA", "/dev/ttyACM0");
  if (access(portaSerial, F_OK) != 0) {
    printf("\n[ERRO] Porta serial %s nao existe.\n", portaSerial);
    printf("  - Arduino na USB?  Veja a porta com:  ls /dev/ttyACM* /dev/ttyUSB*\n");
    printf("  - No WSL2, anexe o Arduino com o usbipd (ver ESP32/README.md).\n");
    printf("  - Sem Arduino? Rode ferramentas/arduino_fake.py e use a porta que ele mostrar:\n");
    printf("      SERIAL_PORTA=/dev/pts/N ./build/ESP32Mock\n\n");
    exit(1);
  }
  Serial.setPortName(portaSerial);
  Serial.begin(9600);
#endif

  MQTT.setServer(broker, (uint16_t)porta);
  MQTT.setCallback(mqtt_callback);
  manterMqtt(millis());
}

void loop() {
  unsigned long agora = millis();

  lerSerialArduino(agora);   // Arduino -> nuvem
  MQTT.loop();               // nuvem -> Arduino (chama mqtt_callback)

  manterMqtt(agora);
  vigiarArduino(agora);
  vigiarComandos();
  enviarStatus(agora);

  // delay justificado: este "ESP32" é um programa no PC. Sem uma pausa curta
  // o while(true) do main.cpp usaria 100% de um núcleo da CPU à toa.
  // 10 ms é bem menor que qualquer tempo do sistema (o menor é 2 s).
  delay(10);
}
