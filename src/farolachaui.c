// Tela 7 do Farol — "Achados" (PLANO.md §2.9, §3.2 tela 7, §3.3 "Nada a achar",
// issue #25). Ver farolachaui.h.
#include "farolachaui.h"
#include "farolbusca.h"
#include "faroltexto.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "perfilinfantil.h"
#include "ponteiro.h"
#include "posterprov.h"
#include "tex_cache.h"
#include "text.h"
#include <stdio.h>
#include <string.h>

// Ganchos fracos para manter testes de Tela 7 sem farolbusca.c / farolvozui.c / perfil*.c.
__attribute__((weak)) int farolvozui_disponivel(void) { return 0; }
__attribute__((weak)) const char *farolvozui_rotulo_botao(void) { return FT_ACAO_FALAR; }
__attribute__((weak)) const char *farolvozui_icone_botao(void) { return "aj_mic"; }
__attribute__((weak)) int perfil_infantil_ativo(void) { return 0; }
__attribute__((weak)) int perfil_regra_busca(const char *certificate) {
  return !perfil_infantil_ativo() || perfil_infantil_classif_ok(certificate);
}
__attribute__((weak)) FarolBuscaEstado farolbusca_estado(FarolBuscaInfo *info) {
  if (info) memset(info, 0, sizeof *info);
  return FB_OCIOSO;
}
__attribute__((weak)) int farolbusca_resolver(FarolBuscaItem *out, int max,
                                              unsigned *revisaoOut) {
  (void)out; (void)max;
  if (revisaoOut) *revisaoOut = cat_revisao();
  return 0;
}
__attribute__((weak)) int farolbusca_abrir_item(const FarolBuscaItem *it) {
  (void)it;
  return -1;
}

#define ACHA_MARGEM    120.0f
#define ACHA_CARD_W    280.0f
#define ACHA_CARD_H    420.0f
#define ACHA_CARD_GAP   48.0f
#define ACHA_ROW_Y     326.0f
#define ACHA_VOLTAR_Y  888.0f
#define ACHA_FOCO_VOLTAR FAROL_ACHA_FOCO_VOLTAR
#define ACHA_FOCO_VOZ    FAROL_ACHA_FOCO_VOZ

static struct {
  FarolAchaTipo   tipo;
  int             modoBusca;
  uint64_t        buscaGenConfirmada;
  FarolBuscaItem  buscaItens[FAROL_ACHA_UI_MAX];
  int             maxMin;
  int             limEfetivo;
  char            genero[32];
  char            kicker[96];
  char            tituloOverride[200];
  char            buscaFraseEstado[200];
  char            l1Override[200];
  char            l2Override[200];
  unsigned        semente;
  unsigned        rev;
  FarolAchado     achados[FAROL_ACHA_UI_MAX];
  FarolAchaPedido cards[FAROL_ACHA_UI_MAX];
  int             nAchados;
  int             fallbackAtivo;
  int             focoIdx;
  int             ultimoPoster;
  float           animCard[FAROL_ACHA_UI_MAX];
  float           animVoltar;
  float           animVoz;
  int             pedidoTem;
  int             pediuVoz;
  FarolAchaPedido pedido;
} S;

static unsigned sementeSeq = 1u;

static int ehSerieTipo(const char *t) {
  return t && (!strcmp(t, "series") || !strcmp(t, "show"));
}

