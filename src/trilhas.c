// TRILHAS (2.1): a pagina da jornada. Ver trilhas.h. Sem rede: o catalogo
// versionado mora em trilhas_catalogo.inc e o progresso e o historico.
#include "trilhas.h"
#include "trilhasdados.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "catalogo.h"
#include "conquistasdados.h"
#include "idioma.h"
#include "ponteiro.h"
#include "text.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define Q_W      1920.0f
#define Q_H      1080.0f
#define MG        148.0f
#define BT_H       56.0f
#define CARD_W    216.0f
#define CARD_H    306.0f
#define CARD_GAP   24.0f
#define PAINEL_Y  610.0f

enum { B_VOLTAR = 1, B_BADGE, B_ASSISTIR, P_PASSO0 = 100 };

static int aberto, foco, passo, quer;
static float entrada, ox, oy;
static const CqTrilha *tr;
static unsigned char visto[CQ_TRILHA_MAX];
static int npassos, nvistos;

int  trilhas_aberta(void) { return aberto; }
const char *trilhas_titulo(void) { return aberto && tr ? tr->nome : ""; }
int  trilhas_passo(void) { return aberto && passo >= 0 ? passo : -1; }
int  trilhas_passos(void) { return npassos; }
int  trilhas_vistos(void) { return nvistos; }
int  trilhas_total(void) { return tr ? tr->n : 0; }

int trilhas_pediu_conquistas(void) { int q = quer; quer = 0; return q; }
void trilhas_teste_esquecer(void) { aberto = 0; entrada = 0.0f; foco = B_VOLTAR; quer = 0; tr = NULL; passo = -1; npassos = nvistos = 0; }

static void reler(void) {
  npassos = tr ? cqd_trilha_vistos(tr, visto, CQ_TRILHA_MAX) : 0;
  nvistos = 0;
  for (int i = 0; i < npassos; i++) nvistos += visto[i] != 0;
}

int trilhas_abrir(const char *chave) {
  tr = cqd_trilha_buscar(chave);
  if (!tr || tr->n <= 0) { aberto = 0; return 0; }
  reler();
  passo = cqd_trilha_proximo(tr, visto, npassos);
  aberto = 1; foco = passo >= 0 ? P_PASSO0 + passo : B_BADGE; quer = 0;
  if (ajustes_animacoes_reduzidas()) entrada = 1.0f;
  return 1;
}

int trilhas_abrir_titulo(const char *imdb, const char *tipo) {
  const CqTrilha *ts[4];
  int n = cqd_trilhas_de(imdb, tipo, ts, 4);
  if (!n) return 0;
  if (trilhas_abrir(ts[0]->id)) {
    for (int i = 0; i < npassos; i++) if (!strcmp(ts[0]->p[i].imdb, imdb)) { passo = i; foco = P_PASSO0 + i; break; }
    return 1;
  }
  return 0;
}

static void origem(void) { ox = (NV_TELA_W - Q_W) * 0.5f; oy = (NV_TELA_H - Q_H) * 0.5f; }
static GfxRect em(float x, float y, float w, float h) { return (GfxRect){ ox + x, oy + y, w, h }; }

static void texto(TxtEstilo est, const char *s, float x, float y, float maxW, float a,
                  int r, int g, int b) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void botao(GfxRect r, const char *s, const char *ic, int foc, float a) {
  float fr, fg, fb, t;
  TxtLinha l;
  float x = r.x + 24.0f;
  if (foc) {
    t = botao_cor_foco(&fr, &fg, &fb);
    botao_luz(r, 1.0f, a);
    gfx_cor(r, 0.5f, fr, fg, fb, a);
    l = txt_linha(TXT_V2_CHIP, s, (int)(t * 255.0f), (int)(t * 255.0f), (int)(t * 255.0f), 255);
    gfx_icone((GfxRect){ x, r.y + (r.h - 26.0f) * 0.5f, 26.0f, 26.0f }, ic,
              (float)(t * 255.0f) / 255.0f, (float)(t * 255.0f) / 255.0f, (float)(t * 255.0f) / 255.0f, a);
  } else {
    gfx_cor(r, 0.5f, 1, 1, 1, 0.08f * a);
    l = txt_linha(TXT_V2_CHIP, s, 243, 242, 239, 255);
    gfx_icone((GfxRect){ x, r.y + (r.h - 26.0f) * 0.5f, 26.0f, 26.0f }, ic, 1, 1, 1, 0.88f * a);
  }
  txt_desenhar_alpha(l, x + 26.0f + 12.0f, r.y + (r.h - (float)l.h) * 0.5f, a);
  if (aberto) ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, NULL, 0, 0);
}

