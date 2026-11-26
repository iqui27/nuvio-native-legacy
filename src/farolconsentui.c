// FAROLCONSENTUI — tela "Vai junto / Nunca vai" do gosto (issue 53).
// Zero _Thread_local; zero log com URL/token/titulo/perfil.
#include "farolconsentui.h"
#include "farolconsent.h"
#include "farolcompor.h"
#include "faroltexto.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "ponteiro.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FCUI_MARGEM   120.0f
#define FCUI_ABRIR_MS 320.0f
#define FCUI_FECHAR_MS 180.0f
#define FCUI_MAX_IT   4
#define FCUI_VT_Y     462.0f
#define FCUI_VT_W     740.0f
#define FCUI_VT_H     116.0f

// VAI TOCAR NESTA TV (4.2), unificado aqui: um consentimento so, na tela do
// Farol, em vez de um segundo cartao no arranque. Fraco porque o teste do
// Farol (tests/farol_consent.sh) liga esta tela sem ajustes.c: sem o ajuste,
// a linha some e a tela fica exatamente como era.
extern int  ajustes_vaitocar(void) __attribute__((weak));
extern void ajustes_definir_vaitocar(int ligado) __attribute__((weak));
#define FCUI_TEXTO_VT     "Compartilhar compatibilidade"
#define FCUI_TEXTO_VT_SUB "Conta, sem título nem endereço, se o vídeo tocou neste aparelho."

typedef struct {
  int     id;
  GfxRect r;
} FcuiItem;

static int   aberto;
static int   focoAtual = FCUI_B_SO_TV;
static float entrada;
static int   vtPendente;

static int vt_presente(void) { return ajustes_vaitocar && ajustes_definir_vaitocar; }

// A decisao da tela leva junto a escolha da linha de compatibilidade.
static void vt_aplicar(void) {
  if (vt_presente()) ajustes_definir_vaitocar(vtPendente);
}

static int foco_padrao(void) {
  return (farolconsent_estado() == FAROL_CONSENT_SIM) ? FCUI_B_PERMITIR : FCUI_B_SO_TV;
}

static void poe(FcuiItem *it, int *n, int id, float ox, float oy,
                float x, float y, float w, float h) {
  if (*n >= FCUI_MAX_IT) return;
  it[*n].id = id;
  it[*n].r = (GfxRect){ ox + x, oy + y, w, h };
  (*n)++;
}

static int montar(FcuiItem *it, float ox, float oy) {
  int n = 0;
  float x = FCUI_MARGEM;
  const char *rotSoTv = i18n(FT_GOSTO_LLM_SO_ESTA_TV);
  const char *rotPerm = i18n(FT_GOSTO_CONSENT_PERMITIR);
  float wSoTv = botao_largura(rotSoTv, NULL, 1);
  float wPerm = botao_largura(rotPerm, "aj_check", 0);
  if (vt_presente()) poe(it, &n, FCUI_B_VAITOCAR, ox, oy, x, FCUI_VT_Y, FCUI_VT_W, FCUI_VT_H);
  poe(it, &n, FCUI_B_SO_TV, ox, oy, x, 610.0f, wSoTv, BOTAO_H_PRIMARIO);
  poe(it, &n, FCUI_B_PERMITIR, ox, oy, x + wSoTv + BOTAO_GAP, 618.0f, wPerm, BOTAO_H_SECUNDARIO);
  return n;
}

static void mover(int dx, int dy) {
  FcuiItem it[FCUI_MAX_IT];
  int n = montar(it, 0.0f, 0.0f), i, cur = -1, melhor = -1;
  float cx, cy, bs = 1e9f;
  for (i = 0; i < n; i++) if (it[i].id == focoAtual) cur = i;
  if (cur < 0) { focoAtual = FCUI_B_SO_TV; return; }
  cx = it[cur].r.x + it[cur].r.w * 0.5f;
  cy = it[cur].r.y + it[cur].r.h * 0.5f;
  for (i = 0; i < n; i++) {
    float ax, ay, along, cross, s;
    if (i == cur) continue;
    ax = it[i].r.x + it[i].r.w * 0.5f - cx;
    ay = it[i].r.y + it[i].r.h * 0.5f - cy;
    along = ax * (float)dx + ay * (float)dy;
    cross = dx ? fabsf(ay) : fabsf(ax);
    if (along < 1.0f) continue;
    s = along + 3.0f * cross;
    if (s < bs) { bs = s; melhor = i; }
  }
  if (melhor >= 0) focoAtual = it[melhor].id;
}

