/**
 * @file valvula.h
 * @brief Controle da válvula (servo) SEM delay: abre, e o loop fecha na hora certa.
 */
#ifndef VALVULA_H
#define VALVULA_H

#include <stdint.h>
#include "../tipos.h"

void valvulaIniciar(Valvula *v);

/** Abre a válvula por 'segundos'. */
void valvulaAbrir(Valvula *v, uint16_t segundos, unsigned long agora);

/** Fecha na hora. */
void valvulaFechar(Valvula *v);

/** Chame em todo loop: fecha sozinha quando o tempo acabar. */
void valvulaAtualizar(Valvula *v, unsigned long agora);

#endif
