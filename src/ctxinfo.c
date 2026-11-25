// Ver ctxinfo.h.
#include "ctxinfo.h"
#include "extras.h"
#include "notasui.h"
#include "imdbnota.h"
#include "amigostitulo.h"
#include "agenda.h"
#include "logotitulo.h"
#include "descoberta.h"
#include "badges.h"
#include "tex_cache.h"
#include "text.h"
#include "sinopse.h"
#include "idioma.h"
#include "idiomacod.h"
#include "linguas.h"
#include "ajustes.h"
#include "layout.h"
#include "farolinfo.h"
#include "faroltexto.h"
#include "ondever.h"
#include "diario.h"
#include "streams.h"
#include "fonteantecipa.h"
#include "farol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#define NV_ESCALA_TELA   // mede pela tela virtual, como ctxmenu.c (escala.h)
#include "escala.h"

static long long (*ctxiRelogioFn)(void);
void ctxinfo_teste_relogio(long long (*epochS)(void)) { ctxiRelogioFn = epochS; }
static long long ctxiAgoraS(void) { return ctxiRelogioFn ? ctxiRelogioFn() : (long long)time(NULL); }

#define CTXI_BORDA   48.0f   // a mesma margem minima do menu (CTX_BORDA)
#define CTXI_AO_LADO 30.0f   // do cartaz ao menu (CTX_AO_LADO)

