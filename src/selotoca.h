// Selo do que esta tocando (fileira de formato do player, PR #632 / NuvioTV
// #4141): a REGRA que escolhe a marca de resolucao e a de HDR a partir do que
// o pipeline MEDIU. Codigo puro, sem estado: o player chama ao montar a
// fileira e o painel de Informacoes usa a mesma regra; tests/selotoca.sh
// exercita a tabela. So cabecalho (static inline): dezenas de testes ligam
// player.c com lista fechada de fontes, e um .c novo quebraria todos.
#ifndef NV_SELOTOCA_H
#define NV_SELOTOCA_H
#include <strings.h>
#include "badges.h"

// Classe de resolucao (FMT_4K, FMT_1080, FMT_720, FMT_SD de badges.h) ou -1.
// Sai da LARGURA primeiro: filme 2.39:1 em 1080p chega como 1920x800. A faixa
// do 1440p (largura 2400..3199) fica sem selo: ausente e mais honesto que
// errado.
static inline int selotoca_resolucao(int w, int h) {
  if (w >= 3200 || h >= 1800) return FMT_4K;
  if (w >= 2400) return -1;
  if (w >= 1800 || h >= 1000) return FMT_1080;
  if (w >= 1200 || h >= 700) return FMT_720;
  if (w > 0) return FMT_SD;
  return -1;
}

// Marca do HDR ATIVO, ou -1 (SDR ou sem saber).
//   hdr           o hdrType cru de video_hdr(): "none", "HDR10", "HLG",
//                 "HDR10+", "DolbyVision", "desconhecido"...
//   dvConfirmado  video_tem_dolby_vision(): o pipeline devolveu Dolby Vision.
//   marcaFonte    badges_fonte_hdr_marca() da fonte tocando, ou -1. So vale
//                 quando a plataforma NAO mede ("desconhecido", AVPlay da
//                 Samsung), e nunca vira Dolby Vision.
static inline int selotoca_hdr(const char *hdr, int dvConfirmado, int marcaFonte) {
  if (dvConfirmado) return FMT_DV;
  if (!hdr) hdr = "";
  if (!strcasecmp(hdr, "HDR10")) return FMT_HDR10;
  if (!strcasecmp(hdr, "HDR10+") || !strcasecmp(hdr, "HDR10Plus") || !strcasecmp(hdr, "HDR10_PLUS"))
    return FMT_HDR10P;
  if (!strcasecmp(hdr, "HLG")) return FMT_HLG;
  if (!strcasecmp(hdr, "desconhecido") &&
      (marcaFonte == FMT_HDR10P || marcaFonte == FMT_HDR10 || marcaFonte == FMT_HLG || marcaFonte == FMT_HDR))
    return marcaFonte;
  return -1;
}

#endif
