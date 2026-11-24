// FAROL #16 — acoes de AUDIO e de MEMORIA que APLICAM a correcao (ADENDO 2.1 C:
// o Farol conserta, nao so informa). Tres acoes do veredito:
//
//   FAROL_A_CONVERTER_AUDIO  liga a conversao de DTS ja existente (AJ_DTS_AC3),
//                            registra o par fazer/desfazer na ilha e pede para
//                            reabrir. Desfazer devolve o valor ANTERIOR do ajuste.
//   FAROL_A_TROCAR_FAIXA     troca para a outra faixa que esta TV toca.
//   FAROL_A_ALIVIAR_REABRIR  alivia a memoria (o mesmo callback do aviso do
//                            memlog) e SO pede reabertura se a TV saiu do
//                            piso (memlog_pode_reabrir).
//
// A decisao e PURA (so `static inline` aqui, sem SDL nem link): quem monta os
// fatos (farol.c) e quem executa (farolacao_audio.c) usam as mesmas funcoes.
//
// REGRA DE PLATAFORMA — conversao de DTS NUNCA e oferecida em:
//   - WASM / .wgt (FP_TIZEN_WGT): a Samsung web nao converte;
//   - .tpk Tizen 4.0-5.5 (osMaior < 6, ou desconhecido): sem o conversor HLS;
//   - Android e outras: o ExoPlayer/Media3 decodifica ou recusa por conta propria.
// So LG webOS e .tpk Tizen 6+ podem. webOS reaproveita nv_dts_* (dts_tv.h).
//
// O QUE ESTE MODULO NAO FAZ: a UI e a navegacao do cartao (#19), o envio do
// registro e os demais executores (#15, #17, #18). A reabertura e um PEDIDO:
// quem hospeda (app.c, na fiacao do #19) registra farolacao_audio_quando_reabrir
// com a recarga da fonte corrente; sem anfitriao o pedido fica pendente e
// farolacao_audio_reabertura_consumir() o entrega.
//
// FIO: executar so no fio principal (mexe em ajustes, player e ilha). Sem
// estado por fio (TLS) e sem rede.
#ifndef NV_FAROLACAO_AUDIO_H
#define NV_FAROLACAO_AUDIO_H

#include "dts_tv.h"
#include "farolregra.h"

// Faixas de audio consideradas na consulta "ha outra faixa que esta TV toca?".
#define FAROLACAO_FAIXAS_MAX 64

// Valores gravados em AJ_DTS_AC3. .tpk: 0 desligado, 1 HLS. LG: 0 original,
// 1 AAC estereo, 2 AC3 5.1, 3 desligado.
#define FAROLACAO_DTS_TPK_LIGADO 1
#define FAROLACAO_DTS_LG_AAC     1
#define FAROLACAO_DTS_LG_AC3     2

// 1 = a PLATAFORMA pode converter DTS (independe do estado do motor).
static inline int farolacao_audio_plataforma_converte(FarolPlat plat, int osMaior) {
  switch (plat) {
    case FP_WEBOS:      return 1;
    case FP_TIZEN_TPK:  return osMaior >= 6;
    default:            return 0;   // WASM/.wgt, Android, outras: NUNCA
  }
}

// O valor de FarolFatos.conversaoPossivel: 1 = pode e esta pronta, 0 = existe
// mas nao serve agora (motor ausente, falhou, TPK < 6), -1 = a plataforma nao
// tem conversao (WASM, Android, outras). `recusado` = a TV ja recusou o audio
// (errorCode 200): ai a conversao serve em qualquer release, como o fallback.
static inline int farolacao_audio_conversao_possivel(FarolPlat plat, int osMaior,
                                                     int motorPronto, int falhou,
                                                     int recusado) {
  if (plat != FP_WEBOS && plat != FP_TIZEN_TPK) return -1;
  if (!farolacao_audio_plataforma_converte(plat, osMaior)) return 0;
  if (!motorPronto || falhou) return 0;
  if (plat == FP_WEBOS && !recusado && !nv_dts_converter_ja(osMaior, 1, 1, 0))
    return 0;   // release antigo: a TV decodifica sozinha, nada a converter
  return 1;
}

// O valor a gravar em AJ_DTS_AC3 para LIGAR a conversao a partir de `atual`:
// quem ja converte (AAC/AC3 na LG) fica como esta; desligado/original vira o
// modo provado (AAC estereo na LG, HLS no .tpk). -1 = plataforma sem conversao.
static inline int farolacao_audio_dts_alvo(FarolPlat plat, int atual) {
  if (plat == FP_TIZEN_TPK) return FAROLACAO_DTS_TPK_LIGADO;
  if (plat != FP_WEBOS) return -1;
  return (atual == FAROLACAO_DTS_LG_AAC || atual == FAROLACAO_DTS_LG_AC3)
             ? atual : FAROLACAO_DTS_LG_AAC;
}

// "Ha outra faixa de audio que esta TV toca?" Devolve o indice da primeira
// faixa diferente da atual e compativel, ou -1. `dts[i]` = 1 quando a faixa i e
// DTS. Com a faixa atual DTS, outra DTS nao serve (a TV a recusaria igual);
// com a atual nao-DTS qualquer outra serve (a TV recusou o codec dela).
static inline int farolacao_audio_faixa_compativel(int n, int atual, int atualDts,
                                                   const signed char *dts) {
  int i;
  if (n > FAROLACAO_FAIXAS_MAX) n = FAROLACAO_FAIXAS_MAX;
  for (i = 0; i < n; i++) {
    if (i == atual) continue;
    if (atualDts > 0 && dts && dts[i] > 0) continue;
    return i;
  }
  return -1;
}

// ------------------------------------------------------------- execucao ----

// Tudo que o executor toca fora de si, para o teste trocar. farolacao_audio_
// ops_padrao() liga o app de verdade; campo NULL num teste e erro dele.
typedef struct {
  int  (*dts_valor)(void);              // AJ_DTS_AC3 gravado
  void (*dts_definir)(int valor);       // grava (volta a gravar o ajuste)
  int  (*faixa_compativel)(void);       // indice da faixa que a TV toca, ou -1
  int  (*faixa_trocar)(void);           // troca para ela; 1 = trocou
  long (*mem_livre_mb)(void);           // MemAvailable em MiB; -1 = desconhecido
  void (*mem_aliviar)(void);            // o callback de alivio do memlog
} FarolAcaoAudioOps;

void farolacao_audio_ops_padrao(FarolAcaoAudioOps *ops);
// NULL restaura o padrao. So o teste usa.
void farolacao_audio_teste_ops(const FarolAcaoAudioOps *ops);

// Executa `acao` (FAROL_A_CONVERTER_AUDIO, _TROCAR_FAIXA ou _ALIVIAR_REABRIR)
// sobre os fatos que o veredito usou. Registra farol_registrar_acao e
// farol_registrar_resultado. 1 = aplicou; 0 = recusou ou nao deu (nada mudou,
// ou o que mudou foi desfeito). Outra acao: 0 sem registrar nada.
int farolacao_audio_executar(int acao, const FarolFatos *fatos);

// REABERTURA. O anfitriao registra `cb` (recarga da fonte corrente, no fio
// principal); cada pedido chama `cb` na hora. Sem anfitriao, o pedido fica
// pendente ate farolacao_audio_reabertura_consumir() (1 = havia pedido).
void farolacao_audio_quando_reabrir(void (*cb)(void));
int  farolacao_audio_reabertura_consumir(void);

#endif
