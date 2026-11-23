// FAROL — produtores de evento dos backends de video (issue 9 do lote sense,
// PLANO.md secoes 2.2 e 2.3). Cada ponto de erro que ja tinha printf/marco
// ganha UMA linha daqui: o produtor passa a razao do erro como o backend a
// gravou mais o codigo nativo; a classe e o `cod` estavel saem do farolerro —
// a lista de assinaturas nao se duplica nestes arquivos.
//
// `texto` e a razao do erro nativo (o que video_erro_texto devolve, ou a
// linha "erro <Enum>" que o host .tpk loga). NUNCA url, titulo, nome de
// perfil nem host cru: o tok do evento sai de e.cod — token fechado — e o
// anel ainda saneia na gravacao (barreiras 1 e 2 de privacidade).
#ifndef NUVIO_FAROLPRODUTOR_H
#define NUVIO_FAROLPRODUTOR_H

#include "farolanel.h"
#include "farolcod.h"
#include "farolerro.h"

// Erro nativo do player. A TV recusar a faixa de audio vai para
// FE_AUDIO_RECUSADO (o playbook de audio e outro caminho); todo o resto e
// FE_ERRO_NATIVO. a = codigo nativo, b = renderer/backend (0 onde o backend
// nao distingue). Sev 2 = erro. O tok passa pela barreira do farolcod: a
// gramatica fechada do farolnativo e a mesma que o redator aceita — um
// fragmento de texto livre (ex.: pedaco de URL depois de "player_error" numa
// excecao do AVPlay) vira "<plataforma>_outro", nunca trilha no anel.
static inline void farolprod_erro(FarolPlat plat, const char *texto,
                                  int nativo, int renderer) {
  FarolErro e;
  char cod[24];
  farolerro_ler(plat, texto, nativo, &e);
  farol_ev(FM_VIDEO, e.classe == FC_AUDIO ? FE_AUDIO_RECUSADO : FE_ERRO_NATIVO,
           2, e.nativo, renderer, farolcod_anel(&e, plat, cod, sizeof cod));
}

// Retentativa agendada pelo nv_recon_erro (a = tentativa, b = espera em ms).
// tok = o cod do erro que derrubou a reproducao, para o anel contar a
// historia inteira (qual erro levou a qual espera) sem texto livre.
static inline void farolprod_recon(FarolPlat plat, const char *texto,
                                   int nativo, int tentativa,
                                   unsigned esperaMs) {
  FarolErro e;
  char cod[24];
  farolerro_ler(plat, texto, nativo, &e);
  farol_ev(FM_VIDEO, FE_RECONEXAO, 1, tentativa, (int32_t)esperaMs,
           farolcod_anel(&e, plat, cod, sizeof cod));
}

#endif
