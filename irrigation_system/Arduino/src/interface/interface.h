/**
 * @file interface.h
 * @brief O que a pessoa VÊ no dispositivo (LEDs). Sem display LCD.
 *
 * LED VERDE (PINO_LED_OK):
 *   aceso fixo ......... modo AUTO
 *   pisca devagar (1 s)  modo MANUAL
 *   pisca rápido ....... regando agora
 *
 * LED VERMELHO (PINO_LED_ERRO), do mais grave para o menos grave:
 *   aceso fixo ......... sem comunicação com o ESP32
 *   pisca rápido ....... falha de sensor (DHT ou solo)
 *   pisca devagar ...... ESP32 OK, mas sem nuvem (MQTT desconectado)
 *   apagado ............ tudo certo
 *
 * Se depois entrar um display (OLED, LCD, 7 segmentos...), é só implementar
 * as funções marcadas com "DISPLAY" em interface.cpp: o resto do código
 * não precisa mudar.
 */
#ifndef INTERFACE_H
#define INTERFACE_H

#include "../tipos.h"

void interfaceIniciar(void);

/** Chame em todo loop: atualiza LEDs (e o display, se existir). */
void interfaceAtualizar(const EstadoSistema *estado, unsigned long agora);

#endif
