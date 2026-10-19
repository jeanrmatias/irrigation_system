/**
 * @file tipos.h
 * @brief As structs do sistema.
 *
 * Analogia: uma struct é uma ficha cadastral. Em vez de ter 10 variáveis
 * soltas pela casa, juntamos tudo que pertence à mesma "coisa" numa ficha só,
 * e passamos o ENDEREÇO da ficha (ponteiro) para quem precisa ler ou mudar.
 */
#ifndef TIPOS_H
#define TIPOS_H

#include <stdint.h>

// Modo de operação. O valor de cada um é a letra enviada no protocolo.
typedef enum {
  MODO_AUTO   = 'A',   // o próprio Arduino decide quando regar
  MODO_MANUAL = 'M'    // só rega quando alguém manda (botão ou nuvem)
} ModoOperacao;

// Bits de erro (podem acontecer ao mesmo tempo, por isso são bits).
#define ERRO_DHT   0x01   // bit 0: falha no DHT11
#define ERRO_SOLO  0x02   // bit 1: sensor de solo desconectado

/** Última leitura dos sensores. */
typedef struct {
  int   soloBruto;        // 0..1023 (menor = mais seco)
  float umidadeAr;        // %
  float temperatura;      // °C
  bool  dhtOk;            // a leitura do DHT é confiável?
  bool  soloOk;           // a leitura do solo é confiável?
  bool  jaLeu;            // já fizemos pelo menos uma leitura?
} LeituraSensores;

/** Um botão físico com debounce. */
typedef struct {
  uint8_t       pino;
  bool          estadoEstavel;     // true = pressionado (já sem tremedeira)
  bool          ultimaLeituraBruta;
  unsigned long instanteMudanca;   // quando a leitura bruta mudou pela última vez
  bool          eventoPressionado; // "campainha": vira true no momento do clique
} Botao;

/** A válvula (servo). */
typedef struct {
  bool          aberta;
  unsigned long instanteAbertura;
  unsigned long duracaoMs;
  unsigned long instanteUltimaRega;
  bool          jaRegou;           // para a 1ª rega não esperar o intervalo mínimo
  uint16_t      totalRegas;
} Valvula;

/** Estado da conversa com o ESP32. */
typedef struct {
  bool          online;            // recebemos algo válido nos últimos TIMEOUT_ESP_MS?
  bool          nuvemOnline;       // o ESP32 disse que o MQTT está conectado?
  unsigned long instanteUltimoQuadro;
  uint16_t      seq;               // número de sequência dos quadros $D
} EstadoLink;

/** A "ficha mestre": tudo o que o sistema sabe agora. */
typedef struct {
  LeituraSensores leitura;
  Valvula         valvula;
  EstadoLink      link;
  ModoOperacao    modo;
  uint8_t         erros;           // combinação de ERRO_DHT | ERRO_SOLO
} EstadoSistema;

#endif