// ------------------------------------------------------------------ geometria
static float prender(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void ctxinfo_geometria(const GfxRect *cartaz, float menuXPadrao, float centroX,
                       float menuW, CtxInfoGeo *g) {
  const float W = NV_TELA_W, B = CTXI_BORDA, G = CTXI_GAP;
  memset(g, 0, sizeof *g);
  g->menuX = menuXPadrao;
  if (cartaz) {
    float dir0 = cartaz->x + cartaz->w + CTXI_AO_LADO, dirLivre = W - B - dir0;
    float esqFim = cartaz->x - CTXI_AO_LADO, esqLivre = esqFim - B;
    float junto = menuW + G;
    if (dirLivre >= junto + CTXI_W) {
      g->menuX = dir0; g->infoW = CTXI_W; g->infoX = dir0 + junto; g->lado = 1;
    } else if (esqLivre >= junto + CTXI_W) {
      g->menuX = esqFim - menuW; g->infoW = CTXI_W; g->infoX = g->menuX - G - CTXI_W; g->lado = -1;
    } else if (dirLivre >= esqLivre && dirLivre >= junto + CTXI_W_MIN) {
      g->menuX = dir0; g->infoW = dirLivre - junto; g->infoX = dir0 + junto; g->lado = 1;
    } else if (esqLivre >= junto + CTXI_W_MIN) {
      g->menuX = esqFim - menuW; g->infoW = esqLivre - junto;
      g->infoX = g->menuX - G - g->infoW; g->lado = -1;
    } else {
      // Nenhum lado leva os dois: o menu fica onde ficaria sozinho e a
      // extensao vai para o OUTRO lado do cartaz, se houver lugar la.
      int menuADireita = menuXPadrao >= cartaz->x + cartaz->w * 0.5f;
      float livre = menuADireita ? esqLivre : dirLivre;
      if (livre >= CTXI_W_SOLTA) {
        g->infoW = livre < CTXI_W ? livre : CTXI_W;
        g->infoX = menuADireita ? esqFim - g->infoW : dir0;
        g->lado = menuADireita ? -1 : 1;
        g->separada = 1;
      }
    }
    return;
  }
  // Sem cartaz. No centro da tela, o grupo inteiro e centrado; fora dele (o
  // painel de Salvos), o menu fica no centro pedido e a extensao vai para o
  // lado com mais espaco.
  if (centroX < 0.0f || (centroX > W * 0.5f - 1.0f && centroX < W * 0.5f + 1.0f)) {
    float total = menuW + G + CTXI_W;
    g->menuX = prender(W * 0.5f - total * 0.5f, B, W - B - total);
    g->infoX = g->menuX + menuW + G; g->infoW = CTXI_W; g->lado = 1;
    return;
  }
  g->menuX = centroX - menuW * 0.5f;
  { float esq = g->menuX - G - B, dir = W - B - (g->menuX + menuW + G);
    if (esq >= dir) {
      g->infoW = esq < CTXI_W ? esq : CTXI_W;
      g->infoX = g->menuX - G - g->infoW; g->lado = -1;
    } else {
      g->infoW = dir < CTXI_W ? dir : CTXI_W;
      g->infoX = g->menuX + menuW + G; g->lado = 1;
    }
    if (g->infoW < CTXI_W_SOLTA) { g->infoW = 0.0f; g->lado = 0; } }
}

void ctxinfo_cartao_geo(const GfxRect *poster, float h, float menuW, CtxCartaoGeo *g) {
  ctxinfo_cartao_geo_w(poster, CTXI_CARTAO_W < poster->w ? poster->w : CTXI_CARTAO_W, h, menuW, g);
}
void ctxinfo_cartao_geo_w(const GfxRect *poster, float w, float h, float menuW, CtxCartaoGeo *g) {
  const float W = NV_TELA_W, H = NV_TELA_H, B = CTXI_BORDA, G = CTXI_GAP;
  float total = w + G + menuW, x;
  if (h < poster->h) h = poster->h;
  memset(g, 0, sizeof *g);
  if (poster->x + total <= W - B) {
    x = poster->x; g->lado = 1; g->menuX = x + w + G;
  } else if (poster->x + poster->w - total >= B) {
    x = poster->x + poster->w - w; g->lado = -1; g->menuX = x - G - menuW;
  } else {
    // Nenhum lado leva os dois: prende o grupo na tela, do lado com mais chao.
    int dir = (W - B - (poster->x + poster->w)) >= (poster->x - B);
    float gx = dir ? poster->x : poster->x + poster->w - total;
    gx = prender(gx, B, W - B - total);
    g->lado = dir ? 1 : -1;
    if (dir) { x = gx; g->menuX = gx + w + G; } else { g->menuX = gx; x = gx + menuW + G; }
  }
  g->cartao = (GfxRect){ x, prender(poster->y, B, H - B - h > B ? H - B - h : B), w, h };
}

// --------------------------------------------------------------------- dados
static int ehSerie(const CatItem *ci) { return ci && !strcmp(ci->tipo, "series"); }
static int temId(const CatItem *ci) { return ci && ci->imdb[0] == 't' && ci->imdb[1] == 't'; }

void ctxinfo_abrir(const CatItem *ci) {
  if (!temId(ci)) return;
  extras_resumo_pedir(ci->imdb, ehSerie(ci), ci->sinopse[0] == 0);
}

// Tudo o que a extensao mostra, montado a cada quadro (barato: copias de texto
// curtas). So a quebra da sinopse tem cache.
#define CTXI_SIN_LINHAS 4
typedef struct {
  char meta[200], genero[200], classif[16];
  int  cru[EX_NFONTES], notasCarregando, temNotas;
  const char *sinopse;
  char farolInfo[160];
  char prog[200]; float progFrac;
  char amigos[220], agenda[200];
  int  salvo, visto;
} Modelo;
static ExResumo resumo;   // copia do quadro (a sinopse aponta para ca)

static int ehPalavraDeTipo(const char *s) {
  static const char *const T[] = { "Filme", "Série", "Programa de TV", "Movie",
                                   "Series", "TV Show", "Show", NULL };
  int i;
  for (i = 0; T[i]; i++) if (!strcmp(s, T[i]) || !strcmp(s, i18n(T[i]))) return 1;
  return 0;
}

static void juntar(char *dst, size_t cap, const char *parte) {
  size_t n = strlen(dst);
  if (!parte || !parte[0] || n + 4 >= cap) return;
  snprintf(dst + n, cap - n, "%s%s", n ? " \xc2\xb7 " : "", parte);
}

static int temDuracao(const char *meta) {
  const char *p;
  if (strstr(meta, "min")) return 1;
  for (p = meta; *p; p++)
    if (isdigit((unsigned char)p[0]) && (p[1] == 'h' || (p[1] == ' ' && p[2] == 'h'))) return 1;
  return 0;
}

static int metaMinutos(const char *s) {
  int h = 0, min = 0;
  const char *p = s;
  if (!p) return 0;
  while (*p) {
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (!*p) break;
    {
      char *fim = NULL;
      long v = strtol(p, &fim, 10);
      p = fim;
      while (*p == ' ') p++;
      if (*p == 'h' && (!p[1] || p[1] == ' ' || p[1] == '\xc2') && v > 0 && v < 24)
        h = (int)v;
      else if (!strncmp(p, "min", 3) && v > 0 && v < 1000)
        min = (int)v;
    }
  }
  return h * 60 + min;
}

static void ctxiIdiomaMidia(const char *cod, char *dst, size_t cap) {
  char base[12];
  const char *nm;
  size_t i = 0;
  if (!dst || !cap) return;
  dst[0] = 0;
  if (!cod || !cod[0]) return;
  while (cod[i] && cod[i] != '-' && cod[i] != '_' && i + 1 < sizeof base) {
    base[i] = (char)tolower((unsigned char)cod[i]);
    i++;
  }
  base[i] = 0;
  if (!strcmp(base, "pob") || !strcmp(base, "ptb") || !strcmp(base, "br") ||
      !strcmp(base, "por"))
    snprintf(base, sizeof base, "pt");
  else if (!strcmp(base, "eng")) snprintf(base, sizeof base, "en");
  else if (!strcmp(base, "spa") || !strcmp(base, "esp")) snprintf(base, sizeof base, "es");
  nm = i18n(ling_nome(base[0] ? base : cod));
  snprintf(dst, cap, "%s", nm && nm[0] ? nm : cod);
}

static int ctxiMesAno(const char *s, char *dst, size_t cap) {
  static const char *const MES_PT[12] = {
    "janeiro", "fevereiro", "março", "abril", "maio", "junho",
    "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"
  };
  int ano, mes;
  if (!s || strlen(s) != 7 || s[4] != '-') return 0;
  ano = atoi(s); mes = atoi(s + 5);
  if (ano < 1900 || mes < 1 || mes > 12) return 0;
  idioma_mes_ano(ajustes_idioma(), i18n(MES_PT[mes - 1]), ano, dst, cap);
  return dst[0] != 0;
}

static int ctxiDecorrido(const char *s, char *dst, size_t cap) {
  char *fim = NULL;
  long n;
  if (!s || !isdigit((unsigned char)s[0])) return 0;
  n = strtol(s, &fim, 10);
  if (n <= 0 || !fim || !fim[0]) return 0;
  if (!strcmp(fim, "min")) { snprintf(dst, cap, i18n("%d min"), (int)n); return 1; }
  if (!strcmp(fim, "h"))   { snprintf(dst, cap, i18n("%d h"), (int)n); return 1; }
  if ((!strcmp(fim, "d") && n == 7) || (!strcmp(fim, "w") && n == 1)) {
    snprintf(dst, cap, "%s", i18n("7 dias")); return 1;
  }
  if (!strcmp(fim, "y") && n >= 2) { snprintf(dst, cap, i18n("%d anos"), (int)n); return 1; }
  return 0;
}

static const char *ctxiModeloInfo(const char *m) {
  static const struct { const char *k, *v; } TAB[] = {
    { "FT_INFO_QUALI", FT_INFO_QUALI },
    { "FT_INFO_QUALI_AUDIO", FT_INFO_QUALI_AUDIO },
    { "FT_INFO_QUALI_SO", FT_INFO_QUALI_SO },
    { "FT_INFO_IDIOMA", FT_INFO_IDIOMA },
    { "FT_INFO_LEGENDA", FT_INFO_LEGENDA },
    { "FT_INFO_SOBE", FT_INFO_SOBE },
    { "FT_INFO_SOBE_SO", FT_INFO_SOBE_SO },
    { "FT_INFO_CAI", FT_INFO_CAI },
    { "FT_INFO_NOTA_CAI", FT_INFO_NOTA_CAI },
    { "FT_INFO_FALTAM", FT_INFO_FALTAM },
    { "FT_INFO_MESMO_NIVEL", FT_INFO_MESMO_NIVEL },
    { "FT_INFO_TERMINA", FT_INFO_TERMINA },
    { "FT_INFO_DUAS_NOITES", FT_INFO_DUAS_NOITES },
    { "FT_INFO_ONDE", FT_INFO_ONDE },
    { "FT_INFO_AMIGOS_TERM", FT_INFO_AMIGOS_TERM },
    { "FT_INFO_AMIGO_TERM", FT_INFO_AMIGO_TERM },
    { "FT_INFO_AMIGOS_GOST", FT_INFO_AMIGOS_GOST },
    { "FT_INFO_AMIGO_GOST", FT_INFO_AMIGO_GOST },
    { "FT_INFO_SAGA", FT_INFO_SAGA },
    { "FT_INFO_SAGA_VOCE_VIU", FT_INFO_SAGA_VOCE_VIU },
    { "FT_INFO_REVER", FT_INFO_REVER },
    { "FT_INFO_REVER_SIMPLES", FT_INFO_REVER_SIMPLES },
    { "FT_INFO_RETOMADA", FT_INFO_RETOMADA },
    { "FT_INFO_RETOMADA_SIMPLES", FT_INFO_RETOMADA_SIMPLES },
  };
  size_t i;
  for (i = 0; i < sizeof TAB / sizeof *TAB; i++)
    if (!strcmp(m, TAB[i].k)) return TAB[i].v;
  return NULL;
}

static int ctxiFormatarInfo(const FarolInfoLinha *l, char *dst, size_t cap) {
  FarolInfoLinha c = *l;
  const char *lit, *p;
  size_t o = 0;
  int ai = 0;
  if (!dst || !cap || !l->m[0]) return 0;
  dst[0] = 0;
  if (!strcmp(c.m, "FT_INFO_RETOMADA") && c.na == 3) {
    if (!ctxiDecorrido(c.a[2], c.a[2], sizeof c.a[2])) {
      snprintf(c.m, sizeof c.m, "FT_INFO_RETOMADA_SIMPLES");
      c.na = 2;
    }
  } else if ((!strcmp(c.m, "FT_INFO_REVER") || !strcmp(c.m, "FT_INFO_REVER_SIMPLES")) && c.na >= 1) {
    if (!ctxiMesAno(c.a[0], c.a[0], sizeof c.a[0])) return 0;
  } else if (!strcmp(c.m, "FT_INFO_QUALI") && c.na == 3) {
    ctxiIdiomaMidia(c.a[2], c.a[2], sizeof c.a[2]);
  } else if (!strcmp(c.m, "FT_INFO_QUALI_AUDIO") && c.na == 2) {
    ctxiIdiomaMidia(c.a[1], c.a[1], sizeof c.a[1]);
  } else if ((!strcmp(c.m, "FT_INFO_IDIOMA") || !strcmp(c.m, "FT_INFO_LEGENDA")) && c.na == 1) {
    ctxiIdiomaMidia(c.a[0], c.a[0], sizeof c.a[0]);
  }
  lit = ctxiModeloInfo(c.m);
  if (!lit) return 0;
  p = i18n(lit);
  while (*p && o + 1 < cap) {
    if (*p != '%') { dst[o++] = *p++; continue; }
    p++;
    if (*p == '%') { dst[o++] = *p++; continue; }
    if (ai >= c.na) return 0;
    if (*p == 'd' || *p == 'i') {
      char num[32];
      snprintf(num, sizeof num, "%d", atoi(c.a[ai++]));
      snprintf(dst + o, cap - o, "%s", num);
      o += strlen(dst + o);
      p++;
    } else if (*p == 's') {
      snprintf(dst + o, cap - o, "%s", c.a[ai++]);
      o += strlen(dst + o);
      p++;
    } else return 0;
  }
  dst[o] = 0;
  return dst[0] != 0;
}

static void montarFarolInfo(const CatItem *ci, const Modelo *m, char *dst, size_t cap) {
  FarolInfoLinha l;
  int serie = ehSerie(ci);
  long long epoch = ctxiAgoraS();
  if (!ci || !dst || !cap) return;
  dst[0] = 0;
  memset(&l, 0, sizeof l);

  // 1) Qualidade confirmada tocavel nesta TV (fontes ja carregadas para o titulo)
  if (temId(ci) && stream_lista_do_alvo(ci->imdb)) {
    FarolInfoFonte fnt[16];
    FarolInfoLinha qLi[2];
    FarolInfoCaps caps = { -1, -1 };
    int hdr = -1, dv = -1, hevc = -1, av1 = -1;
    int ns = stream_n(), nf = 0, i;
    stream_capacidades(&hdr, &dv, &hevc, &av1);
    caps.hdr = hdr;
    caps.dv = dv;
    for (i = 0; i < ns && nf < 16; i++) {
      const Stream *s = stream_item(i);
      int est, cacheOk;
      if (!s) continue;
      memset(&fnt[nf], 0, sizeof fnt[0]);
      if (s->altura >= 2000 || (s->badges & badges_bit("r-4k"))) fnt[nf].altura = 2160;
      else if (s->altura >= 1000 || (s->badges & badges_bit("r-1080"))) fnt[nf].altura = 1080;
      else if (s->altura >= 700 || (s->badges & badges_bit("r-720"))) fnt[nf].altura = 720;
      else fnt[nf].altura = s->altura;
      fnt[nf].dv = (s->dolbyVision ||
                    (s->badges & (badges_bit("v-dv") | badges_bit("a-atmos-dv") |
                                  badges_bit("a-truehd-dv") | badges_bit("a-dd-dv")))) ? 1 : 0;
      fnt[nf].hdr = (!fnt[nf].dv &&
                     (s->badges & (badges_bit("v-hdr10plus") | badges_bit("v-hdr10") |
                                   badges_bit("v-hdr") | badges_bit("v-hlg")))) ? 1 : 0;
      est = stream_fonte_estado(i);
      cacheOk = s->emCache && !s->cacheNegado && (!s->foraCache || s->cacheNativo) &&
                !s->urlP2P && !s->naoVideo;
      if (s->naoVideo || stream_incompativel(i) || est == FA_RUIM)
        fnt[nf].toca = 0;
      else if ((est == FA_OK || cacheOk) && stream_fonte_disponivel(s))
        fnt[nf].toca = 1;
      else
        fnt[nf].toca = -1;
      snprintf(fnt[nf].audio, sizeof fnt[nf].audio, "%.*s",
               (int)strcspn(s->audioIdiomas, ","), s->audioIdiomas);
      snprintf(fnt[nf].legenda, sizeof fnt[nf].legenda, "%.*s",
               (int)strcspn(s->legendaIdiomas, ","), s->legendaIdiomas);
      nf++;
    }
    if (nf > 0 && farolinfo_qualidade(qLi, 2, fnt, nf, &caps) > 0 &&
        ctxiFormatarInfo(&qLi[0], dst, cap))
      return;
  }

  // 2) Retomada no Diario (quando nao ha progresso visivel na linha acima)
  if (serie && !m->prog[0] && ci->imdb[0] && diario_carregado(0)) {
    int rT = 0, rE = 0;
    long long rQ = 0;
    if (diario_ultimo_episodio(0, ci->imdb, &rT, &rE, &rQ) &&
        farolinfo_retomada(&l, rT, rE, rQ, epoch) == 1 &&
        ctxiFormatarInfo(&l, dst, cap))
      return;
  }

  // 3) Rever (filme visto no Diario)
  if (!serie && ci->imdb[0] && diario_carregado(0)) {
    DiarioTitulo dt;
    memset(&dt, 0, sizeof dt);
    if (diario_titulo(0, ci->imdb, &dt) && dt.vezes > 0 && dt.ultimo > 0) {
      time_t tv = (time_t)dt.ultimo;
      struct tm tmv;
      if (localtime_r(&tv, &tmv) &&
          farolinfo_rever(&l, tmv.tm_mon + 1, tmv.tm_year + 1900, (int)dt.nota10) == 1 &&
          ctxiFormatarInfo(&l, dst, cap))
        return;
    }
  }

  // 4) Sobe/cai de temporada (serie com episodios avaliados)
  if (serie && extras_do_titulo(ci->imdb, ci->tmdb) && extras_n_temporadas() > 0) {
    FarolInfoTemp temps[16];
    int nt = extras_n_temporadas(), ti, nT = 0, vEp = 0, tEp = 0, falt = 0;
    for (ti = 0; ti < nt && nT < 16; ti++) {
      int num = extras_temporada_numero(ti), ne = extras_n_eps(ti), ei, soma = 0, cnt = 0;
      if (num <= 0) continue;
      for (ei = 0; ei < ne; ei++) {
        int nt10 = extras_ep_nota(ti, ei);
        if (nt10 > 0) { soma += nt10; cnt++; }
      }
      temps[nT].numero = num;
      temps[nT].comNota = cnt;
      temps[nT].notaX10 = cnt > 0 ? (soma + cnt / 2) / cnt : 0;
      nT++;
    }
    (void)extras_progresso_serie(&vEp, &tEp);
    if (tEp > vEp && vEp > 0) falt = tEp - vEp;
    if (nT >= 2 && farolinfo_continuar(&l, temps, nT, falt, resumo.duracao) == 1 &&
        ctxiFormatarInfo(&l, dst, cap))
      return;
  }

  // 5) Saga (colecao com >= 2 filmes)
  if (!serie && ci->tmdb > 0 && extras_n_colecao() >= 2) {
    int ncol = extras_n_colecao(), pos = 0, vis = 0, i;
    for (i = 0; i < ncol; i++) {
      long ctmdb = extras_colecao_tmdb(i);
      if (ctmdb == ci->tmdb) pos = i + 1;
      if (diario_carregado(0) && ctmdb > 0) {
        char idT[32];
        DiarioTitulo dtc;
        snprintf(idT, sizeof idT, "tmdb:%ld", ctmdb);
        if (diario_titulo(0, idT, &dtc) && dtc.vezes > 0) vis++;
      }
    }
    if (pos > 0 && farolinfo_saga(&l, pos, ncol, vis) == 1 &&
        ctxiFormatarInfo(&l, dst, cap))
      return;
  }

  // 6) Tempo / Termina as (quando duracao medida e relogio valido existem)
  {
    int min = resumo.duracao > 0 ? resumo.duracao : metaMinutos(ci->meta);
    if (min > 0 && epoch > 1700000000LL) {
      time_t tv = (time_t)epoch;
      struct tm tmv;
      if (localtime_r(&tv, &tmv) &&
          farolinfo_tempo(&l, &tmv, ajustes_relogio_12h(), min, 0) == 1 &&
          ctxiFormatarInfo(&l, dst, cap))
        return;
    }
  }

  // 7) Onde assistir
  if (temId(ci) && ondever_n(ci->imdb) > 0) {
    OndeVer ov;
    if (ondever_item(ci->imdb, 0, &ov) && ov.nome[0] &&
        farolinfo_onde(&l, ov.nome) == 1 &&
        ctxiFormatarInfo(&l, dst, cap))
      return;
  }

  // 8) Amigos
  if (temId(ci)) {
    AmigosTitulo amg;
    if (amigostitulo_obter(ci->imdb, &amg) && amg.n > 0) {
      FarolInfoAmigo fa[AMT_MAX];
      int nfa = 0, i;
      for (i = 0; i < amg.n && nfa < AMT_MAX; i++) {
        amigostitulo_primeiro_nome(amg.a[i].nome, fa[nfa].nome, sizeof fa[0].nome);
        if (!fa[nfa].nome[0]) continue;
        fa[nfa].terminou = amg.a[i].terminou ? 1 : 0;
        fa[nfa].gostou = amg.a[i].gostou ? 1 : 0;
        nfa++;
      }
      if (nfa > 0 && farolinfo_amigos(&l, fa, nfa) == 1 &&
          ctxiFormatarInfo(&l, dst, cap))
        return;
    }
  }
}

static void montar(const CatItem *ci, const CtxInfoEstado *st, Modelo *m) {
  int f, serie = ehSerie(ci);
  memset(m, 0, sizeof *m);
  memset(&resumo, 0, sizeof resumo);
  if (temId(ci)) extras_resumo_obter(ci->imdb, &resumo);
  // META: tipo · "2022 · 3 temporadas" · duracao quando a ficha nao a trouxe.
  juntar(m->meta, sizeof m->meta, !strcmp(ci->tipo, "movie") ? i18n("Filme")
                                  : serie ? i18n("Série") : "");
  juntar(m->meta, sizeof m->meta, ci->meta);
  if (!serie && resumo.duracao > 0 && !temDuracao(ci->meta)) {
    char d[32];
    desc_duracao_min(resumo.duracao, d, sizeof d);
    juntar(m->meta, sizeof m->meta, d);
  }
  { const char *g = ci->genero;
    int n = 0;
    while (*g && n < 3) {
      char item[64];
      size_t k = 0;
      while (*g == ' ' || *g == ',') g++;
      while (*g && *g != ',' && !(g[0] == '\xc2' && g[1] == '\xb7') && k + 1 < sizeof item)
        item[k++] = *g++;
      while (k && item[k - 1] == ' ') k--;
      item[k] = 0;
      if (g[0] == '\xc2' && g[1] == '\xb7') g += 2;
      else if (*g == ',') g++;
      if (item[0] && !ehPalavraDeTipo(item)) { juntar(m->genero, sizeof m->genero, i18n(item)); n++; }
    } }
  snprintf(m->classif, sizeof m->classif, "%s", ci->classificacao[0] ? ci->classificacao : resumo.cert);

  // NOTAS: o que o resumo trouxe; o IMDb cai na reserva do catalogo, como na
  // pagina de titulo (detail.c, notasDados).
  for (f = 0; f < EX_NFONTES; f++) m->cru[f] = resumo.cru[f];
  if (!m->cru[EX_IMDB] && ajustes_mdblist_fonte(EX_IMDB) && ci->imdb[0])
    m->cru[EX_IMDB] = imdbnota_obter(ci->imdb, ci->nota, serie);
  for (f = 0; f < EX_NFONTES; f++) if (m->cru[f] > 0 && ajustes_nota_titulo(f)) m->temNotas = 1;
  m->notasCarregando = resumo.carregando && !resumo.pronto;

  // SINOPSE: a do titulo; senao a do TMDB no idioma; em ingles, a do Trakt.
  m->sinopse = ci->sinopse[0] ? ci->sinopse : resumo.sinopse[0] ? resumo.sinopse
             : (!strcmp(idioma_iso(ajustes_idioma()), "en") ? resumo.sinopseEn : "");

  // ONDE A PESSOA PAROU. Os mesmos textos da fileira e da Biblioteca.
  { char ep[48] = "", resto[64] = "";
    int emCurso = ci->progresso > 0 && ci->progresso < 100;
    if (serie && ci->temporada > 0 && ci->episodio > 0)
      snprintf(ep, sizeof ep, i18n("T%dE%d"), ci->temporada, ci->episodio);
    if (emCurso && ci->restanteMin > 0) snprintf(resto, sizeof resto, i18n("%d min restantes"), ci->restanteMin);
    else if (emCurso) snprintf(resto, sizeof resto, i18n("%d%% assistido"), ci->progresso);
    if (emCurso) {
      juntar(m->prog, sizeof m->prog, ep);
      if (serie && ci->nomeEpisodio[0]) juntar(m->prog, sizeof m->prog, ci->nomeEpisodio);
      juntar(m->prog, sizeof m->prog, resto);
      m->progFrac = (float)ci->progresso / 100.0f;
    } else if (ep[0] && ci->progresso == 0) {
      // O "a seguir" da fileira: o proximo episodio, ainda nao comecado.
      juntar(m->prog, sizeof m->prog, i18n("A seguir"));
      juntar(m->prog, sizeof m->prog, ep);
      if (ci->nomeEpisodio[0]) juntar(m->prog, sizeof m->prog, ci->nomeEpisodio);
    } }

  { AmigosTitulo t;
    if (temId(ci) && amigostitulo_obter(ci->imdb, &t))
      amigostitulo_linha_ilha(&t, m->amigos, sizeof m->amigos); }
  if (serie && temId(ci)) agenda_frase(ci->imdb, m->agenda, sizeof m->agenda);
  m->salvo = st && st->salvo;
  m->visto = st && st->visto == 1;
  montarFarolInfo(ci, m, m->farolInfo, sizeof m->farolInfo);
}

// A SINOPSE QUEBRADA UMA VEZ. A chave e o texto, a largura e o idioma; o
// quadro seguinte so desenha as linhas (txt_linha ja guarda a textura).
static struct { char chave[96]; float w; int idioma, tamanho, n, max; char l[CTXI_SIN_LINHAS][300]; } sinCache;
static int sinMax = CTXI_SIN_LINHAS;

static void quebrarSinopse(const char *s, float w) {
  char chave[96];
  const char *p = s;
  int n = 0;
  snprintf(chave, sizeof chave, "%.80s|%zu", s, strlen(s));
  if (sinCache.w == w && sinCache.max == sinMax && sinCache.tamanho == ajustes_sinopse_tamanho() && sinCache.idioma == ajustes_idioma() && !strcmp(sinCache.chave, chave)) return;
  memset(&sinCache, 0, sizeof sinCache);
  snprintf(sinCache.chave, sizeof sinCache.chave, "%s", chave);
  sinCache.w = w; sinCache.idioma = ajustes_idioma(); sinCache.max = sinMax; sinCache.tamanho = ajustes_sinopse_tamanho();
  while (*p && n < sinopse_linhas(TXT_ILHA_TEXTO, 28, sinMax)) {
    char linha[300] = "";
    const char *corte = NULL;
    while (*p == ' ') p++;
    while (*p) {
      char tent[300];
      size_t k = txt_token_tam(p), nl = strlen(linha);
      if (nl + k + 2 >= sizeof tent) break;
      snprintf(tent, sizeof tent, "%s%s%.*s", linha, nl ? " " : "", (int)k, p);
      if (nl && (float)txt_largura(sinopse_estilo(TXT_ILHA_TEXTO), tent) > w) { corte = p; break; }
      snprintf(linha, sizeof linha, "%s", tent);
      p += k;
      while (*p == ' ') p++;
    }
    // A ULTIMA linha leva a reticencia quando ainda sobra texto.
    if (n == sinopse_linhas(TXT_ILHA_TEXTO, 28, sinMax) - 1 && (corte || *p)) {
      size_t nl = strlen(linha);
      for (;;) {
        char tent[310];
        snprintf(tent, sizeof tent, "%s\xe2\x80\xa6", linha);
        if ((float)txt_largura(sinopse_estilo(TXT_ILHA_TEXTO), tent) <= w || nl == 0) {
          snprintf(sinCache.l[n], sizeof sinCache.l[n], "%s", tent); break; }
        // tira a ultima palavra
        while (nl && linha[nl - 1] != ' ') nl--;
        while (nl && linha[nl - 1] == ' ') nl--;
        linha[nl] = 0;
      }
    } else snprintf(sinCache.l[n], sizeof sinCache.l[n], "%s", linha);
    n++;
    if (!corte && !*p) break;
  }
  sinCache.n = n;
}

// -------------------------------------------------------------------- desenho
#define PAD       14.0f    // do vidro ao conteudo (CTX_ILHA_PAD)
#define INSET     20.0f    // do conteudo ao texto (linhaCtx)
#define ARTE_H   224.0f
#define LOGO_W   300.0f
#define LOGO_H    64.0f
#define NOTAS_H   30.0f
#define SIN_LEAD  sinopse_entrelinha(TXT_ILHA_TEXTO, 28.0f)
#define LINHA_H   34.0f
#define ICONE     20.0f

static const char *arteDe(const CatItem *ci) {
  if (ci->backdrop[0]) return ci->backdrop;
  if (ci->backdropTmdb[0]) return ci->backdropTmdb;
  if (ci->backdropCatalogo[0]) return ci->backdropCatalogo;
  return NULL;
}

// Uma linha "icone + texto" do fim da extensao.
static float linhaIcone(const char *icone, const char *texto, float x, float y, float w,
                        float a, int desenhar) {
  TxtLinha t = txt_linha_corta(TXT_ILHA_META, texto, 243, 242, 239, 255, w - ICONE - 12.0f);
  if (desenhar) {
    gfx_icone((GfxRect){ x, y + (LINHA_H - ICONE) * 0.5f, ICONE, ICONE }, icone,
              .953f, .949f, .937f, .62f * a);
    txt_desenhar_alpha(t, x + ICONE + 12.0f, y + (LINHA_H - (float)t.h) * 0.5f, .78f * a);
  }
  return LINHA_H;
}

// O MESMO LACO mede e desenha: a altura nunca discorda do desenho.
static float fazer(const CatItem *ci, const CtxInfoEstado *st, float x, float y, float w,
                   float a, float ca, int desenhar) {
  Modelo m;
  const char *arte;
  float y0 = y, cx, cw;
  if (!ci) return 0.0f;
  montar(ci, st, &m);
  arte = arteDe(ci);
  cx = x + PAD + INSET; cw = w - 2.0f * (PAD + INSET);
  y += PAD;

  // ARTE + LOGO. A arte e a de paisagem do proprio titulo (a home ja a tem no
  // cache de textura); sem ela, o logo vira cabecalho.
  if (arte) {
    GfxRect r = { x + PAD, y, w - 2.0f * PAD, ARTE_H };
    if (desenhar) {
      GLuint t = tex_obter_larg(arte, r.w);
      float raio = 22.0f / r.h;
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(arte);
        gfx_card_forcar_cover_atual = 1.0f;   // preenche a faixa (sem barras laterais)
        gfx_rect(r, t, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
        gfx_card_forcar_cover_atual = 0.0f;
        gfx_tex_aspect_atual = 0.0f;
      } else gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
      gfx_veu_base(r, raio, 0.62f, 0.78f * a);
      logotitulo_desenhar(ci, ci->titulo, TXT_ILHA_NOME, r.x + INSET, r.y + r.h - 18.0f - LOGO_H,
                          LOGO_W, LOGO_H, r.w - 2.0f * INSET, ca);
    }
    y += ARTE_H + 18.0f;
  } else {
    if (desenhar) logotitulo_desenhar(ci, ci->titulo, TXT_ILHA_NOME, cx, y + 8.0f, LOGO_W, 52.0f, cw, ca);
    y += 8.0f + 52.0f + 12.0f;
  }

  // META + CLASSIFICACAO, e os generos embaixo.
  { float cl = m.classif[0] ? badge_largura(m.classif) + 12.0f : 0.0f;
    TxtLinha t = txt_linha_corta(TXT_ILHA_META, m.meta, 243, 242, 239, 255, cw - cl);
    float h = (float)t.h > BADGE_H ? (float)t.h : BADGE_H;
    if (m.meta[0] || m.classif[0]) {
      if (desenhar) {
        txt_desenhar_alpha(t, cx, y + (h - (float)t.h) * 0.5f, .78f * ca);
        if (m.classif[0])
          badge_desenhar(cx + (m.meta[0] ? (float)t.w + 12.0f : 0.0f), y + (h - BADGE_H) * 0.5f,
                         m.classif, BADGE_NEUTRO, ca);
      }
      y += h + 6.0f;
    } }
  if (m.genero[0]) {
    TxtLinha t = txt_linha_corta(TXT_ILHA_GENERO, m.genero, 243, 242, 239, 255, cw);
    if (desenhar) txt_desenhar_alpha(t, cx, y, .52f * ca);
    y += (float)t.h + 4.0f;
  }

  // NOTAS: a linha da pagina de titulo. Enquanto o resumo esta no ar e nao ha
  // nota nenhuma, tres pilulas de esqueleto marcam o lugar.
  if (m.temNotas) {
    NotasPlano p;
    y += 12.0f;
    notasui_planejar(&p, m.cru, cw, 0.0f, NULL);
    if (desenhar) notasui_desenhar_linha(&p, cx, y + NOTAS_H * 0.5f, ca);
    y += NOTAS_H;
  } else if (m.notasCarregando) {
    int i;
    y += 12.0f;
    if (desenhar)
      for (i = 0; i < 3; i++)
        gfx_cor((GfxRect){ cx + (float)i * 92.0f, y + (NOTAS_H - 24.0f) * 0.5f, 80.0f, 24.0f },
                0.5f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, .8f * ca);
    y += NOTAS_H;
  }

  // 1 LINHA P6 (farolinfo): somente quando ha dado medido; sem dado, nada.
  if (m.farolInfo[0]) {
    y += 8.0f;
    y += linhaIcone("aj_farol", m.farolInfo, cx, y, cw, ca, desenhar);
  }

  // SINOPSE, ate quatro linhas.
  if (m.sinopse && m.sinopse[0]) {
    int i;
    quebrarSinopse(m.sinopse, cw);
    y += 16.0f;
    for (i = 0; i < sinCache.n; i++) {
      if (desenhar) {
        TxtLinha t = txt_linha_corta(sinopse_estilo(TXT_ILHA_TEXTO), sinCache.l[i], 243, 242, 239, 255, cw);
        txt_desenhar_alpha(t, cx, y + (float)i * SIN_LEAD, .66f * ca);
      }
    }
    if (sinCache.n) y += (float)sinCache.n * SIN_LEAD -
                         (SIN_LEAD - sinopse_entrelinha(TXT_ILHA_TEXTO, 24.0f));
  }

  // ONDE PAROU, AMIGOS, AGENDA.
  if (m.prog[0] || m.amigos[0] || m.agenda[0]) y += 14.0f;
  if (m.prog[0]) {
    y += linhaIcone("aj_circle-play", m.prog, cx, y, cw, ca, desenhar);
    if (m.progFrac > 0.0f) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      if (desenhar) {
        gfx_cor((GfxRect){ cx + ICONE + 12.0f, y, cw - ICONE - 12.0f, 4.0f }, 0.5f, 1, 1, 1, .14f * ca);
        gfx_cor((GfxRect){ cx + ICONE + 12.0f, y, (cw - ICONE - 12.0f) * m.progFrac, 4.0f }, 0.5f,
                ar, ag, ab, ca);
      }
      y += 10.0f;
    }
  }
  if (m.amigos[0]) y += linhaIcone("aj_users", m.amigos, cx, y, cw, ca, desenhar);
  if (m.agenda[0]) y += linhaIcone("aj_calendar", m.agenda, cx, y, cw, ca, desenhar);

  // SELOS de estado: o que ja e verdade sobre o titulo nesta conta.
  if (m.salvo || m.visto) {
    float bx = cx;
    y += 14.0f;
    if (m.salvo) { if (desenhar) badge_desenhar(bx, y, i18n("Salvo"), BADGE_REALCE, ca);
                   bx += badge_largura(i18n("Salvo")) + BADGE_GAP; }
    if (m.visto && desenhar) badge_desenhar(bx, y, i18n("Assistido"), BADGE_REALCE, ca);
    y += BADGE_H;
  }
  y += PAD + 4.0f;
  return y - y0;
}

