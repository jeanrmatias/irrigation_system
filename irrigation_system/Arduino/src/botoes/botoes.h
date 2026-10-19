/**
 * @file botoes.h
 * @brief Botões com debounce não-bloqueante.
 *
 * Analogia: quando você aperta um botão, o contato metálico "quica" por alguns
 * milissegundos (abre-fecha-abre-fecha), como uma bola que cai no chão.
 * O debounce espera a bola parar de quicar (DEBOUNCE_MS) antes de acreditar.
 */
#ifndef BOTOES_H
#define BOTOES_H

#include <stdint.h>
#include "../tipos.h"

/** Configura o pino com pull-up interno (botão ligado entre o pino e o GND). */
void botaoIniciar(Botao *b, uint8_t pino);

/** Chame em todo loop. */
void botaoAtualizar(Botao *b, unsigned long agora);

/** true UMA vez por clique (consome o evento). */
bool botaoFoiPressionado(Botao *b);

#endif
