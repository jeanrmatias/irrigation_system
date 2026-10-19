/**
 * @file irrigacao.h
 * @brief A "regra de negócio": decide se o modo AUTO deve regar agora.
 *
 * Não mexe em hardware nenhum, só olha o estado e responde sim/não.
 * Por isso dá para testar no computador (ver ferramentas/testes_host.cpp).
 */
#ifndef IRRIGACAO_H
#define IRRIGACAO_H

#include "../tipos.h"

bool irrigacaoPrecisaRegar(const EstadoSistema *estado, unsigned long agora);

#endif