float ctxinfo_altura(const CatItem *ci, const CtxInfoEstado *st, float w) {
  return fazer(ci, st, 0.0f, 0.0f, w, 0.0f, 0.0f, 0);
}

float ctxinfo_desenhar(const CatItem *ci, const CtxInfoEstado *st,
                       float x, float y, float w, float a, float ca) {
  return fazer(ci, st, x, y, w, a, ca, 1);
}

void ctxinfo_texto(const CatItem *ci, const CtxInfoEstado *st, char *dst, size_t cap) {
  Modelo m;
  size_t n;
  int f;
  if (!dst || !cap) return;
  dst[0] = 0;
  if (!ci) return;
  montar(ci, st, &m);
  snprintf(dst, cap, "meta: %s | %s | %s\nnotes:", m.meta, m.genero, m.classif);
  for (f = 0; f < EX_NFONTES; f++)
    if (m.cru[f] > 0) { n = strlen(dst); snprintf(dst + n, cap - n, " %d=%d", f, m.cru[f]); }
  n = strlen(dst);
  snprintf(dst + n, cap - n, "%s\nsynopsis: %.60s\nprogress: %s\nfriends: %s\nschedule: %s\nbadges:%s%s\n",
           m.notasCarregando ? " (loading)" : "", m.sinopse ? m.sinopse : "", m.prog,
           m.amigos, m.agenda, m.salvo ? " saved" : "", m.visto ? " watched" : "");
  if (m.farolInfo[0]) {
    n = strlen(dst);
    snprintf(dst + n, cap - n, "info: %s\n", m.farolInfo);
  }
}

