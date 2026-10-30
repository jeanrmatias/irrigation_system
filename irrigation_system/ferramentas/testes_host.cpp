/**
 * @file testes_host.cpp
 * @brief Testes que rodam no COMPUTADOR (sem Arduino) para o protocolo e a
 *        regra de irrigação. Como esses módulos não usam hardware, dá para
 *        testar a lógica antes de gravar na placa.
 *
 * Compilar e rodar (Linux/WSL), a partir da raiz do repositório:
 *   g++ -std=c++17 -Wall -Wextra -I Arduino/src \
 *       ferramentas/testes_host.cpp Arduino/src/protocolo/protocolo.cpp \
 *       Arduino/src/irrigacao/irrigacao.cpp -o /tmp/testes && /tmp/testes
 */
#include <cstdio>
#include <cstring>

#include "protocolo/protocolo.h"
#include "irrigacao/irrigacao.h"
#include "config.h"

static int falhas = 0;
static int total = 0;

#define CHECAR(cond)                                                    \
  do {                                                                  \
    total++;                                                            \
    if (!(cond)) {                                                      \
      falhas++;                                                         \
      std::printf("  FALHOU (linha %d): %s\n", __LINE__, #cond);        \
    }                                                                   \
  } while (0)

// Entrega uma string inteira ao parser; devolve quantos quadros válidos saíram.
static int alimentar(ProtoParser *p, const char *texto, ProtoQuadro *ultimo) {
  int n = 0;
  for (const char *c = texto; *c; c++) {
    if (protoReceberByte(p, *c, ultimo)) n++;
  }
  return n;
}

static void testeProtocolo() {
  std::printf("[protocolo]\n");
  ProtoParser p;
  ProtoQuadro q;
  char quadro[80];

  // Montar e ler de volta (ida e volta)
  size_t tam = protoMontar(quadro, sizeof(quadro), "D,7,450,553,243,0,A,0");
  CHECAR(tam > 0);
  CHECAR(quadro[0] == '$' && quadro[tam - 1] == '\n');
  protoIniciar(&p);
  CHECAR(alimentar(&p, quadro, &q) == 1);
  CHECAR(q.tipo == 'D');
  CHECAR(q.numCampos == 7);
  long v = 0;
  CHECAR(protoCampoInt(&q, 1, &v) && v == 450);
  CHECAR(protoCampoIgual(&q, 5, "A"));

  // Exemplo documentado em protocolo.h
  protoMontar(quadro, sizeof(quadro), "S,1");
  CHECAR(std::strcmp(quadro, "$S,1*4E\n") == 0);

  // Checksum errado -> rejeita
  protoIniciar(&p);
  CHECAR(alimentar(&p, "$S,1*4F\n", &q) == 0);
  CHECAR(p.quadrosInvalidos == 1);

  // Linhas de log e lixo no meio não atrapalham
  protoIniciar(&p);
  CHECAR(alimentar(&p, "# log do arduino\nlixo$S,1*4E\n", &q) == 1);

  // Quadro cortado no meio + quadro bom em seguida -> só o bom
  protoIniciar(&p);
  CHECAR(alimentar(&p, "$D,1,2,3$S,0*4F\n", &q) == 1);
  CHECAR(q.tipo == 'S');

  // \r\n (Windows) também fecha o quadro
  protoIniciar(&p);
  CHECAR(alimentar(&p, "$S,1*4E\r\n", &q) == 1);

  // Quadro gigante é descartado sem estourar memória
  protoIniciar(&p);
  char grande[200];
  std::memset(grande, 'X', sizeof(grande));
  grande[0] = '$';
  grande[sizeof(grande) - 1] = '\0';
  CHECAR(alimentar(&p, grande, &q) == 0);
  CHECAR(alimentar(&p, "$S,1*4E\n", &q) == 1);

  // Campo não numérico
  protoIniciar(&p);
  protoMontar(quadro, sizeof(quadro), "C,REGAR,5x");
  CHECAR(alimentar(&p, quadro, &q) == 1);
  CHECAR(!protoCampoInt(&q, 1, &v));

  // Destino pequeno demais
  CHECAR(protoMontar(quadro, 5, "D,1,2,3") == 0);
}

static EstadoSistema base() {
  EstadoSistema e;
  std::memset(&e, 0, sizeof(e));
  e.modo = MODO_AUTO;
  e.leitura.jaLeu = true;
  e.leitura.soloOk = true;
  e.leitura.dhtOk = true;
  e.leitura.soloBruto = 400;    // médio
  e.leitura.umidadeAr = 55.0f;
  return e;
}

static void testeIrrigacao() {
  std::printf("[irrigacao]\n");
  unsigned long agora = 100000;

  EstadoSistema e = base();
  CHECAR(!irrigacaoPrecisaRegar(&e, agora));            // tudo normal

  e = base(); e.leitura.soloBruto = 50;
  CHECAR(irrigacaoPrecisaRegar(&e, agora));             // solo seco

  e = base(); e.leitura.soloBruto = 50; e.modo = MODO_MANUAL;
  CHECAR(!irrigacaoPrecisaRegar(&e, agora));            // manual não rega sozinho

  e = base(); e.leitura.soloBruto = 50; e.leitura.soloOk = false;
  CHECAR(!irrigacaoPrecisaRegar(&e, agora));            // sensor de solo com erro

  e = base(); e.leitura.soloBruto = 50; e.valvula.aberta = true;
  CHECAR(!irrigacaoPrecisaRegar(&e, agora));            // já regando

  e = base(); e.leitura.soloBruto = 50;
  e.valvula.jaRegou = true; e.valvula.instanteUltimaRega = agora - 1000;
  CHECAR(!irrigacaoPrecisaRegar(&e, agora));            // regou há 1 s

  e.valvula.instanteUltimaRega = agora - INTERVALO_MIN_REGAS_MS;
  CHECAR(irrigacaoPrecisaRegar(&e, agora));             // intervalo cumprido

  // millis() estourou (voltou a zero) entre a última rega e agora
  // (0 - 256 = o maior número possível menos 255, em qualquer tamanho de unsigned long)
  e.valvula.instanteUltimaRega = 0UL - 256UL;
  CHECAR(!irrigacaoPrecisaRegar(&e, 16UL));             // só 272 ms depois
  CHECAR(irrigacaoPrecisaRegar(&e, INTERVALO_MIN_REGAS_MS)); // ~30 s depois: pode

  e = base(); e.leitura.umidadeAr = 20.0f; e.leitura.soloBruto = 250;
  CHECAR(irrigacaoPrecisaRegar(&e, agora));             // ar seco + solo baixo

  e = base(); e.leitura.umidadeAr = 20.0f; e.leitura.soloBruto = 600;
  CHECAR(!irrigacaoPrecisaRegar(&e, agora));            // ar seco, solo encharcado

  // O BUG ANTIGO: DHT falhou (umidade "0") -> NÃO pode regar por isso
  e = base(); e.leitura.dhtOk = false; e.leitura.umidadeAr = 0.0f; e.leitura.soloBruto = 250;
  CHECAR(!irrigacaoPrecisaRegar(&e, agora));

  e = base(); e.leitura.jaLeu = false; e.leitura.soloBruto = 0;
  CHECAR(!irrigacaoPrecisaRegar(&e, agora));            // antes da 1ª leitura
}

int main() {
  testeProtocolo();
  testeIrrigacao();
  std::printf("\n%d/%d testes OK\n", total - falhas, total);
  return falhas == 0 ? 0 : 1;
}
