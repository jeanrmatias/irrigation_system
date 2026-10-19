/**
 * @file protocolo.h
 * @brief Protocolo de comunicação Arduino <-> ESP32 (UART, texto).
 *
 * ESTE ARQUIVO É IDÊNTICO NO ARDUINO E NO ESP32.
 * Ele só usa C/C++ "puro" (nada de Arduino.h), por isso compila nos dois lados.
 * Se mudar aqui, copie para o outro lado também:
 *   Arduino/src/protocolo/  <->  ESP32/src/protocolo/
 *
 * Formato de um quadro (inspirado no NMEA, usado em GPS):
 *
 *     $<TIPO>,<campo1>,<campo2>,...*<CS>\n
 *
 *   '$'  marca o começo do quadro
 *   TIPO  uma letra: D (dados), S (status), C (comando), A (confirmação)
 *   '*'  separa o conteúdo do checksum
 *   CS    XOR de todos os caracteres entre '$' e '*', em 2 dígitos hexadecimais
 *   '\n'  fim do quadro
 *
 * Exemplo: $S,1*4E\n  (0x53 ^ 0x2C ^ 0x31 = 0x4E)
 *
 * Analogia: o quadro é uma carta. '$' é o envelope abrindo, o checksum é o
 * lacre: se alguém amassou a carta no caminho (ruído no fio), o lacre não
 * confere e a carta vai para o lixo.
 *
 * Qualquer byte fora de um quadro é ignorado. Por isso o Arduino pode mandar
 * linhas de log começando com '#' pela mesma serial sem atrapalhar.
 */
#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stdint.h>
#include <stddef.h>

#define PROTO_TAM_MAX    64   // maior conteúdo aceito entre '$' e '\n'
#define PROTO_MAX_CAMPOS 10   // máximo de campos depois do tipo

/** Estado do "leitor" de quadros: recebe byte a byte. */
typedef struct {
  char     buffer[PROTO_TAM_MAX + 1];
  uint8_t  tamanho;          // quantos bytes já estão no buffer
  bool     dentroDoQuadro;   // já vimos o '$'?
  uint16_t quadrosValidos;   // estatística (útil para debug)
  uint16_t quadrosInvalidos;
} ProtoParser;

/**
 * Quadro já decodificado.
 * ATENÇÃO: os ponteiros em campos[] apontam para dentro do buffer do parser.
 * Eles só valem até o próximo byte ser entregue ao parser, então use-os logo.
 */
typedef struct {
  char        tipo;                     // 'D', 'S', 'C' ou 'A'
  uint8_t     numCampos;                // quantos campos vieram depois do tipo
  const char *campos[PROTO_MAX_CAMPOS];
} ProtoQuadro;

/** Zera o parser. Chame uma vez antes de usar. */
void protoIniciar(ProtoParser *p);

/**
 * Entrega 1 byte recebido ao parser.
 * @return true quando um quadro completo e VÁLIDO acabou de chegar
 *         (e *q foi preenchido). false nos outros casos.
 */
bool protoReceberByte(ProtoParser *p, char c, ProtoQuadro *q);

/** Calcula o XOR de 'tam' caracteres. */
uint8_t protoChecksum(const char *conteudo, size_t tam);

/**
 * Monta um quadro completo "$<conteudo>*HH\n" em 'destino'.
 * @param conteudo texto sem '$', sem '*' e sem '\n'. Ex.: "S,1"
 * @return tamanho do quadro montado, ou 0 se não couber em 'destino'.
 */
size_t protoMontar(char *destino, size_t tamDestino, const char *conteudo);

/**
 * Converte o campo 'indice' para número inteiro.
 * @return true se o campo existe e é um número válido.
 */
bool protoCampoInt(const ProtoQuadro *q, uint8_t indice, long *saida);

/** Compara o campo 'indice' com um texto. */
bool protoCampoIgual(const ProtoQuadro *q, uint8_t indice, const char *texto);

#endif
