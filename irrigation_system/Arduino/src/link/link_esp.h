/**
 * @file link_esp.h
 * @brief A "conversa" com o ESP32 usando o protocolo (ver protocolo.h).
 *
 * Arduino -> ESP32
 *   $D,<seq>,<solo>,<umidAr x10>,<temp x10>,<valvula 0|1>,<modo A|M>,<erros>
 *       a cada INTERVALO_ENVIO_MS. Também serve de "batimento cardíaco".
 *   $A,<COMANDO>,<OK|ERRO>          resposta a um comando recebido
 *
 * ESP32 -> Arduino
 *   $S,<mqtt 0|1>                   status, a cada 2 s (batimento cardíaco)
 *   $C,REGAR,<segundos>             abre a válvula (0 = fecha)
 *   $C,MODO,<A|M>                   troca o modo
 *
 * Números com vírgula vão multiplicados por 10 (24.3 °C -> 243), porque o
 * printf do Arduino UNO não sabe imprimir float.
 */
#ifndef LINK_ESP_H
#define LINK_ESP_H

#include <Arduino.h>
#include "../tipos.h"
#include "../protocolo/protocolo.h"

/**
 * Ponteiro para função: o link não sabe O QUE fazer com um comando,
 * ele só sabe RECEBER. Quem sabe fazer (o Arduino.ino) entrega a função.
 * Analogia: o link é o garçom, a função é a cozinha.
 * Deve retornar true se o comando foi executado.
 */
typedef bool (*TratadorComando)(const ProtoQuadro *quadro);

void linkIniciar(EstadoLink *link, Stream *porta, TratadorComando tratador);

/** Lê o que chegou do ESP32 (sem travar) e trata os quadros. */
void linkProcessarEntrada(EstadoLink *link, unsigned long agora);

/** Marca o ESP32 como offline se ele sumiu há mais de TIMEOUT_ESP_MS. */
void linkVerificarTimeout(EstadoLink *link, unsigned long agora);

/** Envia o quadro $D com o estado atual. */
void linkEnviarDados(EstadoSistema *estado);

#endif
