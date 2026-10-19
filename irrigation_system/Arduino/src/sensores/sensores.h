/**
 * @file sensores.h
 * @brief Leitura e VALIDAÇÃO dos sensores (DHT11 + umidade do solo).
 */
#ifndef SENSORES_H
#define SENSORES_H

#include <stdint.h>
#include "../tipos.h"

void sensoresIniciar(void);

/** Lê todos os sensores e preenche a struct apontada por 'leitura'. */
void sensoresLer(LeituraSensores *leitura);

/** Devolve os bits de erro (ERRO_DHT, ERRO_SOLO) da leitura. */
uint8_t sensoresErros(const LeituraSensores *leitura);

/** Texto do nível do solo: "SECO", "BAIXO", "MEDIO" ou "ALTO". */
const char *sensoresNivelSolo(int soloBruto);

#endif
