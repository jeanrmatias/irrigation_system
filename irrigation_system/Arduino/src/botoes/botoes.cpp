/**
 * @file botoes.cpp
 */
#include "botoes.h"

#include <Arduino.h>
#include "../config.h"

void botaoIniciar(Botao *b, uint8_t pino) {
  b->pino = pino;
  pinMode(pino, INPUT_PULLUP);
  // Com pull-up: solto = HIGH, pressionado = LOW. Por isso o "== LOW".
  b->ultimaLeituraBruta = (digitalRead(pino) == LOW);
  b->estadoEstavel = b->ultimaLeituraBruta;
  b->instanteMudanca = 0;
  b->eventoPressionado = false;
}

void botaoAtualizar(Botao *b, unsigned long agora) {
  bool leitura = (digitalRead(b->pino) == LOW);

  if (leitura != b->ultimaLeituraBruta) {
    // A leitura mudou: pode ser quique. Zera o cronômetro e espera.
    b->ultimaLeituraBruta = leitura;
    b->instanteMudanca = agora;
    return;
  }

  // Ficou igual por mais de DEBOUNCE_MS: agora é "de verdade".
  if ((agora - b->instanteMudanca) >= DEBOUNCE_MS && leitura != b->estadoEstavel) {
    b->estadoEstavel = leitura;
    if (leitura) {
      b->eventoPressionado = true;   // só no APERTAR (borda de descida)
    }
  }
}

bool botaoFoiPressionado(Botao *b) {
  if (b->eventoPressionado) {
    b->eventoPressionado = false;
    return true;
  }
  return false;
}