// A VERSAO COMPACTA, para dentro da linha expandida do painel de Salvos: meta +
// classificacao, notas e sinopse em duas linhas. Sem arte (a faixa e de quem
// chama). Devolve a altura. `gap` entre os blocos, `sinN` linhas de sinopse,
// `sombra` 1 = sombra suave sob o texto (sobre foto).
static float compactoN(const CatItem *ci, const CtxInfoEstado *st, float x, float y,
                       float w, float ca, int desenhar, float gap, int sinN, int sombra) {
  Modelo m;
  float y0 = y;
  if (!ci) return 0.0f;
  montar(ci, st, &m);
  { float cl = m.classif[0] ? badge_largura(m.classif) + 12.0f : 0.0f;
    TxtLinha t = txt_linha_corta(TXT_ILHA_META, m.meta, 243, 242, 239, 255, w - cl);
    float h = (float)t.h > BADGE_H ? (float)t.h : BADGE_H;
    if (m.meta[0] || m.classif[0]) {
      if (desenhar) {
        if (sombra) { TxtLinha d = txt_linha_corta(TXT_ILHA_META, m.meta, 0, 0, 0, 255, w - cl);
                      txt_desenhar_alpha(d, x + 1.5f, y + (h - (float)t.h) * 0.5f + 1.5f, .7f * ca); }
        txt_desenhar_alpha(t, x, y + (h - (float)t.h) * 0.5f, (sombra ? .92f : .78f) * ca);
        if (m.classif[0])
          badge_desenhar(x + (m.meta[0] ? (float)t.w + 12.0f : 0.0f), y + (h - BADGE_H) * 0.5f,
                         m.classif, BADGE_NEUTRO, ca);
      }
      y += h + gap;
    } }
  if (m.temNotas) {
    NotasPlano p;
    notasui_planejar(&p, m.cru, w, 0.0f, NULL);
    if (desenhar) notasui_desenhar_linha(&p, x, y + NOTAS_H * 0.5f, ca);
    y += NOTAS_H + gap;
  }
  if (m.sinopse && m.sinopse[0]) {
    int i;
    sinMax = sinN; quebrarSinopse(m.sinopse, w); sinMax = CTXI_SIN_LINHAS;
    for (i = 0; i < sinCache.n; i++) {
      if (desenhar) {
        TxtLinha t = txt_linha_corta(sinopse_estilo(TXT_ILHA_TEXTO), sinCache.l[i], 243, 242, 239, 255, w);
        if (sombra) { TxtLinha d = txt_linha_corta(sinopse_estilo(TXT_ILHA_TEXTO), sinCache.l[i], 0, 0, 0, 255, w);
                      txt_desenhar_alpha(d, x + 1.5f, y + (float)i * SIN_LEAD + 1.5f, .7f * ca); }
        txt_desenhar_alpha(t, x, y + (float)i * SIN_LEAD, (sombra ? .9f : .66f) * ca);
      }
    }
    y += (float)sinCache.n * SIN_LEAD;
  } else if (y > y0) y -= gap;
  return y - y0;
}
float ctxinfo_compacto(const CatItem *ci, const CtxInfoEstado *st, float x, float y,
                       float w, float ca, int desenhar) {
  return compactoN(ci, st, x, y, w, ca, desenhar, 6.0f, 2, 0);
}