static void voltar(void) { aberto = 0; }

static void ok(void) {
  // O passo segue o historico do perfil: ja visto conta antes de abrir a
  // trilha, e marcar aqui nao inventa progresso — grava no mesmo historico.
  if (foco >= P_PASSO0 && foco - P_PASSO0 < npassos && tr) {
    cat_historico_definir_id(tr->p[foco - P_PASSO0].imdb, "movie", 1);
    cqd_atualizar_local();
    reler();
    if (foco == P_PASSO0 + passo) passo = cqd_trilha_proximo(tr, visto, npassos);
  } else if (foco == B_ASSISTIR && passo >= 0) {
    cat_historico_definir_id(tr->p[passo].imdb, "movie", 1);
    cqd_atualizar_local();
    reler();
    passo = cqd_trilha_proximo(tr, visto, npassos);
    if (passo >= 0) foco = P_PASSO0 + passo;
    else foco = B_BADGE;
  } else if (foco == B_BADGE) {
    aberto = 0;
    quer = 1;
    return;
  }
  else if (foco == B_VOLTAR) voltar();
}

void trilhas_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP)) return;
  k = e->key.keysym.sym;
  if (e->type == SDL_KEYUP) return;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { voltar(); return; }
  if (foco >= P_PASSO0) {
    int i = foco - P_PASSO0;
    if (k == SDLK_LEFT && i > 0) foco = P_PASSO0 + i - 1;
    else if (k == SDLK_RIGHT && i + 1 < npassos) foco = P_PASSO0 + i + 1;
    else if (k == SDLK_DOWN) foco = passo >= 0 && i != passo ? B_ASSISTIR : B_BADGE;
    else if (k == SDLK_UP) foco = B_VOLTAR;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) ok();
    return;
  }
  if (k == SDLK_LEFT || k == SDLK_UP) { foco = P_PASSO0 + (passo >= 0 ? passo : npassos - 1); return; }
  if (k == SDLK_DOWN) { if (foco == B_VOLTAR && passo >= 0) foco = P_PASSO0 + passo; return; }
  if (k == SDLK_RIGHT) {
    if (foco == B_VOLTAR) foco = passo >= 0 ? B_ASSISTIR : B_BADGE;
    else if (foco == B_ASSISTIR) foco = B_BADGE;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) ok();
}

static void cartao(int i, float a) {
  const CqPasso *p = &tr->p[i];
  GfxRect r = em(MG + (float)i * (CARD_W + CARD_GAP), 262.0f, CARD_W, CARD_H);
  int foc = foco == P_PASSO0 + i, v = visto[i];
  char b[128];
  TxtLinha l;
  if (foc) { botao_luz(r, 1.0f, a); gfx_cor(r, 10.0f / r.h, 1, 1, 1, 0.13f * a); }
  else gfx_cor(r, 10.0f / r.h, 1, 1, 1, (v ? 0.10f : 0.055f) * a);
  {
    GfxRect d = { r.x + (CARD_W - 56.0f) * 0.5f, r.y + 26.0f, 56.0f, 56.0f };
    gfx_cor(d, 0.5f, 1, 1, 1, (v ? 0.95f : foc ? 0.22f : 0.10f) * a);
    if (v) gfx_icone((GfxRect){ d.x + 14.0f, d.y + 14.0f, 28.0f, 28.0f }, "aj_check", 0.06f, 0.10f, 0.10f, a);
    else {
      char nb[8];
      snprintf(nb, sizeof nb, "%d", i + 1);
      l = txt_linha(TXT_ILHA_NOME, nb, foc ? 243 : 160, foc ? 242 : 164, foc ? 239 : 174, 255);
      txt_desenhar_alpha(l, d.x + (56.0f - (float)l.w) * 0.5f, d.y + (56.0f - (float)l.h) * 0.5f, a);
    }
  }
  texto(TXT_ILHA_NOME, p->titulo, r.x + 16.0f, r.y + 106.0f, CARD_W - 32.0f, a,
        foc ? 243 : v ? 226 : 196, foc ? 242 : v ? 228 : 201, foc ? 239 : v ? 230 : 212);
  if (p->ano > 0) snprintf(b, sizeof b, "%d", p->ano);
  else b[0] = 0;
  texto(TXT_V2_18, b, r.x + 16.0f, r.y + 140.0f, CARD_W - 32.0f, a * 0.62f, 150, 156, 168);
  if (foc && passo == i) texto(TXT_V2_KICK, i18n("CONTINUAR"), r.x + 16.0f, r.y + r.h - 48.0f,
                               CARD_W - 32.0f, a, 243, 242, 239);
}

