/**
 * @file irrigacao.cpp
 */
#include "irrigacao.h"
#include "../config.h"

bool irrigacaoPrecisaRegar(const EstadoSistema *estado, unsigned long agora) {
  const LeituraSensores *l = &estado->leitura;   // atalho (ponteiro para dentro da struct)
  const Valvula *v = &estado->valvula;

  if (estado->modo != MODO_AUTO) return false;   // no MANUAL quem decide é a pessoa
  if (!l->jaLeu) return false;                   // ainda não lemos nada
  if (v->aberta) return false;                   // já está regando

  // Segurança: sem sensor de solo confiável, NÃO rega sozinho.
  if (!l->soloOk) return false;

  // Evita ficar abrindo e fechando sem parar: espera um intervalo mínimo.
  if (v->jaRegou && (agora - v->instanteUltimaRega) < INTERVALO_MIN_REGAS_MS) return false;

  // Regra 1: solo seco -> rega.
  if (l->soloBruto < SOLO_SECO) return true;

  // Regra 2: ar muito seco E solo abaixo do médio -> rega preventiva.
  // (Antes regava com ar seco mesmo com o solo encharcado.)
  if (l->dhtOk && l->umidadeAr < UMIDADE_AR_BAIXA && l->soloBruto < SOLO_MEDIO) return true;

  return false;
}
