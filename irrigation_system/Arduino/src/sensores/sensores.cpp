/**
 * @file sensores.cpp
 */
#include "sensores.h"

#include <Arduino.h>
#include <DHT.h>
#include "../config.h"

// 'static' aqui = "só este arquivo enxerga". Ninguém de fora mexe no sensor
// diretamente: todo mundo passa pelas funções sensores*().
static DHT dht(PINO_DHT, TIPO_DHT);

void sensoresIniciar(void) {
  dht.begin();
  pinMode(PINO_SOLO, INPUT);
  // Antes havia um delay(2000) aqui para o DHT "acordar".
  // Não precisa: a 1ª leitura só acontece depois de INTERVALO_LEITURA_MS
  // (controlado por millis() no loop), o que já dá esse tempo.
}

// Teste de "fio solto" do sensor de solo.
// Ligamos o resistor de pull-up interno (puxa o pino de leve para 5 V).
// Se o sensor estiver ligado, ele "segura" a tensão no valor dele.
// Se o fio estiver solto, ninguém segura e a leitura vai lá para cima (~1023).
static bool soloConectado(void) {
  pinMode(PINO_SOLO, INPUT_PULLUP);
  analogRead(PINO_SOLO);                  // 1ª leitura descartada (ADC estabilizando)
  int comPullup = analogRead(PINO_SOLO);
  pinMode(PINO_SOLO, INPUT);              // desliga o pull-up para a leitura real
  return comPullup < SOLO_DESCONECTADO;
}

static int lerSoloMedia(void) {
  analogRead(PINO_SOLO);                  // descarta (pino acabou de mudar de modo)
  long soma = 0;
  for (uint8_t i = 0; i < SOLO_AMOSTRAS; i++) {
    soma += analogRead(PINO_SOLO);        // ~0,1 ms cada: não bloqueia de verdade
  }
  return (int)(soma / SOLO_AMOSTRAS);
}

void sensoresLer(LeituraSensores *leitura) {
  // ---- Solo ----
  leitura->soloOk = soloConectado();
  leitura->soloBruto = lerSoloMedia();

  // ---- DHT11 ----
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  // isnan = "is not a number": é o que a biblioteca devolve quando falha.
  bool leuAlgo = !isnan(umidade) && !isnan(temperatura);
  bool dentroDaFaixa = leuAlgo &&
                       umidade >= UMIDADE_MIN && umidade <= UMIDADE_MAX &&
                       temperatura >= TEMPERATURA_MIN && temperatura <= TEMPERATURA_MAX;

  leitura->dhtOk = dentroDaFaixa;
  if (dentroDaFaixa) {
    leitura->umidadeAr = umidade;
    leitura->temperatura = temperatura;
  }
  // Se falhou, mantemos os últimos valores bons, mas dhtOk = false avisa
  // todo mundo para NÃO confiar neles. (O bug antigo era confiar no 0.0
  // e abrir a válvula achando que o ar estava seco.)

  leitura->jaLeu = true;
}

uint8_t sensoresErros(const LeituraSensores *leitura) {
  uint8_t erros = 0;
  if (!leitura->jaLeu) return 0;
  if (!leitura->dhtOk)  erros |= ERRO_DHT;   // |= "liga" o bit sem mexer nos outros
  if (!leitura->soloOk) erros |= ERRO_SOLO;
  return erros;
}

const char *sensoresNivelSolo(int soloBruto) {
  if (soloBruto < SOLO_SECO)  return "SECO";
  if (soloBruto < SOLO_MEDIO) return "BAIXO";
  if (soloBruto < SOLO_ALTO)  return "MEDIO";
  return "ALTO";
}