static void desenhar_painel(float a) {
  GfxRect p = em(MG, PAINEL_Y, 1624.0f, 330.0f);
  const CqPasso *pass;
  char b[160];
  int i = passo >= 0 ? passo : npassos - 1;
  if (!tr || npassos <= 0) return;
  pass = &tr->p[i];
  gfx_cor(p, 30.0f / p.h, 0.14f, 0.15f, 0.17f, 0.92f * a);
  if (passo >= 0)
    snprintf(b, sizeof b, i18n("Passo %d · %s · %d"), passo + 1, pass->titulo, pass->ano);
  else
    snprintf(b, sizeof b, i18n("Trilha completa · %d de %d passos"), nvistos, npassos);
  texto(TXT_V2_KICK, b, p.x + 32.0f, p.y + 28.0f, p.w - 64.0f, a, 150, 156, 168);
  texto(TXT_ILHA_TITULO, tr->nome, p.x + 32.0f, p.y + 64.0f, p.w - 64.0f, a, 243, 242, 239);
  snprintf(b, sizeof b, i18n("%d de %d filmes vistos"), nvistos, npassos);
  texto(TXT_V2_24, b, p.x + 32.0f, p.y + 122.0f, p.w - 64.0f, a, 186, 192, 204);
  if (passo >= 0)
    texto(TXT_V2_24, i18n("OK marca como visto e o passo conta; o progresso sai do seu histórico."),
          p.x + 32.0f, p.y + 160.0f, p.w - 64.0f, a * 0.8f, 150, 156, 168);
  else
    texto(TXT_V2_24, i18n("Tudo visto, em qualquer ordem. O emblema fica no Perfil."),
          p.x + 32.0f, p.y + 160.0f, p.w - 64.0f, a, 186, 192, 204);
  if (passo >= 0)
    botao(em(MG + 32.0f, PAINEL_Y + 232.0f, 390.0f, BT_H), i18n("Marcar como visto"), "aj_check",
          foco == B_ASSISTIR, a);
  botao(em(MG + p.w - 336.0f - 32.0f, PAINEL_Y + 232.0f, 336.0f, BT_H), i18n("Ver o emblema"),
        "aj_star", foco == B_BADGE, a);
}

void trilhas_desenhar(Uint32 agora) {
  float a;
  int i;
  char b[192];
  TxtLinha l;
  (void)agora;
  if (!aberto || !tr) return;
  if (entrada < 1.0f) entrada = anim_suave(entrada + 0.08f);
  a = anim_suave(entrada);
  ESCALA_SE_COUBER_INI(Q_W, Q_H);
  origem();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.051f, 0.051f, 0.051f, entrada);
  l = txt_linha(TXT_V2_CHIP, i18n("Voltar"), 243, 242, 239, 255);
  botao(em(MG, 36.0f, 24.0f * 2.0f + (float)l.w, BT_H), i18n("Voltar"), "aj_corner-up-left",
        foco == B_VOLTAR, a);
  texto(TXT_V2_KICK, i18n("Biblioteca › Trilhas"), MG + ox, 118.0f + oy, 900.0f, a, 150, 156, 168);
  texto(TXT_ILHA_TITULO, tr->nome, MG + ox, 142.0f + oy, 1250.0f, a, 243, 242, 239);
  snprintf(b, sizeof b, i18n("%d de %d passos · %s"), nvistos, npassos, tr->lema);
  texto(TXT_V2_24, b, MG + ox, 190.0f + oy, 1250.0f, a, 186, 192, 204);
  for (i = 0; i < npassos; i++) cartao(i, a);
  desenhar_painel(a);
  ESCALA_SE_COUBER_FIM();
}