static void acionar_ok(void) {
  if (focoAtual == FCUI_B_VAITOCAR) {
    vtPendente = !vtPendente;
  } else if (focoAtual == FCUI_B_SO_TV) {
    vt_aplicar();
    (void)farolconsent_definir(FAROL_CONSENT_SO_TV);
    farolcompor_definir_consentimento(0);
    farolconsentui_fechar();
  } else if (focoAtual == FCUI_B_PERMITIR) {
    vt_aplicar();
    (void)farolconsent_definir(FAROL_CONSENT_SIM);
    farolconsentui_fechar();
  }
}

int farolconsentui_presente(void) { return 1; }
int farolconsentui_aberto(void) { return aberto; }
int farolconsentui_foco(void) { return focoAtual; }
int farolconsentui_vaitocar_pendente(void) { return vtPendente; }

void farolconsentui_abrir(void) {
  aberto = 1;
  focoAtual = foco_padrao();
  vtPendente = vt_presente() ? (ajustes_vaitocar() != 0) : 0;
  entrada = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
}

void farolconsentui_fechar(void) {
  aberto = 0;
}

void farolconsentui_esquecer(void) {
  aberto = 0;
  focoAtual = FCUI_B_SO_TV;
  vtPendente = 0;
  entrada = 0.0f;
}

void farolconsentui_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { mover(-1, 0); return; }
  if (k == SDLK_RIGHT) { mover(1, 0); return; }
  if (k == SDLK_UP)    { mover(0, -1); return; }
  if (k == SDLK_DOWN)  { mover(0, 1); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (e->key.repeat != 0) return;
    acionar_ok();
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    farolconsentui_fechar();
  }
}

void farolconsentui_atualizar(float dt) {
  if (ajustes_animacoes_reduzidas()) {
    entrada = aberto ? 1.0f : 0.0f;
    return;
  }
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? FCUI_ABRIR_MS : FCUI_FECHAR_MS);
}

static void txt_em(TxtEstilo est, const char *s, float x, float y, float maxW,
                   int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void pt_foco(int id, int nada) { (void)nada; focoAtual = id; }

static void alvo(GfxRect r, int id) {
  ponteiro_alvo(r.x, r.y, r.w, r.h, pt_foco, NULL, id, 0);
}

static void painel(GfxRect r, float raioPx, float a) {
  if (ajustes_vidro()) gfx_vidro_painel(r, raioPx / r.h, 0.78f, a);
  else {
    gfx_cor(r, raioPx / r.h, 0.12f, 0.125f, 0.145f, 0.9f * a);
    gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.08f * a);
  }
}

static void desenha_espelho(float ox, float oy, float a) {
  size_t nVai = 0, nNunca = 0, i;
  const char *const *vai = farolconsent_vai_junto(&nVai);
  const char *const *nunca = farolconsent_nunca_vai(&nNunca);
  float px = ox + 900.0f, py = oy + 290.0f, pw = 900.0f, ph = 668.0f, colw = 396.0f;
  float ar, ag, ab;

  ajustes_acento(&ar, &ag, &ab);
  painel((GfxRect){ px, py, pw, ph }, 32.0f, a);
  txt_em(TXT_CAPTION, i18n(FT_VAI_JUNTO), px + 36.0f, py + 28.0f, colw, 150, 156, 168, a);
  txt_em(TXT_CAPTION, i18n(FT_NUNCA_VAI), px + 36.0f + colw + 36.0f, py + 28.0f, colw, 150, 156, 168, a);

  for (i = 0; i < nVai; i++) {
    float ry = py + 74.0f + (float)i * 74.0f;
    gfx_icone((GfxRect){ px + 36.0f, ry + 3.0f, 22.0f, 22.0f }, "aj_check", ar, ag, ab, a);
    txt_bloco(TXT_CAPTION, i18n(vai[i]), 225, 228, 236, px + 36.0f + 34.0f, ry, colw - 34.0f, 26.0f, a, 2);
  }
  for (i = 0; i < nNunca; i++) {
    float ry = py + 74.0f + (float)i * 74.0f;
    gfx_icone((GfxRect){ px + 36.0f + colw + 36.0f, ry + 3.0f, 22.0f, 22.0f }, "aj_x", 0.6f, 0.62f, 0.66f, a);
    txt_bloco(TXT_CAPTION, i18n(nunca[i]), 170, 176, 188, px + 36.0f + colw + 36.0f + 34.0f, ry, colw - 34.0f, 26.0f, a, 2);
  }
  txt_em(TXT_CAPTION, i18n(FT_GOSTO_CONSENT_RODAPE), px + 36.0f, py + ph - 44.0f, pw - 72.0f, 150, 156, 168, a);
}

