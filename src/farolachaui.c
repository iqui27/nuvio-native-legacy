// Tela 7 do Farol — "Achados" (PLANO.md §2.9, §3.2 tela 7, §3.3 "Nada a achar",
// issue #25). Ver farolachaui.h.
#include "farolachaui.h"
#include "faroltexto.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "ponteiro.h"
#include "posterprov.h"
#include "tex_cache.h"
#include "text.h"
#include <stdio.h>
#include <string.h>

#define ACHA_MARGEM    120.0f
#define ACHA_CARD_W    280.0f
#define ACHA_CARD_H    420.0f
#define ACHA_CARD_GAP   48.0f
#define ACHA_ROW_Y     326.0f
#define ACHA_VOLTAR_Y  888.0f
#define ACHA_FOCO_VOLTAR (-1)

typedef struct {
  int  tem;
  long tmdb;
  char imdb[64];
  char tipo[8];
  char titulo[160];
} AchaPedido;

static struct {
  FarolAchaTipo tipo;
  int           maxMin;
  int           limEfetivo;
  char          genero[32];
  char          kicker[96];
  char          l1Override[200];
  char          l2Override[200];
  unsigned      semente;
  unsigned      rev;
  FarolAchado   achados[FAROL_ACHA_UI_MAX];
  int           nAchados;
  int           fallbackAtivo;
  int           focoIdx;
  int           ultimoPoster;
  float         animCard[FAROL_ACHA_UI_MAX];
  float         animVoltar;
  AchaPedido    pedido;
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
  } else if (S.fallbackAtivo && S.tipo == FA_CURTO && n == 3 && todosCurtos) {
    snprintf(linha2, sizeof linha2, "%s", i18n(FT_NADA_ACHAR_L2));
  } else if (todosNaoVi) {
    snprintf(linha2, sizeof linha2, "%s", i18n(FT_ACHA_L2_NAO_VISTO));
  }
}

static void recarregar(void) {
  int lim = S.maxMin > 0 ? S.maxMin : FAROL_ACHA_CURTO_PADRAO;
  int n = 0;
  unsigned r = 0;

  S.fallbackAtivo = 0;
  S.limEfetivo = lim;
  memset(S.achados, 0, sizeof S.achados);

  if (S.genero[0]) {
    n = farolacha_listar_genero(S.tipo, lim, S.genero, S.achados,
                                FAROL_ACHA_UI_MAX, S.semente, &r);
    if (n == 0) {
      n = farolacha_listar_rev(S.tipo, lim, S.achados,
                               FAROL_ACHA_UI_MAX, S.semente, &r);
      if (n > 0) S.fallbackAtivo = 1;
    }
  } else {
    n = farolacha_listar_rev(S.tipo, lim, S.achados,
                             FAROL_ACHA_UI_MAX, S.semente, &r);
  }

  if (n == 0 && S.tipo == FA_CURTO) {
    int limFb = lim < 130 ? 130 : lim + 30;
    n = farolacha_listar_rev(FA_CURTO, limFb, S.achados,
                             FAROL_ACHA_UI_MAX, S.semente, &r);
    if (n > 0) {
      S.fallbackAtivo = 1;
      S.limEfetivo = limFb;
    }
  }

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
}

static void garantirRevisao(void) {
  int i;
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
  const CatItem *c;
  garantirRevisao();
  if (S.focoIdx < 0 || S.focoIdx >= S.nAchados) return 0;
  c = farolacha_item(&S.achados[S.focoIdx], S.rev);
  if (!c) return 0;

  memset(&S.pedido, 0, sizeof S.pedido);
  S.pedido.tem = 1;
  S.pedido.tmdb = c->tmdb;
  if (c->imdb[0])
    snprintf(S.pedido.imdb, sizeof S.pedido.imdb, "%s", c->imdb);
  else if (c->tmdb > 0)
    snprintf(S.pedido.imdb, sizeof S.pedido.imdb, "tmdb:%ld", c->tmdb);
  snprintf(S.pedido.tipo, sizeof S.pedido.tipo, "%s", c->tipo);
  snprintf(S.pedido.titulo, sizeof S.pedido.titulo, "%s", c->titulo);
  return 1;
}

