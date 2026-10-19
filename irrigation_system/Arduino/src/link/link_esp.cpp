/**
 * @file link_esp.cpp
 */
#include "link_esp.h"

#include "../config.h"
#include "../debug.h"

#define MAX_BYTES_POR_LOOP 64   // lê no máximo isso por volta do loop (não trava)

static Stream *portaEsp = NULL;           // ponteiro: pode ser Serial, SoftwareSerial...
static TratadorComando tratadorComando = NULL;
static ProtoParser parser;

static void enviarConteudo(const char *conteudo) {
  char quadro[PROTO_TAM_MAX + 6];
  size_t tam = protoMontar(quadro, sizeof(quadro), conteudo);
  if (tam > 0 && portaEsp != NULL) {
    portaEsp->write((const uint8_t *)quadro, tam);
  }
}

static void responderComando(const ProtoQuadro *q, bool ok) {
  char conteudo[32];
  const char *nome = (q->numCampos > 0) ? q->campos[0] : "?";
  snprintf(conteudo, sizeof(conteudo), "A,%s,%s", nome, ok ? "OK" : "ERRO");
  enviarConteudo(conteudo);
}

static void tratarQuadro(EstadoLink *link, const ProtoQuadro *q) {
  switch (q->tipo) {
    case 'S': {   // status do ESP32
      long mqtt = 0;
      if (protoCampoInt(q, 0, &mqtt)) {
        bool novo = (mqtt == 1);
        if (novo != link->nuvemOnline) {
          debugLog(novo ? F("Nuvem (MQTT) ONLINE") : F("Nuvem (MQTT) OFFLINE"));
        }
        link->nuvemOnline = novo;
      }
      break;
    }
    case 'C': {   // comando vindo da nuvem
      bool ok = (tratadorComando != NULL) && tratadorComando(q);
      responderComando(q, ok);
      break;
    }
    default:
      break;      // tipo desconhecido: ignora
  }
}

void linkIniciar(EstadoLink *link, Stream *porta, TratadorComando tratador) {
  portaEsp = porta;
  tratadorComando = tratador;
  protoIniciar(&parser);
  link->online = false;
  link->nuvemOnline = false;
  link->instanteUltimoQuadro = 0;
  link->seq = 0;
}

void linkProcessarEntrada(EstadoLink *link, unsigned long agora) {
  ProtoQuadro quadro;
  uint8_t lidos = 0;

  while (portaEsp->available() > 0 && lidos < MAX_BYTES_POR_LOOP) {
    char c = (char)portaEsp->read();
    lidos++;
    if (protoReceberByte(&parser, c, &quadro)) {
      if (!link->online) debugLog(F("ESP32 ONLINE"));
      link->online = true;
      link->instanteUltimoQuadro = agora;
      tratarQuadro(link, &quadro);
    }
  }
}

void linkVerificarTimeout(EstadoLink *link, unsigned long agora) {
  if (link->online && (agora - link->instanteUltimoQuadro) > TIMEOUT_ESP_MS) {
    link->online = false;
    link->nuvemOnline = false;
    debugLog(F("ERRO: sem comunicacao com o ESP32"));
  }
}

// float -> inteiro x10 com arredondamento (24.26 -> 243)
static int x10(float valor) {
  return (int)(valor * 10.0f + (valor >= 0 ? 0.5f : -0.5f));
}

void linkEnviarDados(EstadoSistema *estado) {
  char conteudo[PROTO_TAM_MAX];
  const LeituraSensores *l = &estado->leitura;

  estado->link.seq++;
  snprintf(conteudo, sizeof(conteudo), "D,%u,%d,%d,%d,%d,%c,%u",
           estado->link.seq,
           l->soloBruto,
           x10(l->umidadeAr),
           x10(l->temperatura),
           estado->valvula.aberta ? 1 : 0,
           (char)estado->modo,
           estado->erros);
  enviarConteudo(conteudo);
}
