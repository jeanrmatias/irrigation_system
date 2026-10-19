/**
 * @file interface.cpp
 */
#include "interface.h"

#include <Arduino.h>
#include "../config.h"

#define PISCA_RAPIDO_MS  150UL
#define PISCA_LENTO_MS   500UL

// Pisca sem delay: divide o tempo em "fatias" e acende nas fatias pares.
static bool piscando(unsigned long agora, unsigned long periodo) {
  return ((agora / periodo) % 2) == 0;
}

// ---------- DISPLAY (gancho para o futuro) ----------
static void displayIniciar(void) {
  // Ex.: lcd.init(); lcd.backlight();
}

static void displayAtualizar(const EstadoSistema *estado, unsigned long agora) {
  // Ex.: a cada 250 ms escrever estado->leitura.soloBruto, umidadeAr, etc.
  (void)estado;
  (void)agora;
}
// -----------------------------------------------------

void interfaceIniciar(void) {
  pinMode(PINO_LED_OK, OUTPUT);
  pinMode(PINO_LED_ERRO, OUTPUT);
  digitalWrite(PINO_LED_OK, LOW);
  digitalWrite(PINO_LED_ERRO, LOW);
  displayIniciar();
}

void interfaceAtualizar(const EstadoSistema *estado, unsigned long agora) {
  // ---- LED verde ----
  bool verde;
  if (estado->valvula.aberta) {
    verde = piscando(agora, PISCA_RAPIDO_MS);
  } else if (estado->modo == MODO_MANUAL) {
    verde = piscando(agora, PISCA_LENTO_MS);
  } else {
    verde = true;
  }

  // ---- LED vermelho (prioridade: o erro mais grave ganha) ----
  bool vermelho;
  if (!estado->link.online) {
    vermelho = true;
  } else if (estado->erros != 0) {
    vermelho = piscando(agora, PISCA_RAPIDO_MS);
  } else if (!estado->link.nuvemOnline) {
    vermelho = piscando(agora, PISCA_LENTO_MS);
  } else {
    vermelho = false;
  }

  digitalWrite(PINO_LED_OK, verde ? HIGH : LOW);
  digitalWrite(PINO_LED_ERRO, vermelho ? HIGH : LOW);

  displayAtualizar(estado, agora);
}