int farolachaui_evento(const SDL_Event *e, int *pediuVoltar, int *pediuAbrir) {
  SDL_Keycode k;
  if (pediuVoltar) *pediuVoltar = 0;
  if (pediuAbrir) *pediuAbrir = 0;
  if (!e || e->type != SDL_KEYDOWN) return 0;

  k = e->key.keysym.sym;
  if (k == SDLK_LEFT) {
    if (S.nAchados > 0 && S.focoIdx > 0) S.focoIdx--;
    return 1;
  }
  if (k == SDLK_RIGHT) {
    if (S.nAchados > 0 && S.focoIdx >= 0 && S.focoIdx + 1 < S.nAchados) S.focoIdx++;
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
    if (S.focoIdx < 0 && S.nAchados > 0) {
      S.focoIdx = (S.ultimoPoster >= 0 && S.ultimoPoster < S.nAchados)
                ? S.ultimoPoster : 0;
    }
    return 1;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (S.focoIdx < 0) {
      if (pediuVoltar) *pediuVoltar = 1;
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
    float alvoV = (S.focoIdx == ACHA_FOCO_VOLTAR) ? 1.0f : 0.0f;
    if (red) S.animVoltar = alvoV;
    else S.animVoltar = anim_mola(S.animVoltar, alvoV, dt,
                                  alvoV > S.animVoltar ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
}

static void pt_foco_acha(int id, int nada) {
  (void)nada;
  if (id == ACHA_FOCO_VOLTAR) {
    S.focoIdx = ACHA_FOCO_VOLTAR;
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

  ponteiro_alvo(x, y, ACHA_CARD_W, ACHA_CARD_H + 84.0f, pt_foco_acha, NULL, i, 0);

  if (f > 0.01f)
    botao_luz(p, f, a);

  arte = posterprov_card_addon(ci->origem, ci->imdb, ci->tmdb, ci->tipo, ci->poster);
  if (!arte || !arte[0]) arte = ci->backdrop[0] ? ci->backdrop : NULL;
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
              ehSerieTipo(ci->tipo) ? "aj_tv-minimal-play" : "aj_film",
              0.72f, 0.74f, 0.78f, 0.45f * a);
  }

  if (f > 0.01f) {
    float fr, fg, fb;
    botao_cor_foco(&fr, &fg, &fb);
    gfx_anel(p, raio, 3.0f, fr, fg, fb, f * a);
  } else {
    gfx_anel(p, raio, 1.0f, 1, 1, 1, 0.08f * a);
  }

  txt_em(TXT_ILHA_NOME, ci->titulo[0] ? ci->titulo : ci->imdb,
         x, p.y + p.h + 16.0f, ACHA_CARD_W,
         S.focoIdx == i ? 248 : 220, S.focoIdx == i ? 249 : 224, S.focoIdx == i ? 252 : 232, a);

  sub[0] = '\0';
  if (S.achados[i].minutos > 0) {
    formatarDuracaoCurta(S.achados[i].minutos, sub, sizeof sub);
  } else if (ci->meta[0]) {
    snprintf(sub, sizeof sub, "%s", ci->meta);
  }
  if (sub[0]) {
    txt_em(TXT_CAPTION, sub, x, p.y + p.h + 50.0f, ACHA_CARD_W, 166, 172, 184, a);
  }
}

void farolachaui_desenhar(float ox, float oy, float alpha, Uint32 agora) {
  char kick[160];
  int i;
  float vx = ox + ACHA_MARGEM;
  float vy = oy + ACHA_VOLTAR_Y;
  const char *rotV = i18n(FT_ACAO_VOLTAR);
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
  txt_em(TXT_NOV_TITULO, i18n(farolacha_rotulo(S.tipo)), ox + ACHA_MARGEM, oy + 128.0f,
         1680.0f, 248, 249, 252, alpha);
  if (linha1[0])
    txt_em(TXT_V2_26, linha1, ox + ACHA_MARGEM, oy + 206.0f, 1500.0f, 232, 235, 242, alpha);
  if (linha2[0])
    txt_em(TXT_V2_26, linha2, ox + ACHA_MARGEM, oy + 244.0f, 1500.0f, 186, 192, 204, alpha);

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
      const CatItem *ci = farolacha_item(&S.achados[i], S.rev);
      float cx;
      if (!ci) break;
      cx = ox + ACHA_MARGEM + (float)i * (ACHA_CARD_W + ACHA_CARD_GAP);
      desenha_card(i, ci, cx, oy + ACHA_ROW_Y, alpha);
    }
  }

  botao_pilula(rVoltar, rotV, "aj_arrow-left", S.animVoltar, S.nAchados == 0 ? 1 : 0, 0, alpha);
  ponteiro_alvo(rVoltar.x, rVoltar.y, rVoltar.w, rVoltar.h, pt_foco_acha, NULL, ACHA_FOCO_VOLTAR, 0);
}

static int resolvePedido(const AchaPedido *p) {
  int tentativa;
  char basePed[64];
  int temBase;

  if (!p || (!p->imdb[0] && p->tmdb <= 0 && !p->titulo[0])) return -1;
  idBaseDe(p->imdb, basePed, sizeof basePed);
  temBase = basePed[0] != '\0';

  for (tentativa = 0; tentativa < 3; tentativa++) {
    unsigned r = cat_revisao();
    int nc = cat_n();
    int i, found = -1;

    for (i = 0; i < nc; i++) {
      const CatItem *c = cat_item(i);
      char baseCat[64];
      if (!c) continue;
      if (p->tipo[0] && c->tipo[0] && ehSerieTipo(p->tipo) != ehSerieTipo(c->tipo))
        continue;
      if (p->imdb[0] && !strcmp(c->imdb, p->imdb)) {
        found = i;
        break;
      }
      if (temBase && found < 0) {
        idBaseDe(c->imdb, baseCat, sizeof baseCat);
        if (baseCat[0] && !strcmp(baseCat, basePed)) found = i;
      }
      if (found < 0 && p->tmdb > 0 && c->tmdb == p->tmdb)
        found = i;
    }

    if (found < 0 && !p->imdb[0] && p->tmdb <= 0 && p->titulo[0]) {
      for (i = 0; i < nc; i++) {
        const CatItem *c = cat_item(i);
        if (!c) continue;
        if (p->tipo[0] && c->tipo[0] && ehSerieTipo(p->tipo) != ehSerieTipo(c->tipo))
          continue;
        if (!strcmp(c->titulo, p->titulo)) {
          found = i;
          break;
        }
      }
    }

    if (cat_revisao() == r) return found;
  }
  return -1;
}

int farolachaui_pediu_abrir(int *indiceCatalogo) {
  AchaPedido p;
  int idx;
  if (!S.pedido.tem) return 0;
  p = S.pedido;
  memset(&S.pedido, 0, sizeof S.pedido);
  idx = resolvePedido(&p);
  if (idx < 0) return 0;
  if (indiceCatalogo) *indiceCatalogo = idx;
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
