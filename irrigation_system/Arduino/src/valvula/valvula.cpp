/**
 * @file valvula.cpp
 */
#include "valvula.h"

#include <Arduino.h>
#include <Servo.h>
#include "../config.h"
#include "../debug.h"

static Servo servo;

void valvulaIniciar(Valvula *v) {
  servo.attach(PINO_SERVO);
  servo.write(SERVO_FECHADO);
  v->aberta = false;
  v->instanteAbertura = 0;
  v->duracaoMs = 0;
  v->instanteUltimaRega = 0;
  v->jaRegou = false;
  v->totalRegas = 0;
}

void valvulaAbrir(Valvula *v, uint16_t segundos, unsigned long agora) {
  if (segundos == 0) {
    valvulaFechar(v);
    return;
  }
  if (segundos > REGA_MAX_S) segundos = REGA_MAX_S;

  servo.write(SERVO_ABERTO);
  v->aberta = true;
  v->instanteAbertura = agora;
  v->duracaoMs = (unsigned long)segundos * 1000UL;
  v->instanteUltimaRega = agora;
  v->jaRegou = true;
  v->totalRegas++;
  debugLogValor(F("Valvula ABERTA por (s): "), segundos);
}

void valvulaFechar(Valvula *v) {
  if (!v->aberta) return;
  servo.write(SERVO_FECHADO);
  v->aberta = false;
  debugLog(F("Valvula FECHADA"));
}

void valvulaAtualizar(Valvula *v, unsigned long agora) {
  // "agora - inicio >= duração" funciona mesmo quando o millis() estoura
  // (volta a zero depois de ~49 dias), porque a conta é feita sem sinal.
  if (v->aberta && (agora - v->instanteAbertura >= v->duracaoMs)) {
    valvulaFechar(v);
  }
}
