/**
 * @file debug.h
 * @brief Linhas de log "# ..." enviadas pela mesma serial do ESP32.
 *
 * O protocolo ignora tudo que não começa com '$', então essas linhas não
 * atrapalham. O simulador de ESP32 mostra elas no terminal como [Arduino].
 */
#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "config.h"

// F("texto") guarda o texto na memória FLASH em vez da RAM.
// O UNO tem só 2 KB de RAM, então isso faz diferença.
inline void debugLog(const __FlashStringHelper *msg) {
#if DEBUG_ATIVO
  LINK_SERIAL.print(F("# "));
  LINK_SERIAL.println(msg);
#else
  (void)msg;
#endif
}

inline void debugLogValor(const __FlashStringHelper *msg, long valor) {
#if DEBUG_ATIVO
  LINK_SERIAL.print(F("# "));
  LINK_SERIAL.print(msg);
  LINK_SERIAL.println(valor);
#else
  (void)msg;
  (void)valor;
#endif
}

#endif
