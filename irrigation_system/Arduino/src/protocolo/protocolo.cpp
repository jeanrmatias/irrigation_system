/**
 * @file protocolo.cpp
 * @brief Implementação do protocolo (idêntica no Arduino e no ESP32).
 */
#include "protocolo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Converte um caractere hexadecimal ('0'-'9', 'A'-'F', 'a'-'f') para 0..15.
// Retorna -1 se não for hexadecimal.
static int hexParaInt(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Valida o quadro que está no buffer e, se estiver OK, separa os campos.
static bool validarEDividir(ProtoParser *p, ProtoQuadro *q) {
  // O buffer tem algo como "S,1*2E". Procuramos o último '*'.
  char *asterisco = strrchr(p->buffer, '*');
  if (asterisco == NULL) return false;

  // Depois do '*' precisam vir exatamente 2 dígitos hexa.
  if (strlen(asterisco + 1) != 2) return false;
  int alto  = hexParaInt(asterisco[1]);
  int baixo = hexParaInt(asterisco[2]);
  if (alto < 0 || baixo < 0) return false;
  uint8_t recebido = (uint8_t)((alto << 4) | baixo);

  size_t tamConteudo = (size_t)(asterisco - p->buffer);  // aritmética de ponteiros!
  if (tamConteudo == 0) return false;
  if (protoChecksum(p->buffer, tamConteudo) != recebido) return false;

  // Corta a string no '*' e troca cada ',' por '\0'.
  // Assim cada campo vira uma "mini string" dentro do mesmo buffer,
  // e só guardamos ponteiros para o começo de cada uma (sem copiar nada).
  *asterisco = '\0';

  char *tipo = p->buffer;
  char *virgula = strchr(tipo, ',');
  if (virgula != NULL) *virgula = '\0';
  if (strlen(tipo) != 1) return false;  // o tipo tem que ser uma letra só

  q->tipo = tipo[0];
  q->numCampos = 0;

  char *cursor = (virgula != NULL) ? virgula + 1 : NULL;
  while (cursor != NULL) {
    if (q->numCampos >= PROTO_MAX_CAMPOS) return false;
    q->campos[q->numCampos++] = cursor;
    char *prox = strchr(cursor, ',');
    if (prox != NULL) {
      *prox = '\0';
      cursor = prox + 1;
    } else {
      cursor = NULL;
    }
  }
  return true;
}

void protoIniciar(ProtoParser *p) {
  memset(p, 0, sizeof(*p));
}

bool protoReceberByte(ProtoParser *p, char c, ProtoQuadro *q) {
  if (c == '$') {               // começo de quadro (mesmo no meio de outro: recomeça)
    p->dentroDoQuadro = true;
    p->tamanho = 0;
    p->buffer[0] = '\0';
    return false;
  }

  if (!p->dentroDoQuadro) return false;  // lixo ou log '#': ignora

  if (c == '\n' || c == '\r') {          // fim do quadro
    p->dentroDoQuadro = false;
    p->buffer[p->tamanho] = '\0';
    if (validarEDividir(p, q)) {
      p->quadrosValidos++;
      return true;
    }
    p->quadrosInvalidos++;
    return false;
  }

  if (p->tamanho >= PROTO_TAM_MAX) {     // quadro grande demais: descarta
    p->dentroDoQuadro = false;
    p->quadrosInvalidos++;
    return false;
  }

  p->buffer[p->tamanho++] = c;
  return false;
}

uint8_t protoChecksum(const char *conteudo, size_t tam) {
  uint8_t cs = 0;
  for (size_t i = 0; i < tam; i++) {
    cs ^= (uint8_t)conteudo[i];
  }
  return cs;
}

size_t protoMontar(char *destino, size_t tamDestino, const char *conteudo) {
  size_t tam = strlen(conteudo);
  uint8_t cs = protoChecksum(conteudo, tam);
  int escrito = snprintf(destino, tamDestino, "$%s*%02X\n", conteudo, cs);
  if (escrito < 0 || (size_t)escrito >= tamDestino) return 0;
  return (size_t)escrito;
}

bool protoCampoInt(const ProtoQuadro *q, uint8_t indice, long *saida) {
  if (indice >= q->numCampos) return false;
  const char *texto = q->campos[indice];
  if (*texto == '\0') return false;
  char *fim = NULL;
  long valor = strtol(texto, &fim, 10);
  if (*fim != '\0') return false;  // sobrou coisa que não é número
  *saida = valor;
  return true;
}

bool protoCampoIgual(const ProtoQuadro *q, uint8_t indice, const char *texto) {
  if (indice >= q->numCampos) return false;
  return strcmp(q->campos[indice], texto) == 0;
}
