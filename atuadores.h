#ifndef ATUADORES_H
#define ATUADORES_H

#include <Arduino.h>
#include "Servo.h"

extern unsigned long tempoAnteriorServo;
extern const unsigned long intervaloServo;
extern bool valvulaAberta;

void inicializarServo();
void gerenciarFechamentoValvula(unsigned long tempoAtual);
void verificarCondicaoRegar(unsigned long tempoAtual);

#endif