static void desenha_botao(FcuiItem *it, const char *rot, const char *ic, int prim, float a) {
  int f = focoAtual == it->id;
  botao_pilula(it->r, rot, ic, f ? 1.0f : 0.0f, prim, 0, a);
  if (prim && !f && !ajustes_vidro()) {
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_anel(it->r, 0.5f, 2.0f, ar, ag, ab, a);
  }
  alvo(it->r, it->id);
}

// A linha de compatibilidade: superficie de botao (mesmo foco de todo o app),
// caixa de marcar, o nome do ajuste e o que vai, sem titulo nem endereco.
static void desenha_vaitocar(FcuiItem *it, float a) {
  float f = focoAtual == it->id ? 1.0f : 0.0f, ar, ag, ab;
  GfxRect cx = { it->r.x + 28.0f, it->r.y + (it->r.h - 36.0f) * 0.5f, 36.0f, 36.0f };
  // Em repouso, o painel neutro da tela (nao o primario rosado, que parece
  // foco); em foco, a superficie de botao de todo o app.
  int tx = 235;
  if (f > 0.5f) tx = botao_superficie(it->r, f, a);
  else painel(it->r, it->r.h * 0.5f, a);
  float tx0 = cx.x + cx.w + 24.0f, tw = it->r.w - (tx0 - it->r.x) - 28.0f;
  ajustes_acento(&ar, &ag, &ab);
  if (vtPendente) {
    gfx_cor(cx, 0.28f, ar, ag, ab, a);
    gfx_icone((GfxRect){ cx.x + 6.0f, cx.y + 6.0f, 24.0f, 24.0f }, "aj_check", 1, 1, 1, a);
  } else {
    gfx_anel(cx, 0.28f, 2.0f, tx / 255.0f, tx / 255.0f, tx / 255.0f, 0.7f * a);
  }
  txt_em(TXT_V2_26, i18n(FCUI_TEXTO_VT), tx0, it->r.y + 14.0f, tw, tx, tx, tx, a);
  txt_bloco(TXT_CAPTION, i18n(FCUI_TEXTO_VT_SUB), tx, tx, tx, tx0, it->r.y + 50.0f, tw, 26.0f, 0.72f * a, 2);
  alvo(it->r, it->id);
}

void farolconsentui_desenhar(float ox, float oy, float a) {
  FcuiItem it[FCUI_MAX_IT];
  char kick[160];
  float x = ox + FCUI_MARGEM;
  float ar, ag, ab, effA;
  int n, i;

  if (!aberto && entrada < 0.002f) return;
  effA = a * (entrada > 0.0f ? anim_suave(entrada) : (aberto ? 1.0f : 0.0f));
  if (effA <= 0.001f) return;

  ajustes_acento(&ar, &ag, &ab);
  snprintf(kick, sizeof kick, "%s › %s", i18n(FT_NOME), i18n(FT_GOSTO_CONSENT_KICKER));
  txt_em(TXT_CAPTION, kick, x, oy + 96.0f, 1500.0f, 150, 156, 168, effA);
  // Ponto de luz de 10 px no acento antes do titulo.
  gfx_cor((GfxRect){ x, oy + 150.0f, 10.0f, 10.0f }, 0.5f, ar, ag, ab, effA);
  txt_em(TXT_NOV_TITULO, i18n(FT_GOSTO_CONSENT_TITULO), x + 24.0f, oy + 128.0f, 1656.0f, 248, 249, 252, effA);
  txt_bloco(TXT_V2_26, i18n(FT_GOSTO_CONSENT_SUB), 186, 192, 204, x, oy + 206.0f, 1500.0f, 34.0f, 0.9f * effA, 2);

  txt_bloco(TXT_V2_26, i18n(FT_GOSTO_CONSENT_NOTA_SO_TV), 186, 192, 204, x, oy + 320.0f, 740.0f, 34.0f, 0.95f * effA, 4);
  desenha_espelho(ox, oy, effA);

  n = montar(it, ox, oy);
  for (i = 0; i < n; i++) {
    if (it[i].id == FCUI_B_VAITOCAR) {
      desenha_vaitocar(&it[i], effA);
    } else if (it[i].id == FCUI_B_SO_TV) {
      desenha_botao(&it[i], i18n(FT_GOSTO_LLM_SO_ESTA_TV), NULL, 1, effA);
    } else if (it[i].id == FCUI_B_PERMITIR) {
      desenha_botao(&it[i], i18n(FT_GOSTO_CONSENT_PERMITIR), "aj_check", 0, effA);
    }
  }
}