// SOBRE A ARTE (cartoes grandes, paisagem): a informacao e uma camada em cima da
// propria foto, de cima para baixo e sem sobreposicao: logo (altura limitada),
// meta com a classificacao, notas, sinopse em 2 linhas (1 se nao couber).
// Escurecimento forte (degrade ate ~.85 nos 65% de baixo + 20% sobre a foto).
#define SOBRE_PAD  24.0f
#define SOBRE_LOGO 56.0f
#define SOBRE_GAP  12.0f
float ctxinfo_sobre_altura(const CatItem *ci, const CtxInfoEstado *st, float w) {
  return SOBRE_PAD + SOBRE_LOGO + SOBRE_GAP +
         compactoN(ci, st, 0, 0, w - 2.0f * SOBRE_PAD, 0, 0, SOBRE_GAP, 2, 1) + SOBRE_PAD;
}
void ctxinfo_sobre_arte(const CatItem *ci, const CtxInfoEstado *st, GfxRect r, float raioPx, float ca) {
  float cw = r.w - 2.0f * SOBRE_PAD, fixo = SOBRE_PAD + SOBRE_LOGO + SOBRE_GAP + SOBRE_PAD, y0;
  int sinN = 2;
  if (ca < 0.01f) return;
  if (fixo + compactoN(ci, st, 0, 0, cw, 0, 0, SOBRE_GAP, 2, 1) > r.h) sinN = 1;
  gfx_cor(r, raioPx / r.h, 0, 0, 0, 0.20f * ca);
  gfx_veu_base(r, raioPx / r.h, 0.65f, 0.85f * ca);
  y0 = r.y + r.h - (fixo + compactoN(ci, st, 0, 0, cw, 0, 0, SOBRE_GAP, sinN, 1));
  if (y0 < r.y) y0 = r.y;
  logotitulo_desenhar(ci, ci->titulo, TXT_ILHA_NOME, r.x + SOBRE_PAD, y0 + SOBRE_PAD, LOGO_W, SOBRE_LOGO, cw, ca);
  compactoN(ci, st, r.x + SOBRE_PAD, y0 + SOBRE_PAD + SOBRE_LOGO + SOBRE_GAP, cw, ca, 1, SOBRE_GAP, sinN, 1);
}