static void idBaseDe(const char *src, char *dst, size_t tam) {
  size_t n;
  if (!tam) return;
  if (!src || !src[0]) { dst[0] = '\0'; return; }
  if (!strncmp(src, "tt", 2)) {
    const char *dp = strchr(src, ':');
    n = dp ? (size_t)(dp - src) : strlen(src);
  } else {
    const char *dp = strrchr(src, ':');
    n = strlen(src);
    if (dp && dp > src) {
      const char *p = dp + 1;
      int dig = *p != '\0';
      while (*p) {
        if (*p < '0' || *p > '9') { dig = 0; break; }
        p++;
      }
      if (dig) {
        const char *c1 = dp - 1;
        while (c1 > src && *c1 >= '0' && *c1 <= '9') c1--;
        if (*c1 == ':' && c1 > src && c1 + 1 < dp) n = (size_t)(c1 - src);
      }
    }
  }
  if (n >= tam) n = tam - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static void formatarDuracaoCurta(int min, char *dst, size_t tam) {
  if (min <= 0) {
    if (tam) dst[0] = '\0';
    return;
  }
  if (min >= 60 && min % 60 == 0)
    snprintf(dst, tam, "%dh", min / 60);
  else if (min > 60)
    snprintf(dst, tam, "%dh%02d", min / 60, min % 60);
  else
    snprintf(dst, tam, i18n("%d min"), min);
}

static char linha1[220];
static char linha2[220];

static void montarTextos(void) {
  int n = S.nAchados, i;
  int todosNaoVi = n > 0;
  int todosCurtos = n > 0;
  int limPedido = S.maxMin > 0 ? S.maxMin : FAROL_ACHA_CURTO_PADRAO;
  char gen[64];
  int temGen = 0;

  linha1[0] = '\0';
  linha2[0] = '\0';
  gen[0] = '\0';

  for (i = 0; i < n; i++) {
    if (!(S.achados[i].motivo & FA_MOT_NAO_VI)) todosNaoVi = 0;
    if (!(S.achados[i].motivo & FA_MOT_CURTO)) todosCurtos = 0;
  }
  if (n > 0)
    temGen = farolacha_genero_comum(S.achados, n, S.rev, gen, sizeof gen);

  if (n == 0) {
    if (S.l1Override[0]) {
      snprintf(linha1, sizeof linha1, "%s", S.l1Override);
    } else if (S.tipo == FA_CURTO) {
      snprintf(linha1, sizeof linha1, i18n(FT_NADA_ACHAR_L1), limPedido);
    } else {
      snprintf(linha1, sizeof linha1, "%s", i18n("Nada encontrado"));
    }
    return;
  }

  if (S.l1Override[0]) {
    snprintf(linha1, sizeof linha1, "%s", S.l1Override);
  } else if (S.fallbackAtivo && S.tipo == FA_CURTO) {
    snprintf(linha1, sizeof linha1, i18n(FT_NADA_ACHAR_L1), limPedido);
  } else if (todosCurtos) {
    if (temGen) {
      char dur[32];
      formatarDuracaoCurta(S.limEfetivo, dur, sizeof dur);
      snprintf(linha1, sizeof linha1, i18n(FT_ACHA_L1), n, gen, dur);
    } else {
      snprintf(linha1, sizeof linha1, i18n(FT_ACHA_CURTOS_L1), n, S.limEfetivo);
    }
  } else if (S.tipo == FA_RETOMAR) {
    snprintf(linha1, sizeof linha1,
             i18n(n == 1 ? "%d título em andamento" : "%d títulos em andamento"), n);
  } else if (S.tipo == FA_NAO_VI) {
    if (temGen && n > 1)
      snprintf(linha1, sizeof linha1, i18n("%d títulos · %s"), n, gen);
    else
      snprintf(linha1, sizeof linha1,
               i18n(n == 1 ? "%d título para ver" : "%d títulos para ver"), n);
  } else {
    if (temGen && n > 1)
      snprintf(linha1, sizeof linha1, i18n("%d títulos · %s"), n, gen);
    else
      snprintf(linha1, sizeof linha1,
               i18n(n == 1 ? "%d título" : "%d títulos"), n);
  }

  if (S.l2Override[0]) {
    snprintf(linha2, sizeof linha2, "%s", S.l2Override);
  } else if (S.fallbackAtivo && S.tipo == FA_CURTO) {
    if (n == 3 && todosCurtos)
      snprintf(linha2, sizeof linha2, "%s", i18n(FT_NADA_ACHAR_L2));
  } else if (!S.fallbackAtivo && todosNaoVi) {
    snprintf(linha2, sizeof linha2, "%s", i18n(FT_ACHA_L2_NAO_VISTO));
  }
}

static void identidadeCardDe(FarolAchaPedido *dst, unsigned rev, int indice,
                             const CatItem *c) {
  memset(dst, 0, sizeof *dst);
  dst->rev = rev;
  dst->indice = indice;
  if (!c) return;
  dst->tmdb = c->tmdb;
  snprintf(dst->imdb, sizeof dst->imdb, "%s", c->imdb);
  snprintf(dst->tipo, sizeof dst->tipo, "%s", c->tipo);
  snprintf(dst->titulo, sizeof dst->titulo, "%s", c->titulo);
  snprintf(dst->meta, sizeof dst->meta, "%s", c->meta);
}

static int itemBateSemId(const CatItem *c, const FarolAchaPedido *p) {
  if (!c || !p) return 0;
  if (c->imdb[0] || c->tmdb > 0) return 0;
  if (strcmp(c->tipo, p->tipo) != 0) return 0;
  if (strcmp(c->titulo, p->titulo) != 0) return 0;
  if (strcmp(c->meta, p->meta) != 0) return 0;
  return 1;
}

static int itemBateComId(const CatItem *c, const FarolAchaPedido *p,
                         const char *basePed, int temBase) {
  char baseCat[64];
  if (!c || !p) return 0;
  if (p->tipo[0] && c->tipo[0] && ehSerieTipo(p->tipo) != ehSerieTipo(c->tipo))
    return 0;
  if (p->imdb[0] && !strcmp(c->imdb, p->imdb)) return 1;
  if (temBase) {
    idBaseDe(c->imdb, baseCat, sizeof baseCat);
    if (baseCat[0] && !strcmp(baseCat, basePed)) return 1;
  }
  if (p->tmdb > 0) {
    if (c->tmdb == p->tmdb) return 1;
    if (c->tmdb <= 0 && !strncmp(c->imdb, "tmdb:", 5)) {
      char tbuf[32];
      snprintf(tbuf, sizeof tbuf, "tmdb:%ld", p->tmdb);
      if (!strcmp(c->imdb, tbuf)) return 1;
    }
  }
  if (!strncmp(p->imdb, "tmdb:", 5) && c->tmdb > 0) {
    char tbuf[32];
    snprintf(tbuf, sizeof tbuf, "tmdb:%ld", c->tmdb);
    if (!strcmp(p->imdb, tbuf)) return 1;
  }
  return 0;
}

static int itemBatePedido(const CatItem *c, const FarolAchaPedido *p) {
  char basePed[64];
  if (!c || !p) return 0;
  if (p->imdb[0] || p->tmdb > 0) {
    idBaseDe(p->imdb, basePed, sizeof basePed);
    return itemBateComId(c, p, basePed, basePed[0] != '\0');
  }
  return itemBateSemId(c, p);
}

static int mesmaIdentidadeCard(const FarolAchaPedido *a, const FarolAchaPedido *b) {
  int temIdA, temIdB;
  if (!a || !b) return 0;
  temIdA = (a->imdb[0] != '\0' || a->tmdb > 0);
  temIdB = (b->imdb[0] != '\0' || b->tmdb > 0);
  if (temIdA != temIdB) return 0;
  if (temIdA) {
    char ba[64], bb[64];
    if (a->tipo[0] && b->tipo[0] && ehSerieTipo(a->tipo) != ehSerieTipo(b->tipo))
      return 0;
    if (a->imdb[0] && b->imdb[0] && !strcmp(a->imdb, b->imdb)) return 1;
    idBaseDe(a->imdb, ba, sizeof ba);
    idBaseDe(b->imdb, bb, sizeof bb);
    if (ba[0] && bb[0] && !strcmp(ba, bb)) return 1;
    if (a->tmdb > 0 && b->tmdb > 0 && a->tmdb == b->tmdb) return 1;
    if (a->tmdb > 0 && !strncmp(b->imdb, "tmdb:", 5)) {
      char tbuf[32];
      snprintf(tbuf, sizeof tbuf, "tmdb:%ld", a->tmdb);
      if (!strcmp(b->imdb, tbuf)) return 1;
    }
    if (b->tmdb > 0 && !strncmp(a->imdb, "tmdb:", 5)) {
      char tbuf[32];
      snprintf(tbuf, sizeof tbuf, "tmdb:%ld", b->tmdb);
      if (!strcmp(a->imdb, tbuf)) return 1;
    }
    return 0;
  }
  return !strcmp(a->tipo, b->tipo) &&
         !strcmp(a->titulo, b->titulo) &&
         !strcmp(a->meta, b->meta);
}

static int contarSemIdExato(const FarolAchaPedido *p) {
  int tentativa;
  if (!p) return -1;
  for (tentativa = 0; tentativa < 3; tentativa++) {
    unsigned r = cat_revisao();
    int nc = cat_n(), i, matches = 0;
    for (i = 0; i < nc; i++) {
      const CatItem *c = cat_item(i);
      if (itemBateSemId(c, p)) matches++;
    }
    if (cat_revisao() == r) return matches;
  }
  return -1;
}

static int itemInfantilPermitido(const CatItem *c, int infantil) {
  if (!infantil) return 1;
  if (!c) return 0;
  return perfil_regra_busca(c->classificacao) &&
         perfil_infantil_classif_ok(c->classificacao);
}

static void formatarFraseEstadoBusca(char *dst, size_t cap,
                                     const FarolBuscaInfo *info, int n) {
  if (!dst || !cap) return;
  if (info && info->fraseEstado && info->fraseEstado[0]) {
    snprintf(dst, cap, "%s", i18n(info->fraseEstado));
  } else if (n > 0) {
    snprintf(dst, cap, i18n(n == 1 ? "%d título" : "%d títulos"), n);
  } else {
    snprintf(dst, cap, "%s", i18n("Nada encontrado"));
  }
}

static void ajustarFocoBusca(void) {
  if (S.nAchados <= 0) {
    if (S.focoIdx != ACHA_FOCO_VOZ || !farolvozui_disponivel())
      S.focoIdx = ACHA_FOCO_VOLTAR;
  } else if (S.focoIdx >= S.nAchados) {
    S.focoIdx = S.nAchados - 1;
  } else if (S.focoIdx < 0 && S.focoIdx != ACHA_FOCO_VOLTAR &&
             S.focoIdx != ACHA_FOCO_VOZ) {
    S.focoIdx = 0;
  }
  if (S.focoIdx == ACHA_FOCO_VOZ && !farolvozui_disponivel())
    S.focoIdx = S.nAchados > 0 ? 0 : ACHA_FOCO_VOLTAR;
  if (S.ultimoPoster >= S.nAchados)
    S.ultimoPoster = S.nAchados > 0 ? S.nAchados - 1 : 0;
}

static void preencherCardsBusca(const FarolBuscaItem *bi, int n, unsigned r,
                                FarolAchado *ach, FarolAchaPedido *cards,
                                int *okOut) {
  int i;
  *okOut = 1;
  for (i = 0; i < n && i < FAROL_ACHA_UI_MAX; i++) {
    if (bi[i].noCatalogo) {
      const CatItem *ci = cat_item(bi[i].indice);
      if (!ci || cat_revisao() != r) { *okOut = 0; break; }
      ach[i].indice = bi[i].indice;
      ach[i].minutos = bi[i].minutos;
      ach[i].motivo = bi[i].motivoAcha;
      identidadeCardDe(&cards[i], r, bi[i].indice, ci);
    } else {
      ach[i].indice = -1;
      ach[i].minutos = bi[i].minutos;
      ach[i].motivo = 0;
      cards[i].rev = r;
      cards[i].indice = -1;
      cards[i].tmdb = bi[i].tmdb;
      snprintf(cards[i].imdb, sizeof cards[i].imdb, "%s", bi[i].imdb);
      snprintf(cards[i].tipo, sizeof cards[i].tipo, "%s", bi[i].tipo);
    }
  }
}

static int carregarBuscaNova(const char *titulo, uint64_t geracaoEsperada) {
  int red = ajustes_animacoes_reduzidas();
  int tentativa;
  for (tentativa = 0; tentativa < 3; tentativa++) {
    FarolBuscaInfo info1, info2;
    FarolBuscaItem bi[FAROL_ACHA_UI_MAX];
    FarolAchado ach[FAROL_ACHA_UI_MAX];
    FarolAchaPedido cards[FAROL_ACHA_UI_MAX];
    unsigned r = 0;
    int n, i, ok = 1;

    memset(bi, 0, sizeof bi);
    memset(ach, 0, sizeof ach);
    memset(cards, 0, sizeof cards);

    if (farolbusca_estado(&info1) != FB_PRONTO) return 0;
    if (geracaoEsperada != 0 && info1.geracao != geracaoEsperada) return 0;

    n = farolbusca_resolver(bi, FAROL_ACHA_UI_MAX, &r);
    if (farolbusca_estado(&info2) != FB_PRONTO || info2.geracao != info1.geracao)
      continue;

    preencherCardsBusca(bi, n, r, ach, cards, &ok);

    if (ok && cat_revisao() == r) {
      memset(&S, 0, sizeof S);
      S.modoBusca = 1;
      S.tipo = FA_SURPRESA;
      S.buscaGenConfirmada = info1.geracao;
      S.rev = r;
      S.nAchados = n;
      S.fallbackAtivo = info1.usouLocal;
      memcpy(S.buscaItens, bi, sizeof S.buscaItens);
      memcpy(S.achados, ach, sizeof S.achados);
      memcpy(S.cards, cards, sizeof S.cards);
      snprintf(S.kicker, sizeof S.kicker, "%s › %s", i18n(FT_NOME), i18n(FT_BUSCA_FRASE));
      snprintf(S.tituloOverride, sizeof S.tituloOverride, "%s",
               (titulo && titulo[0]) ? titulo : i18n(FT_BUSCA_FRASE));
      formatarFraseEstadoBusca(S.buscaFraseEstado, sizeof S.buscaFraseEstado, &info1, n);
      snprintf(S.l1Override, sizeof S.l1Override, "%s", S.buscaFraseEstado);
      S.l2Override[0] = '\0';
      S.focoIdx = (S.nAchados > 0) ? 0 : ACHA_FOCO_VOLTAR;
      S.ultimoPoster = 0;
      if (red) {
        for (i = 0; i < FAROL_ACHA_UI_MAX; i++)
          S.animCard[i] = (S.nAchados > 0 && S.focoIdx == i) ? 1.0f : 0.0f;
        S.animVoltar = (S.focoIdx == ACHA_FOCO_VOLTAR) ? 1.0f : 0.0f;
        S.animVoz = (S.focoIdx == ACHA_FOCO_VOZ) ? 1.0f : 0.0f;
      }
      montarTextos();
      return 1;
    }
  }
  return 0;
}

// Revalida os cartazes ja confirmados contra o catalogo vivo sem consultar
// outra geracao em farolbusca (ex.: quando o usuario cancelou uma 2a busca).
static void revalidarCardsBuscaConfirmados(void) {
  int tentativa;
  int infantil = perfil_infantil_ativo() ? 1 : 0;

  for (tentativa = 0; tentativa < 3; tentativa++) {
    unsigned r = cat_revisao();
    FarolBuscaItem bi[FAROL_ACHA_UI_MAX];
    FarolAchado ach[FAROL_ACHA_UI_MAX];
    FarolAchaPedido cards[FAROL_ACHA_UI_MAX];
    int i, k = 0, retry = 0;

    memset(bi, 0, sizeof bi);
    memset(ach, 0, sizeof ach);
    memset(cards, 0, sizeof cards);

    for (i = 0; i < S.nAchados && i < FAROL_ACHA_UI_MAX; i++) {
      unsigned rItem = 0;
      int idx = farolachaui_resolver_pedido(&S.cards[i], &rItem);
      if (rItem != r || cat_revisao() != r) { retry = 1; break; }
      if (idx >= 0) {
        const CatItem *ci = cat_item(idx);
        if (!ci || cat_revisao() != r) { retry = 1; break; }
        if (!itemInfantilPermitido(ci, infantil)) continue;
        bi[k] = S.buscaItens[i];
        bi[k].noCatalogo = 1;
        bi[k].indice = idx;
        ach[k] = S.achados[i];
        ach[k].indice = idx;
        identidadeCardDe(&cards[k], r, idx, ci);
        k++;
      } else if (!S.fallbackAtivo && !infantil &&
                 (S.cards[i].imdb[0] || S.cards[i].tmdb > 0)) {
        bi[k] = S.buscaItens[i];
        bi[k].noCatalogo = 0;
        bi[k].indice = -1;
        ach[k] = S.achados[i];
        ach[k].indice = -1;
        cards[k] = S.cards[i];
        cards[k].rev = r;
        cards[k].indice = -1;
        k++;
      }
    }

    if (!retry && cat_revisao() == r) {
      memcpy(S.buscaItens, bi, sizeof S.buscaItens);
      memcpy(S.achados, ach, sizeof S.achados);
      memcpy(S.cards, cards, sizeof S.cards);
      S.nAchados = k;
      S.rev = r;
      if (S.nAchados <= 0) {
        snprintf(S.l1Override, sizeof S.l1Override, "%s", i18n("Nada encontrado"));
      } else if (S.buscaFraseEstado[0]) {
        snprintf(S.l1Override, sizeof S.l1Override, "%s", S.buscaFraseEstado);
      }
      S.l2Override[0] = '\0';
      ajustarFocoBusca();
      montarTextos();
      return;
    }
  }

  memset(S.buscaItens, 0, sizeof S.buscaItens);
  memset(S.achados, 0, sizeof S.achados);
  memset(S.cards, 0, sizeof S.cards);
  S.nAchados = 0;
  S.rev = cat_revisao();
  S.focoIdx = ACHA_FOCO_VOLTAR;
  S.ultimoPoster = 0;
  snprintf(S.l1Override, sizeof S.l1Override, "%s", i18n("Nada encontrado"));
  S.l2Override[0] = '\0';
  montarTextos();
}

static void recarregarBusca(void) {
  FarolBuscaInfo inf;
  int tentativa;

  if (S.buscaGenConfirmada == 0 ||
      farolbusca_estado(&inf) != FB_PRONTO ||
      inf.geracao != S.buscaGenConfirmada) {
    revalidarCardsBuscaConfirmados();
    return;
  }

  for (tentativa = 0; tentativa < 3; tentativa++) {
    FarolBuscaInfo info1, info2;
    FarolBuscaItem bi[FAROL_ACHA_UI_MAX];
    FarolAchado ach[FAROL_ACHA_UI_MAX];
    FarolAchaPedido cards[FAROL_ACHA_UI_MAX];
    unsigned r = 0;
    int n, ok = 1;

    memset(bi, 0, sizeof bi);
    memset(ach, 0, sizeof ach);
    memset(cards, 0, sizeof cards);

    if (farolbusca_estado(&info1) != FB_PRONTO ||
        info1.geracao != S.buscaGenConfirmada) {
      revalidarCardsBuscaConfirmados();
      return;
    }

    n = farolbusca_resolver(bi, FAROL_ACHA_UI_MAX, &r);
    if (farolbusca_estado(&info2) != FB_PRONTO ||
        info2.geracao != S.buscaGenConfirmada) {
      if (info2.geracao != S.buscaGenConfirmada) {
        revalidarCardsBuscaConfirmados();
        return;
      }
      continue;
    }

    preencherCardsBusca(bi, n, r, ach, cards, &ok);

    if (ok && cat_revisao() == r) {
      memcpy(S.buscaItens, bi, sizeof S.buscaItens);
      memcpy(S.achados, ach, sizeof S.achados);
      memcpy(S.cards, cards, sizeof S.cards);
      S.nAchados = n;
      S.rev = r;
      S.fallbackAtivo = info1.usouLocal;
      formatarFraseEstadoBusca(S.buscaFraseEstado, sizeof S.buscaFraseEstado, &info1, n);
      snprintf(S.l1Override, sizeof S.l1Override, "%s", S.buscaFraseEstado);
      S.l2Override[0] = '\0';
      ajustarFocoBusca();
      montarTextos();
      return;
    }
  }

  revalidarCardsBuscaConfirmados();
}

static void recarregar(void) {
  int lim = S.maxMin > 0 ? S.maxMin : FAROL_ACHA_CURTO_PADRAO;
  int limConsulta = (S.tipo == FA_CURTO) ? (lim > 1 ? lim - 1 : 1) : lim;
  int limFb = lim < 130 ? 130 : lim + 30;
  int limFbConsulta = limFb > 1 ? limFb - 1 : 1;
  int tentativa;

  if (S.modoBusca) {
    recarregarBusca();
    return;
  }

  for (tentativa = 0; tentativa < 3; tentativa++) {
    int n = 0, i, ok = 1;
    unsigned r = 0;

    S.fallbackAtivo = 0;
    S.limEfetivo = lim;
    memset(S.achados, 0, sizeof S.achados);
    memset(S.cards, 0, sizeof S.cards);

    if (S.genero[0]) {
      n = farolacha_listar_genero(S.tipo, limConsulta, S.genero, S.achados,
                                  FAROL_ACHA_UI_MAX, S.semente, &r);
      if (n == 0) {
        n = farolacha_listar_rev(S.tipo, limConsulta, S.achados,
                                 FAROL_ACHA_UI_MAX, S.semente, &r);
        if (n > 0) S.fallbackAtivo = 1;
      }
    } else {
      n = farolacha_listar_rev(S.tipo, limConsulta, S.achados,
                               FAROL_ACHA_UI_MAX, S.semente, &r);
    }

    if (n == 0 && S.tipo == FA_CURTO) {
      n = farolacha_listar_rev(FA_CURTO, limFbConsulta, S.achados,
                               FAROL_ACHA_UI_MAX, S.semente, &r);
      if (n > 0) {
        S.fallbackAtivo = 1;
        S.limEfetivo = limFb;
      }
    }

    for (i = 0; i < n; i++) {
      const CatItem *ci = farolacha_item(&S.achados[i], r);
      if (!ci) { ok = 0; break; }
      identidadeCardDe(&S.cards[i], r, S.achados[i].indice, ci);
    }

    if (ok && cat_revisao() == r) {
      S.nAchados = n;
      S.rev = r;
      if (S.nAchados <= 0) {
        S.focoIdx = ACHA_FOCO_VOLTAR;
      } else if (S.focoIdx >= S.nAchados) {
        S.focoIdx = S.nAchados - 1;
      }
      if (S.ultimoPoster >= S.nAchados)
        S.ultimoPoster = S.nAchados > 0 ? S.nAchados - 1 : 0;
      montarTextos();
      return;
    }
  }

  S.nAchados = 0;
  S.rev = cat_revisao();
  S.fallbackAtivo = 0;
  S.limEfetivo = lim;
  memset(S.achados, 0, sizeof S.achados);
  memset(S.cards, 0, sizeof S.cards);
  S.focoIdx = ACHA_FOCO_VOLTAR;
  S.ultimoPoster = 0;
  montarTextos();
}

void farolachaui_recarregar(void) {
  recarregar();
}

static void garantirRevisao(void) {
  int i;
  if (S.modoBusca) {
    if (cat_revisao() != S.rev) {
      recarregar();
      return;
    }
    for (i = 0; i < S.nAchados; i++) {
      if (S.buscaItens[i].noCatalogo) {
        const CatItem *ci = cat_item(S.buscaItens[i].indice);
        if (!ci || !itemBatePedido(ci, &S.cards[i])) {
          recarregar();
          return;
        }
      }
    }
    return;
  }
  if (cat_revisao() != S.rev) {
    recarregar();
    return;
  }
  for (i = 0; i < S.nAchados; i++) {
    if (!farolacha_item(&S.achados[i], S.rev)) {
      recarregar();
      return;
    }
  }
}

void farolachaui_reset(void) {
  memset(&S, 0, sizeof S);
  linha1[0] = '\0';
  linha2[0] = '\0';
  S.focoIdx = ACHA_FOCO_VOLTAR;
}

int farolachaui_abrir_busca_geracao(const char *titulo, uint64_t geracao) {
  return carregarBuscaNova(titulo, geracao);
}

void farolachaui_abrir_busca(const char *titulo) {
  if (carregarBuscaNova(titulo, 0)) return;
  if (!S.modoBusca) {
    memset(&S, 0, sizeof S);
    S.modoBusca = 1;
    S.tipo = FA_SURPRESA;
    S.rev = cat_revisao();
    snprintf(S.kicker, sizeof S.kicker, "%s › %s", i18n(FT_NOME), i18n(FT_BUSCA_FRASE));
    snprintf(S.tituloOverride, sizeof S.tituloOverride, "%s",
             (titulo && titulo[0]) ? titulo : i18n(FT_BUSCA_FRASE));
    snprintf(S.buscaFraseEstado, sizeof S.buscaFraseEstado, "%s", i18n("Nada encontrado"));
    snprintf(S.l1Override, sizeof S.l1Override, "%s", S.buscaFraseEstado);
    S.focoIdx = ACHA_FOCO_VOLTAR;
    montarTextos();
  }
}

void farolachaui_abrir_op(const FarolAchaAbrir *op) {
  int red = ajustes_animacoes_reduzidas();
  int i;
  FarolAchaTipo t = op ? op->tipo : FA_CURTO;
  if (t != FA_CURTO && t != FA_NAO_VI && t != FA_RETOMAR && t != FA_SURPRESA)
    t = FA_CURTO;

  memset(&S, 0, sizeof S);
  S.tipo = t;
  S.maxMin = (op && op->maxMin > 0) ? op->maxMin : FAROL_ACHA_CURTO_PADRAO;
  if (op && op->genero && op->genero[0])
    snprintf(S.genero, sizeof S.genero, "%s", op->genero);
  if (op && op->kicker && op->kicker[0])
    snprintf(S.kicker, sizeof S.kicker, "%s", op->kicker);
  if (op && op->l1Override && op->l1Override[0])
    snprintf(S.l1Override, sizeof S.l1Override, "%s", op->l1Override);
  if (op && op->l2Override && op->l2Override[0])
    snprintf(S.l2Override, sizeof S.l2Override, "%s", op->l2Override);

  if (op && op->semente != 0u) {
    S.semente = op->semente;
  } else {
    unsigned ticks = (unsigned)SDL_GetTicks();
    S.semente = (ticks ^ (sementeSeq++ * 2654435761u)) | 1u;
  }

  S.focoIdx = 0;
  S.ultimoPoster = 0;
  recarregar();

  if (red) {
    for (i = 0; i < FAROL_ACHA_UI_MAX; i++)
      S.animCard[i] = (S.nAchados > 0 && S.focoIdx == i) ? 1.0f : 0.0f;
    S.animVoltar = (S.focoIdx == ACHA_FOCO_VOLTAR) ? 1.0f : 0.0f;
  }
}

void farolachaui_abrir(FarolAchaTipo tipo, int maxMin, unsigned semente) {
  FarolAchaAbrir op;
  memset(&op, 0, sizeof op);
  op.tipo = tipo;
  op.maxMin = maxMin;
  op.semente = semente;
  farolachaui_abrir_op(&op);
}

static int selecionarFocado(void) {
  FarolAchaPedido exibido;
  int focoAntes;
  unsigned revAtual;
  int infantil;
  const CatItem *c;

  if (S.focoIdx < 0 || S.focoIdx >= S.nAchados) return 0;
  exibido = S.cards[S.focoIdx];
  focoAntes = S.focoIdx;
  revAtual = cat_revisao();
  infantil = perfil_infantil_ativo() ? 1 : 0;

  if (S.modoBusca) {
    if (!S.buscaItens[focoAntes].noCatalogo) {
      int idx, tentativa;
      unsigned revNova;
      if (infantil) return 0;
      idx = farolbusca_abrir_item(&S.buscaItens[focoAntes]);
      if (idx < 0) return 0;
      for (tentativa = 0; tentativa < 3; tentativa++) {
        revNova = cat_revisao();
        c = cat_item(idx);
        if (!c || !itemBatePedido(c, &exibido)) {
          idx = farolachaui_resolver_pedido(&exibido, &revNova);
          if (idx < 0) return 0;
          c = cat_item(idx);
        }
        if (!c || !itemBatePedido(c, &exibido)) continue;
        if (!itemInfantilPermitido(c, infantil)) return 0;
        identidadeCardDe(&S.pedido, revNova, idx, c);
        if (cat_revisao() == revNova) {
          S.pedidoTem = 1;
          return 1;
        }
        memset(&S.pedido, 0, sizeof S.pedido);
      }
      return 0;
    }
    if (revAtual != exibido.rev) {
      recarregar();
      if (focoAntes < 0 || focoAntes >= S.nAchados) return 0;
      if (!mesmaIdentidadeCard(&S.cards[focoAntes], &exibido)) return 0;
      if (!exibido.imdb[0] && exibido.tmdb <= 0 &&
          contarSemIdExato(&exibido) != 1)
        return 0;
      c = cat_item(S.buscaItens[focoAntes].indice);
      if (!c || !itemBatePedido(c, &exibido) ||
          !itemInfantilPermitido(c, infantil) || cat_revisao() != S.rev)
        return 0;
      S.pedido = S.cards[focoAntes];
      S.pedidoTem = 1;
      return 1;
    }
    c = cat_item(S.buscaItens[focoAntes].indice);
    if (!c || !itemBatePedido(c, &exibido) || cat_revisao() != exibido.rev) {
      recarregar();
      return 0;
    }
    if (!itemInfantilPermitido(c, infantil)) {
      recarregar();
      return 0;
    }
    S.pedido = exibido;
    S.pedidoTem = 1;
    return 1;
  }

  if (revAtual != exibido.rev) {
    recarregar();
    if (focoAntes < 0 || focoAntes >= S.nAchados) return 0;
    if (!mesmaIdentidadeCard(&S.cards[focoAntes], &exibido)) return 0;
    if (!exibido.imdb[0] && exibido.tmdb <= 0 &&
        contarSemIdExato(&exibido) != 1)
      return 0;
    c = farolacha_item(&S.achados[focoAntes], S.rev);
    if (!itemInfantilPermitido(c, infantil)) return 0;
    S.pedido = S.cards[focoAntes];
    S.pedidoTem = 1;
    return 1;
  }

  c = farolacha_item(&S.achados[focoAntes], exibido.rev);
  if (!c || !itemBatePedido(c, &exibido) || cat_revisao() != exibido.rev) {
    recarregar();
    return 0;
  }
  if (!itemInfantilPermitido(c, infantil)) return 0;

  S.pedido = exibido;
  S.pedidoTem = 1;
  return 1;
}

int farolachaui_evento(const SDL_Event *e, int *pediuVoltar, int *pediuAbrir) {
  SDL_Keycode k;
  int vozDisp = farolvozui_disponivel() ? 1 : 0;
  if (pediuVoltar) *pediuVoltar = 0;
  if (pediuAbrir) *pediuAbrir = 0;
  if (!e || e->type != SDL_KEYDOWN) return 0;

  k = e->key.keysym.sym;
  if (k == SDLK_LEFT) {
    if (S.focoIdx == ACHA_FOCO_VOZ) S.focoIdx = ACHA_FOCO_VOLTAR;
    else if (S.nAchados > 0 && S.focoIdx > 0) S.focoIdx--;
    return 1;
  }
  if (k == SDLK_RIGHT) {
    if (S.focoIdx == ACHA_FOCO_VOLTAR) {
      if (vozDisp) S.focoIdx = ACHA_FOCO_VOZ;
    } else if (S.nAchados > 0 && S.focoIdx >= 0 && S.focoIdx + 1 < S.nAchados) {
      S.focoIdx++;
    }
    return 1;
  }
  if (k == SDLK_DOWN) {
    if (S.focoIdx >= 0) {
      S.ultimoPoster = S.focoIdx;
      S.focoIdx = ACHA_FOCO_VOLTAR;
    }
    return 1;
  }
  if (k == SDLK_UP) {
    if ((S.focoIdx == ACHA_FOCO_VOLTAR || S.focoIdx == ACHA_FOCO_VOZ) && S.nAchados > 0) {
      S.focoIdx = (S.ultimoPoster >= 0 && S.ultimoPoster < S.nAchados)
                ? S.ultimoPoster : 0;
    }
    return 1;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (e->key.repeat) return 1;
    if (S.focoIdx == ACHA_FOCO_VOLTAR) {
      if (pediuVoltar) *pediuVoltar = 1;
    } else if (S.focoIdx == ACHA_FOCO_VOZ) {
      if (vozDisp) S.pediuVoz = 1;
    } else if (selecionarFocado()) {
      if (pediuAbrir) *pediuAbrir = 1;
    }
    return 1;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (pediuVoltar) *pediuVoltar = 1;
    return 1;
  }
  return 0;
}

void farolachaui_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  int i;
  (void)agora;
  garantirRevisao();
  for (i = 0; i < FAROL_ACHA_UI_MAX; i++) {
    float alvo = (S.nAchados > 0 && S.focoIdx == i) ? 1.0f : 0.0f;
    if (red) S.animCard[i] = alvo;
    else S.animCard[i] = anim_mola(S.animCard[i], alvo, dt,
                                   alvo > S.animCard[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  {
    int vozDisp = farolvozui_disponivel() ? 1 : 0;
    float alvoV, alvoVoz;
    if (S.focoIdx == ACHA_FOCO_VOZ && !vozDisp)
      S.focoIdx = S.nAchados > 0 ? 0 : ACHA_FOCO_VOLTAR;
    alvoV = (S.focoIdx == ACHA_FOCO_VOLTAR) ? 1.0f : 0.0f;
    alvoVoz = (S.focoIdx == ACHA_FOCO_VOZ && vozDisp) ? 1.0f : 0.0f;
    if (red) {
      S.animVoltar = alvoV;
      S.animVoz = alvoVoz;
    } else {
      S.animVoltar = anim_mola(S.animVoltar, alvoV, dt,
                               alvoV > S.animVoltar ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
      S.animVoz = anim_mola(S.animVoz, alvoVoz, dt,
                            alvoVoz > S.animVoz ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    }
  }
}

static void pt_foco_acha(int id, int nada) {
  (void)nada;
  if (id == ACHA_FOCO_VOLTAR || id == ACHA_FOCO_VOZ) {
    S.focoIdx = id;
  } else if (id >= 0 && id < S.nAchados) {
    S.focoIdx = id;
    S.ultimoPoster = id;
  }
}

static void txt_em(TxtEstilo est, const char *s, float x, float y, float maxW,
                   int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void painel_acha(GfxRect r, float raioPx, float a) {
  if (ajustes_vidro()) {
    gfx_vidro_painel(r, raioPx / r.h, 0.78f, a);
  } else {
    gfx_cor(r, raioPx / r.h, 0.12f, 0.125f, 0.145f, 0.90f * a);
    gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.08f * a);
  }
}

static void desenha_card(int i, const CatItem *ci, float x, float y, float a) {
  float f = S.animCard[i];
  float esc = 1.0f + 0.05f * f;
  GfxRect p = {
    x - ACHA_CARD_W * (esc - 1.0f) * 0.5f,
    y - ACHA_CARD_H * (esc - 1.0f) * 0.5f - 6.0f * f,
    ACHA_CARD_W * esc,
    ACHA_CARD_H * esc
  };
  float raio = 18.0f / p.h;
  const char *arte;
  GLuint tex;
  char sub[96];
  const char *tipoStr = ci ? ci->tipo : S.buscaItens[i].tipo;
  const char *titStr = ci ? (ci->titulo[0] ? ci->titulo : ci->imdb)
                          : S.buscaItens[i].imdb;

  ponteiro_alvo(x, y, ACHA_CARD_W, ACHA_CARD_H + 84.0f, pt_foco_acha, NULL, i, 0);

  if (f > 0.01f)
    botao_luz(p, f, a);

  if (ci) {
    arte = posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster);
    if (!arte || !arte[0]) arte = ci->backdrop[0] ? ci->backdrop : NULL;
  } else {
    arte = posterprov_card_addon("", S.buscaItens[i].imdb, S.buscaItens[i].tmdb,
                                 S.buscaItens[i].tipo, S.buscaItens[i].poster);
  }
  tex = (arte && arte[0]) ? tex_obter_larg(arte, p.w) : 0;

  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(p, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    if (arte && arte[0] && !tex_falhou(arte))
      gfx_esqueleto(p, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
    else
      gfx_cor(p, raio, 0.14f, 0.15f, 0.17f, 0.92f * a);
    gfx_icone((GfxRect){ p.x + p.w * 0.5f - 28.0f, p.y + p.h * 0.5f - 28.0f, 56.0f, 56.0f },
              ehSerieTipo(tipoStr) ? "aj_tv-minimal-play" : "aj_film",
              0.72f, 0.74f, 0.78f, 0.45f * a);
  }

  if (f > 0.01f) {
    float fr, fg, fb;
    botao_cor_foco(&fr, &fg, &fb);
    gfx_anel(p, raio, 3.0f, fr, fg, fb, f * a);
  } else {
    gfx_anel(p, raio, 1.0f, 1, 1, 1, 0.08f * a);
  }

  txt_em(TXT_ILHA_NOME, titStr,
         x, p.y + p.h + 16.0f, ACHA_CARD_W,
         S.focoIdx == i ? 248 : 220, S.focoIdx == i ? 249 : 224, S.focoIdx == i ? 252 : 232, a);

  sub[0] = '\0';
  if (S.achados[i].minutos > 0) {
    formatarDuracaoCurta(S.achados[i].minutos, sub, sizeof sub);
  } else if (ci && ci->meta[0]) {
    snprintf(sub, sizeof sub, "%s", ci->meta);
  }
  if (sub[0]) {
    txt_em(TXT_CAPTION, sub, x, p.y + p.h + 50.0f, ACHA_CARD_W, 166, 172, 184, a);
  }
}

void farolachaui_desenhar(float ox, float oy, float alpha, Uint32 agora) {
  char kick[160];
  int i;
  int vozDisp = farolvozui_disponivel() ? 1 : 0;
  float vx = ox + ACHA_MARGEM;
  float vy = oy + ACHA_VOLTAR_Y;
  const char *rotV = i18n(FT_ACAO_VOLTAR);
  const char *titTela = S.tituloOverride[0] ? S.tituloOverride : i18n(farolacha_rotulo(S.tipo));
  GfxRect rVoltar = {
    vx, vy,
    botao_largura(rotV, "aj_arrow-left", S.nAchados == 0 ? 1 : 0),
    S.nAchados == 0 ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO
  };
  (void)agora;

  garantirRevisao();

  if (S.kicker[0])
    snprintf(kick, sizeof kick, "%s", S.kicker);
  else
    snprintf(kick, sizeof kick, "%s › %s", i18n(FT_NOME), i18n(FT_ACHAR_O_QUE_VER));

  txt_em(TXT_CAPTION, kick, ox + ACHA_MARGEM, oy + 96.0f, 1500.0f, 150, 156, 168, alpha);
  txt_em(TXT_NOV_TITULO, titTela, ox + ACHA_MARGEM, oy + 128.0f,
         1680.0f, 248, 249, 252, alpha);
  if (S.nAchados > 0) {
    if (linha1[0])
      txt_em(TXT_V2_26, linha1, ox + ACHA_MARGEM, oy + 206.0f, 1500.0f, 232, 235, 242, alpha);
    if (linha2[0])
      txt_em(TXT_V2_26, linha2, ox + ACHA_MARGEM, oy + 244.0f, 1500.0f, 186, 192, 204, alpha);
  }

  if (S.nAchados == 0) {
    GfxRect pr = { ox + ACHA_MARGEM, oy + ACHA_ROW_Y, 820.0f, 160.0f };
    painel_acha(pr, 28.0f, alpha);
    gfx_cor((GfxRect){ pr.x + 32.0f, pr.y + 52.0f, 56.0f, 56.0f }, 0.31f, 1, 1, 1, 0.07f * alpha);
    gfx_icone((GfxRect){ pr.x + 48.0f, pr.y + 68.0f, 24.0f, 24.0f }, "aj_compass",
              0.95f, 0.95f, 0.94f, alpha);
    txt_em(TXT_ILHA_NOME, linha1[0] ? linha1 : i18n("Nada encontrado"),
           pr.x + 112.0f, pr.y + 64.0f, pr.w - 144.0f, 243, 242, 239, alpha);
  } else {
    for (i = 0; i < S.nAchados && i < FAROL_ACHA_UI_MAX; i++) {
      const CatItem *ci = NULL;
      float cx;
      if (S.modoBusca && !S.buscaItens[i].noCatalogo) {
        ci = NULL;
      } else {
        ci = farolacha_item(&S.achados[i], S.rev);
        if (!ci) break;
      }
      cx = ox + ACHA_MARGEM + (float)i * (ACHA_CARD_W + ACHA_CARD_GAP);
      desenha_card(i, ci, cx, oy + ACHA_ROW_Y, alpha);
    }
  }

  botao_pilula(rVoltar, rotV, "aj_arrow-left", S.animVoltar, S.nAchados == 0 ? 1 : 0, 0, alpha);
  ponteiro_alvo(rVoltar.x, rVoltar.y, rVoltar.w, rVoltar.h, pt_foco_acha, NULL, ACHA_FOCO_VOLTAR, 0);
  if (vozDisp) {
    const char *rotFal = i18n(farolvozui_rotulo_botao());
    const char *icFal = farolvozui_icone_botao();
    GfxRect rVoz = {
      rVoltar.x + rVoltar.w + BOTAO_GAP,
      vy + (rVoltar.h - BOTAO_H_SECUNDARIO) * 0.5f,
      botao_largura(rotFal, icFal, 0),
      BOTAO_H_SECUNDARIO
    };
    botao_pilula(rVoz, rotFal, icFal, S.animVoz, 0, 0, alpha);
    ponteiro_alvo(rVoz.x, rVoz.y, rVoz.w, rVoz.h, pt_foco_acha, NULL, ACHA_FOCO_VOZ, 0);
  }
}

int farolachaui_pedido_bate(const CatItem *c, const FarolAchaPedido *p) {
  if (!c || !p || (!p->imdb[0] && p->tmdb <= 0 && !p->titulo[0])) return 0;
  return itemBatePedido(c, p);
}

int farolachaui_resolver_pedido(const FarolAchaPedido *p, unsigned *revisaoOut) {
  int tentativa;
  char basePed[64];
  int temBase, temId;

  if (revisaoOut) *revisaoOut = 0u;
  if (!p || (!p->imdb[0] && p->tmdb <= 0 && !p->titulo[0])) return -1;
  temId = (p->imdb[0] != '\0' || p->tmdb > 0);
  idBaseDe(p->imdb, basePed, sizeof basePed);
  temBase = basePed[0] != '\0';

  for (tentativa = 0; tentativa < 3; tentativa++) {
    unsigned r = cat_revisao();
    int nc = cat_n();
    int i, found = -1;

    if (temId) {
      if (r == p->rev && p->indice >= 0 && p->indice < nc) {
        const CatItem *cp = cat_item(p->indice);
        if (cp && itemBateComId(cp, p, basePed, temBase))
          found = p->indice;
      }
      if (found < 0) {
        for (i = 0; i < nc; i++) {
          const CatItem *c = cat_item(i);
          if (!c) continue;
          if (p->tipo[0] && c->tipo[0] && ehSerieTipo(p->tipo) != ehSerieTipo(c->tipo))
            continue;
          if (p->imdb[0] && !strcmp(c->imdb, p->imdb)) {
            found = i;
            break;
          }
          if (temBase && found < 0) {
            char baseCat[64];
            idBaseDe(c->imdb, baseCat, sizeof baseCat);
            if (baseCat[0] && !strcmp(baseCat, basePed)) found = i;
          }
          if (found < 0 && itemBateComId(c, p, basePed, temBase))
            found = i;
        }
      }
    } else {
      if (r == p->rev) {
        if (p->indice >= 0 && p->indice < nc) {
          const CatItem *cp = cat_item(p->indice);
          if (itemBateSemId(cp, p)) found = p->indice;
        }
      } else {
        int matches = 0, matchIdx = -1;
        for (i = 0; i < nc; i++) {
          const CatItem *c = cat_item(i);
          if (itemBateSemId(c, p)) {
            matchIdx = i;
            matches++;
          }
        }
        if (matches == 1) found = matchIdx;
      }
    }

    if (cat_revisao() == r) {
      if (revisaoOut) *revisaoOut = r;
      return found;
    }
  }
  return -1;
}

int farolachaui_pediu_abrir(FarolAchaPedido *pedido) {
  if (!S.pedidoTem) return 0;
  if (pedido) *pedido = S.pedido;
  S.pedidoTem = 0;
  memset(&S.pedido, 0, sizeof S.pedido);
  return 1;
}

int farolachaui_pediu_voz(void) {
  if (!S.pediuVoz) return 0;
  S.pediuVoz = 0;
  return 1;
}

int           farolachaui_n(void)          { return S.nAchados; }
int           farolachaui_foco(void)       { return S.focoIdx; }
FarolAchaTipo farolachaui_tipo(void)       { return S.tipo; }
unsigned      farolachaui_revisao(void)    { return S.rev; }
unsigned      farolachaui_semente(void)    { return S.semente; }
int           farolachaui_em_fallback(void){ return S.fallbackAtivo; }
const char   *farolachaui_l1(void)         { return linha1; }
const char   *farolachaui_l2(void)         { return linha2; }
