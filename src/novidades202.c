// Cartao de NOVIDADES DA 2.0.2. Ver novidades202.h.
//
// O MESMO PADRAO DA 2.0.1 (e da 1.4.8, aprovado pelo dono): um cartao, a
// previa viva a esquerda, as mudancas em grupos a direita, os botoes no canto
// de baixo e, no fim, "Apoie o projeto" com os QRs (apoio.h). A previa tem tres
// cenas montadas com pecas pequenas do proprio app: o menu de contexto (o
// cartaz vira o cartao de informacao), o novo destaque da Home e a ilha do
// relogio dizendo o que esta esperando.
//
// ALTURAS FIXAS: a lista nao mede texto; cada item tem uma linha de nome e uma
// de frase, apagada, e o rodape e reservado. CUSTO DE GPU: nenhuma passada de
// desfoque; artes do pacote, veus e retangulos pequenos.
#include "novidades202.h"
#include "apoio.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "dados.h"
#include "gfx.h"
#include "horafmt.h"
#include "idioma.h"
#include "layout.h"
#include "perfiltv.h"
#include "plrui.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include "text.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N_W         1720.0f
#define N_H          960.0f
#define N_X         ((NV_TELA_W - N_W) * 0.5f)
#define N_Y         ((NV_TELA_H - N_H) * 0.5f)
#define N_PAD         56.0f
#define N_RAIO        36.0f
#define PV_W         760.0f
#define PV_H         (N_H - 2.0f * N_PAD)
#define PV_RAIO       28.0f
#define COL_GAP       72.0f
#define TX           (N_X + N_PAD + PV_W + COL_GAP)
#define TW           (N_X + N_W - N_PAD - TX)
#define ICONE         44.0f
#define ABRIR_MS     320.0f
#define FECHAR_MS    180.0f
#define PAGINA_MS    420.0f
// A previa: 4,2 s por cena, 0,55 s de passagem.
#define CENAS          3
#define CICLO_S        4.2f
#define TROCA_S        0.55f

enum { P0_DEPOIS = 0, P0_CONTINUAR, P0_N };
enum { P1_VOLTAR = 0, P1_CONCLUIR, P1_N };

static int   aberto, decidido, pagina, foco = P0_CONTINUAR, pedido;
static float entrada, pag, relogio;
static char  dirArte[512] = "deploy/app/art";
static char  arte[CENAS][600];
// Folga entre o fim da lista e o topo do rodape no ultimo quadro desenhado,
// e quantas frases precisaram de reticencias (testes).
static float folgaLista = 999.0f;
static int   frasesCortadas;

// A arte de fundo de cada cena (do pacote, sem rede): a do menu, o hero da
// Home (a mesma do cartaz) e uma cena para a ilha.
static const int ARTE_N[CENAS] = { 12, 15, 18 };

void novidades202_dir(const char *d) {
  int i;
  if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d);
  for (i = 0; i < CENAS; i++)
    if (ARTE_N[i] >= 0) snprintf(arte[i], sizeof arte[i], "%s/%02d.jpg", dirArte, ARTE_N[i]);
    else arte[i][0] = 0;
}

static void pedirArtes(void) {
  int i;
  if (!arte[0][0]) novidades202_dir(NULL);
  for (i = 0; i < CENAS; i++) if (arte[i][0]) tex_obter_hero(arte[i]);
}

int novidades202_previa_pronta(void) {
  int i;
  for (i = 0; i < CENAS; i++) if (arte[i][0] && !tex_obter_hero(arte[i])) return 0;
  return 1;
}

int novidades202_aberto(void) { return aberto; }
int novidades202_pagina(void) { return pagina; }
int novidades202_foco(void) { return foco; }
void novidades202_teste_relogio(float s) { relogio = s; }
void novidades202_teste_esquecer(void) { decidido = 0; aberto = 0; }
float novidades202_teste_folga(void) { return folgaLista; }
int novidades202_teste_cortadas(void) { return frasesCortadas; }

int novidades202_pedido(void) {
  int p = pedido;
  pedido = N202_PEDIU_NADA;
  return p;
}

static void comecar(float e) {
  aberto = 1;
  decidido = 1;
  pagina = 0;
  pag = 0.0f;
  foco = P0_CONTINUAR;
  entrada = e;
  relogio = 0.0f;
  pedirArtes();
}

void novidades202_abrir(void) { comecar(0.0f); }

void novidades202_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N202_ARQ);
  if (s) { free(s); return; }
  // Sem a marca do guia da 2.0 o guia abre agora (novidades20_primeira_vez,
  // logo depois desta): ele ja conta o app inteiro, e um segundo cartao em
  // seguida seria demais. Fica visto.
  s = dados_ler("novidades-20-guia.txt");
  if (!s) { dados_gravar(N202_ARQ, "1\n"); return; }
  free(s);
  comecar(0.0f);
}

static void fechar(int oQue) {
  aberto = 0;
  pedido = oQue;
  dados_gravar(N202_ARQ, "1\n");
}

static void irPagina(int p) {
  pagina = p;
  foco = p ? P1_CONCLUIR : P0_CONTINUAR;
  if (ajustes_animacoes_reduzidas()) pag = (float)p;
}

static void ok(void) {
  if (!pagina) {
    if (foco == P0_DEPOIS) fechar(N202_PEDIU_NADA);
    else irPagina(1);
  } else {
    if (foco == P1_VOLTAR) irPagina(0);
    else fechar(N202_PEDIU_NADA);
  }
}

void novidades202_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int n;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  n = pagina ? P1_N : P0_N;
  if (k == SDLK_LEFT)  {
    if (foco > 0) foco--;
    return;
  }
  if (k == SDLK_RIGHT) {
    if (foco < n - 1) foco++;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { ok(); return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (pagina) irPagina(0);
    else fechar(N202_PEDIU_NADA);
  }
}

void novidades202_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (aberto) {
    pedirArtes();
    if (novidades202_previa_pronta()) relogio += dt;
  }
  if (red) { entrada = aberto ? 1.0f : 0.0f; pag = (float)pagina; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? ABRIR_MS : FECHAR_MS);
  pag = anim_rampa(pag, (float)pagina, dt, PAGINA_MS);
}

// ------------------------------------------------------------- pecas comuns
static float sai3(float t) { float u = 1.0f - anim_clamp(t, 0.0f, 1.0f); return 1.0f - u * u * u; }
static float janela(float t, float ini, float dur) { return anim_clamp((t - ini) / dur, 0.0f, 1.0f); }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// O vidro escuro das ilhas: miolo quase opaco e um fio claro na borda.
static void vidro(GfxRect r, float raioPx, float a) {
  gfx_cor(r, raioPx / r.h, 0.070f, 0.073f, 0.082f, 0.90f * a);
  gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.09f * a);
}

static void arteFundo(int cena, GfxRect p, float a) {
  GLuint t;
  if (a <= 0.004f || !arte[cena][0]) return;
  t = tex_obter_hero(arte[cena]);
  if (!t) return;
  gfx_tex_aspect_atual = tex_aspecto(arte[cena]);
  if (gfx_tex_aspect_atual <= 0.0f) gfx_tex_aspect_atual = 16.0f / 9.0f;
  // Cover sempre: a previa e mais alta que a arte, e o "contain" do GFX_CARD
  // deixaria faixas vazias em cima e embaixo.
  gfx_card_forcar_cover_atual = 1.0f;
  gfx_rect(p, t, GFX_CARD, 0, 0, 0, PV_RAIO / p.h, 1, 1, 1, a);
  gfx_card_forcar_cover_atual = 0.0f;
  gfx_tex_aspect_atual = 0.0f;
}

// ------------------------------------------------ cena 1: o menu de contexto
// O cartaz sobe do meio para a esquerda, vira o cartao de informacao (veu no
// pe, tipo e duas linhas apagadas) e o menu aparece ao lado, com o foco
// andando. So pecas pequenas; nenhuma passada de desfoque.
static void cenaMenu(GfxRect p, float t, float a) {
  int red = ajustes_animacoes_reduzidas();
  float m = red ? 1.0f : sai3(janela(t, 0.45f, 0.65f));
  float e = red ? 1.0f : sai3(janela(t, 0.05f, 0.40f));
  float pw = 240.0f, ph = 380.0f, ar, ag, ab;
  GfxRect c, mn;
  static const char *const ROT[3] = { "Salvar", "Marcar como assistido", "Tirar de Continuar assistindo" };
  static const char *const ICO[3] = { "aj_bookmark", "aj_eye", "aj_x" };
  int i, f = red ? 0 : t < 1.9f ? -1 : t < 2.9f ? 0 : 1;
  ajustes_acento(&ar, &ag, &ab);
  arteFundo(0, p, a);
  gfx_cor(p, PV_RAIO / p.h, 0.02f, 0.02f, 0.025f, 0.62f * a);
  c.w = pw; c.h = ph;
  c.x = lerp(p.x + (p.w - pw) * 0.5f, p.x + 40.0f, m);
  c.y = p.y + 40.0f + (1.0f - e) * 10.0f;
  gfx_rect((GfxRect){ c.x - 24.0f, c.y - 8.0f, c.w + 48.0f, c.h + 44.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.12f,
           0, 0, 0, 0.55f * e * a);
  gfx_cor(c, 26.0f / c.h, 0.07f, 0.073f, 0.082f, e * a);
  arteFundo(1, c, e * a);
  // O cartao de informacao sobre a propria arte.
  { GfxRect v = { c.x, c.y + c.h - 130.0f * m, c.w, 130.0f * m };
    if (v.h > 4.0f) {
      TxtLinha tp = txt_linha(TXT_ILHA_GENERO, i18n("Filme"), 243, 242, 239, 255);
      gfx_rect(v, 0, GFX_VEU_CARD, 0, 0, 0, 26.0f / v.h, 0.03f, 0.03f, 0.035f, m * a);
      txt_desenhar_alpha(tp, c.x + 22.0f, v.y + v.h - 98.0f, m * a);
      gfx_cor((GfxRect){ c.x + 22.0f, v.y + v.h - 62.0f, c.w - 44.0f, 8.0f }, 0.5f, 1, 1, 1, 0.55f * m * a);
      gfx_cor((GfxRect){ c.x + 22.0f, v.y + v.h - 42.0f, (c.w - 44.0f) * 0.62f, 8.0f }, 0.5f, 1, 1, 1, 0.30f * m * a);
    } }
  gfx_anel(c, 26.0f / c.h, 1.0f, 1, 1, 1, 0.10f * e * a);
  // O menu, ao lado do cartaz.
  mn.x = p.x + 40.0f + pw + 20.0f;
  mn.w = p.x + p.w - 28.0f - mn.x;
  mn.h = 3.0f * 76.0f + 28.0f;
  mn.y = c.y + 18.0f + (1.0f - m) * 12.0f;
  if (m > 0.01f) {
    vidro(mn, 28.0f, m * a);
    for (i = 0; i < 3; i++) {
      GfxRect r = { mn.x + 12.0f, mn.y + 14.0f + (float)i * 76.0f, mn.w - 24.0f, 68.0f };
      TxtLinha l = txt_linha_corta(TXT_ILHA_ITEM, i18n(ROT[i]), 243, 242, 239, 255, r.w - 76.0f);
      if (i == f) plrui_linha_foco(r, 22.0f, m * a);
      gfx_icone((GfxRect){ r.x + 20.0f, r.y + 22.0f, 24.0f, 24.0f }, ICO[i], i == f ? ar : 0.9f, i == f ? ag : 0.9f,
                i == f ? ab : 0.9f, m * a);
      txt_desenhar_alpha(l, r.x + 62.0f, r.y + (r.h - (float)l.h) * 0.5f, m * a);
    } }
}

// ----------------------------------------------------- cena 2: o novo hero
// O destaque da Home: botao menor na cor do acento, pontos que seguem o
// acento e um veu mais macio embaixo.
static void cenaHero(GfxRect p, float t, float a) {
  int red = ajustes_animacoes_reduzidas();
  float e = red ? 1.0f : sai3(janela(t, 0.10f, 0.50f));
  float ar, ag, ab, x = p.x + 52.0f, y = p.y + 110.0f;
  int i, ativo = red ? 0 : ((int)(t / 1.1f)) % 4;
  GfxRect b;
  ajustes_acento(&ar, &ag, &ab);
  arteFundo(1, p, a);
  { GfxRect v = { p.x, p.y, p.w, p.h };
    gfx_rect(v, 0, GFX_VEU_CARD, 0, 0, 0, PV_RAIO / v.h, 0.03f, 0.03f, 0.035f, 0.85f * a); }
  gfx_cor((GfxRect){ x, y + (1.0f - e) * 8.0f, 340.0f, 26.0f }, 0.5f, 1, 1, 1, 0.88f * e * a);
  gfx_cor((GfxRect){ x, y + 44.0f + (1.0f - e) * 8.0f, 460.0f, 12.0f }, 0.5f, 1, 1, 1, 0.40f * e * a);
  gfx_cor((GfxRect){ x, y + 66.0f + (1.0f - e) * 8.0f, 400.0f, 12.0f }, 0.5f, 1, 1, 1, 0.25f * e * a);
  b.w = botao_largura(i18n("Assistir"), NULL, 1);
  b.h = 56.0f;
  b.x = x;
  b.y = y + 106.0f + (1.0f - e) * 8.0f;
  botao_pilula(b, i18n("Assistir"), NULL, 1.0f, 1, 0, e * a);
  for (i = 0; i < 4; i++) {
    float on = i == ativo ? 1.0f : 0.0f, w = 12.0f + 22.0f * on;
    GfxRect d = { x + (float)i * 28.0f + (i > ativo ? 22.0f : 0.0f), b.y + b.h + 34.0f, w, 8.0f };
    gfx_cor(d, 0.5f, on ? ar : 1.0f, on ? ag : 1.0f, on ? ab : 1.0f, (on ? 0.95f : 0.30f) * e * a);
  }
}

// ----------------------------------------- cena 3: a ilha explica a espera
static void cenaIlha(GfxRect p, float t, float a) {
  int red = ajustes_animacoes_reduzidas();
  float dir = p.x + p.w - 36.0f, topo = p.y + 36.0f;
  float m = red ? 1.0f : sai3(janela(t, 0.30f, 0.55f));
  int fase = red || t >= 2.6f ? 1 : 0;
  char hora[16], txt[96];
  GfxRect r;
  { time_t ag = time(NULL);
    struct tm tmv;
    localtime_r(&ag, &tmv);
    hora_tela(hora, sizeof hora, &tmv); }
  arteFundo(2, p, a);
  gfx_cor(p, PV_RAIO / p.h, 0.02f, 0.02f, 0.025f, 0.42f * a);
  if (!fase) snprintf(txt, sizeof txt, i18n("Esperando %d add-ons"), 2);
  else snprintf(txt, sizeof txt, "%s", i18n("Verificando a fonte…"));
  { TxtLinha hl = txt_linha(TXT_W20_24B, hora, 243, 242, 239, 255);
    TxtLinha sl = txt_linha_corta(TXT_ILHA_GENERO, txt, 243, 242, 239, 255, p.w - 36.0f * 2.0f - 44.0f - (float)hl.w - 48.0f);
    float cheio = (float)hl.w + 44.0f + (float)sl.w + 48.0f;
    float pr = red ? 0.6f : fase ? janela(t, 2.6f, 1.4f) : janela(t, 0.9f, 1.7f);
    r.w = lerp(118.0f, cheio, m);
    r.h = NV_ILHA_H;
    r.x = dir - r.w;
    r.y = topo;
    vidro(r, NV_ILHA_H * 0.5f, a);
    if (m > 0.01f) {
      float tf = fase && !red ? sai3(janela(t, 2.6f, 0.30f)) : 1.0f;
      txt_desenhar_alpha(sl, r.x + 24.0f, topo + (NV_ILHA_H - (float)sl.h) * 0.5f + (1.0f - tf) * 6.0f, 0.85f * m * tf * a);
      { GfxRect bb = { r.x + 24.0f, r.y + r.h - 8.0f, (r.w - 48.0f) * pr, 3.0f };
        if (bb.w > 1.0f) gfx_cor(bb, 0.5f, 1, 1, 1, 0.85f * m * a); }
    }
    txt_desenhar_alpha(hl, dir - 22.0f - (float)hl.w, topo + (NV_ILHA_H - (float)hl.h) * 0.5f - 1.0f, a); }
}

// ------------------------------------------------------------------ a previa
typedef struct { const char *kicker, *linha; } Legenda;
static const Legenda LEGENDA[CENAS] = {
  { "Menu de contexto", "O cartaz vira o cartão de informação." },
  { "Início e Amigos", "Destaque mais leve e um botão só para adicionar." },
  { "Início mais rápido", "Sem baixar a qualidade. A ilha diz o que espera." },
};

static void cena(int i, GfxRect p, float t, float a) {
  if (a <= 0.004f) return;
  if (i == 0) cenaMenu(p, t, a);
  else if (i == 1) cenaHero(p, t, a);
  else cenaIlha(p, t, a);
}

static void desenhaPrevia(float x, float y, float a) {
  GfxRect p = { x, y, PV_W, PV_H };
  int volta = (int)floorf(relogio / CICLO_S), agora = volta % CENAS, antes = (volta + CENAS - 1) % CENAS;
  float dentro = relogio - (float)volta * CICLO_S;
  float e = (volta == 0 || ajustes_animacoes_reduzidas()) ? 1.0f : sai3(dentro / TROCA_S);
  gfx_cor(p, PV_RAIO / p.h, 0.055f, 0.058f, 0.068f, a);
  // A cena que sai fica parada no fim dela; a que entra comeca do zero.
  if (e < 1.0f) cena(antes, p, CICLO_S, a * (1.0f - e));
  cena(agora, p, dentro, a * e);

  // A legenda da cena: veu escuro no pe da previa, kicker e uma linha. Troca
  // em sequencia (a velha sai na primeira metade da passagem, a nova entra na
  // segunda), como os logos da 1.4.8: duas frases cruzadas viram borrao.
  { GfxRect pe = { p.x, p.y + p.h - 230.0f, p.w, 230.0f };
    float fOld = e < 1.0f ? 1.0f - anim_clamp(e / 0.45f, 0.0f, 1.0f) : 0.0f;
    float fNew = e < 1.0f ? anim_clamp((e - 0.45f) / 0.55f, 0.0f, 1.0f) : 1.0f;
    int k;
    gfx_rect(pe, 0, GFX_VEU_CARD, 0, 0, 0, PV_RAIO / pe.h, 0.03f, 0.03f, 0.035f, a);
    for (k = 0; k < 2; k++) {
      int c = k ? agora : antes;
      float f = k ? fNew : fOld, dy = (1.0f - f) * (k ? 8.0f : -6.0f);
      TxtLinha l;
      if (f <= 0.004f) continue;
      ajustes_ui_kicker(i18n(LEGENDA[c].kicker), p.x + 44.0f, p.y + p.h - 112.0f + dy, f * a);
      l = txt_linha_corta(TXT_V2_LN_B, i18n(LEGENDA[c].linha), 243, 242, 239, 255, p.w - 88.0f);
      txt_desenhar_alpha(l, p.x + 44.0f, p.y + p.h - 84.0f + dy, f * a);
    } }

  // Os tres tracos do ciclo, no pe a direita: o da cena enche no tempo dela.
  { int i;
    for (i = 0; i < CENAS; i++) {
      // Na altura do kicker (curto): a frase de baixo tem a largura toda.
      GfxRect tr = { p.x + p.w - 44.0f - (float)(CENAS - i) * 40.0f + 8.0f, p.y + p.h - 106.0f, 32.0f, 4.0f };
      gfx_cor(tr, 0.5f, 1, 1, 1, 0.22f * a);
      if (i == agora) {
        float pr = ajustes_animacoes_reduzidas() ? 1.0f : dentro / CICLO_S;
        tr.w *= anim_clamp(pr, 0.0f, 1.0f);
        if (tr.w > 1.0f) gfx_cor(tr, 0.5f, 1, 1, 1, 0.9f * a);
      }
    } }
  gfx_anel(p, PV_RAIO / p.h, 1.0f, 1, 1, 1, 0.07f * a);
}

// ------------------------------------------------------------- as mudancas
typedef struct { const char *grupo, *icone, *nome, *linha; } Item;
// UMA linha por frase, escrita para caber na Montserrat da TV (mais larga que
// a Inter do Mac). A altura de cada item e FIXA: a TCL do dono mostrou que
// medir o texto no primeiro quadro (antes de ele existir) deixava a lista
// descer por baixo dos botoes. Nada aqui depende de medida.
static const Item ITENS[] = {
  { "Mais bonito", "aj_ellipsis", "Menu de contexto",
    "O cartaz vira o cartão de informação." },
  { NULL, "aj_users", "Início e Amigos",
    "Destaque mais leve e um botão só para adicionar." },
  { NULL, "aj_sliders-horizontal", "Ajustes",
    "Espaçamento, proporção padrão e resolução automática." },
  { "Assistir", "aj_zap", "Início mais rápido",
    "Sem baixar a qualidade. A ilha diz o que espera." },
  { NULL, "aj_tv-minimal-play", "Player e TV ao vivo",
    "Pular créditos some sozinho. O canal reconecta." },
  { "Som e imagem", "aj_sparkles", "Dolby Vision em MKV nas TVs LG (experimental)",
    "Só para LG. No Android já toca normalmente." },
  { NULL, "aj_audio-lines", "DTS",
    "5.1 na LG. A Samsung avisa se a TV não toca." },
};
#define N_ITENS ((int)(sizeof ITENS / sizeof *ITENS))

#define ITEM_H     64.0f   // nome (34) + frase (30)
#define FRASE_DY   34.0f
#define GRUPO_H    34.0f
#define NOTA_H     30.0f
#define VAO_MIN    12.0f
#define VAO_MAX    24.0f
#define FOLGA_MIN  28.0f   // entre o fim da lista e o rodape

static float ox;   // deslocamento horizontal da pagina (a troca de pagina desliza)
static void item(int i, float y, float a) {
  float tx = TX + ox + ICONE + 22.0f, tw = TW - (ICONE + 22.0f);
  GfxRect d = { TX + ox, y - 2.0f, ICONE, ICONE };
  const char *frase = i18n(ITENS[i].linha);
  TxtLinha n, f;
  gfx_cor(d, 0.5f, 1, 1, 1, 0.075f * a);
  gfx_icone((GfxRect){ d.x + 11.0f, d.y + 11.0f, 22.0f, 22.0f }, ITENS[i].icone, 0.93f, 0.93f, 0.95f, a);
  n = txt_linha_corta(TXT_ILHA_NOME, i18n(ITENS[i].nome), 246, 247, 250, 255, tw);
  txt_desenhar_alpha(n, tx, y, a);
  f = txt_linha_corta(TXT_CAPTION, frase, 186, 192, 204, 255, tw);
  txt_desenhar_alpha(f, tx, y + FRASE_DY, 0.92f * a);
  if (txt_largura(TXT_CAPTION, frase) > (int)tw) frasesCortadas++;
  if (txt_largura(TXT_ILHA_NOME, i18n(ITENS[i].nome)) > (int)tw) frasesCortadas++;
}

static float grupo(const char *g, float y, float a) {
  float kw = ajustes_ui_kicker(i18n(g), TX + ox, y, a);
  gfx_cor((GfxRect){ TX + ox + kw + 16.0f, y + 8.0f, TW - kw - 16.0f, 1.0f }, 0, 1, 1, 1, 0.08f * a);
  return GRUPO_H;
}

static void paginaNovidades(float y0, float a, float dx) {
  float y = y0 + N_PAD - 6.0f, vao, total, limite;
  char t[64];
  int i, g = 0, ng = 0;
  if (a <= 0.004f) return;
  // O rodape (tracos e botoes) e reservado: a lista nunca desenha abaixo
  // de `limite`.
  limite = y0 + N_H - N_PAD - BOTAO_H_PRIMARIO - FOLGA_MIN;
  snprintf(t, sizeof t, i18n("Novidades da %s"), N202_VERSAO);
  { TxtLinha l = txt_linha_corta(TXT_NOV_TITULO, t, 248, 249, 252, 255, TW);
    txt_desenhar_alpha(l, TX + dx, y, a);
    y += 60.0f + 34.0f; }
  for (i = 0; i < N_ITENS; i++) if (ITENS[i].grupo) ng++;
  total = (float)N_ITENS * ITEM_H + (float)ng * GRUPO_H;
  vao = (limite - y - total) / ((float)(N_ITENS - 1) + 0.8f * (float)(ng - 1));
  if (vao > VAO_MAX) vao = VAO_MAX;
  if (vao < VAO_MIN) vao = VAO_MIN;
  ox = dx;
  frasesCortadas = 0;
  for (i = 0; i < N_ITENS; i++) {
    // Cascata de entrada: cada linha sobe 12 px, 45 ms depois da anterior.
    float local = ajustes_animacoes_reduzidas() ? 1.0f
                : anim_clamp((anim_suave(entrada) - 0.04f * (float)i) * 2.6f, 0.0f, 1.0f);
    float al = a * local, sobe = (1.0f - local) * 12.0f;
    if (ITENS[i].grupo) {
      if (g++) y += vao * 0.8f;
      if (y + GRUPO_H + ITEM_H > limite) break;
      y += grupo(ITENS[i].grupo, y + sobe, al);
    }
    if (y + ITEM_H > limite) break;
    item(i, y + sobe, al);
    y += ITEM_H;
    if (i < N_ITENS - 1) y += vao;
  }
  folgaLista = limite + FOLGA_MIN - y;
  ox = 0.0f;
}

static void paginaApoio(float y0, float a, float dx) {
  float y = y0 + N_PAD - 6.0f, x = TX + dx;
  int n = apoio_n(), i;
  if (a <= 0.004f) return;
  { TxtLinha l = txt_linha_corta(TXT_NOV_TITULO, i18n("Apoie o projeto"), 248, 249, 252, 255, TW);
    txt_desenhar_alpha(l, x, y, a);
    y += (float)l.h + 22.0f; }
  y += txt_bloco(TXT_V2_26, i18n("O Nuvio Legacy é gratuito. Se ele te ajuda e você quiser apoiar quem faz o app, aponte a câmera do celular para um dos códigos."),
                 200, 205, 215, x, y, TW, 36.0f, 0.9f * a, 4);
  y += 44.0f;
  { float lado = 300.0f, gap = 44.0f;
    for (i = 0; i < n; i++) {
      int q = apoio_qual(i);
      float qx = x + (float)i * (lado + gap);
      TxtLinha u = txt_linha_corta(TXT_V2_18, apoio_url_curta(q), 243, 242, 239, 255, lado + gap * 0.8f);
      // A luz atras do cartao claro: a peca mais importante da pagina sem
      // gritar (sombra, nao borda colorida).
      gfx_rect((GfxRect){ qx - 18.0f, y - 10.0f, lado + 36.0f, lado + 40.0f }, 0, GFX_SOMBRA, 0.8f, 0, 0, 0.2f,
               0, 0, 0, 0.5f * a);
      apoio_qr(q, qx, y, lado, a);
      // O selo (Ko-fi oficial; o do Patreon no mesmo formato) e o endereco,
      // centrados embaixo do codigo.
      apoio_rotulo(q, qx + lado * 0.5f, y + lado + 20.0f, 72.0f, 1, a);
      txt_desenhar_alpha(u, qx + (lado - (float)u.w) * 0.5f, y + lado + 20.0f + 72.0f + 10.0f, 0.6f * a);
    }
    y += lado + 20.0f + 72.0f + 10.0f + 24.0f + 34.0f; }
  { float ar, ag, ab;
    ajustes_acento_marca(&ar, &ag, &ab);
    gfx_icone((GfxRect){ x, y + 1.0f, 22.0f, 22.0f }, "aj_heart", ar, ag, ab, 0.9f * a);
    txt_bloco(TXT_CAPTION, i18n("É opcional e nada muda no app. Fica também em Ajustes › Sobre e ajuda."),
              186, 192, 204, x + 34.0f, y, TW - 34.0f, 28.0f, 0.92f * a, 2); }
}

static void ptFoco(int b, int nada) { (void)nada; foco = b; }

static void rodape(float y0, float a) {
  float yBase = y0 + N_H - N_PAD;
  const char *rot[3];
  float w[3], xd = N_X + N_W - N_PAD;
  int n = pagina ? P1_N : P0_N, i;
  if (!pagina) { rot[0] = i18n("Agora não"); rot[1] = i18n("Continuar"); }
  else { rot[0] = i18n("Voltar"); rot[1] = i18n("Concluir"); }
  for (i = 0; i < n; i++) w[i] = botao_largura(rot[i], NULL, i == n - 1);
  for (i = n - 1; i >= 0; i--) {
    int prim = i == n - 1;
    GfxRect r;
    r.w = w[i];
    r.h = prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
    r.x = xd - r.w;
    r.y = yBase - BOTAO_H_PRIMARIO * 0.5f - r.h * 0.5f;
    botao_pilula(r, rot[i], NULL, foco == i ? 1.0f : 0.0f, prim, 0, a);
    if (aberto) ponteiro_alvo(r.x, r.y, r.w, r.h, ptFoco, NULL, i, 0);
    xd = r.x - BOTAO_GAP;
  }
  // As duas paginas, como dois tracos no comeco do rodape. Somem quando os
  // botoes (rotulos longos, japones) chegam ate eles.
  if (xd + BOTAO_GAP > TX + 14.0f + 40.0f + 10.0f + 24.0f)
  { float ar, ag, ab, x = TX, yc = yBase - BOTAO_H_PRIMARIO * 0.5f;
    ajustes_acento(&ar, &ag, &ab);
    for (i = 0; i < 2; i++) {
      float on = i ? pag : 1.0f - pag, tw = 14.0f + 26.0f * on;
      gfx_cor((GfxRect){ x, yc - 3.0f, tw, 6.0f }, 0.5f, 1, 1, 1, (0.22f + 0.68f * on) * a);
      x += tw + 10.0f;
    } }
}

static void desenharCorpo(void) {
  float a = anim_suave(entrada), y0;
  if (entrada < 0.002f) return;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.78f * entrada);
  y0 = N_Y + (1.0f - a) * 28.0f;
  // O cartao: o vidro escuro das ilhas, um fio de luz e uma luz fria no alto.
  { GfxRect c = { N_X, y0, N_W, N_H };
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_rect((GfxRect){ c.x - 40.0f, c.y - 20.0f, c.w + 80.0f, c.h + 70.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.12f,
             0, 0, 0, 0.6f * a);
    gfx_cor(c, N_RAIO / N_H, 0.052f, 0.055f, 0.064f, 0.96f * a);
    gfx_luz_canto(c, N_RAIO / N_H, c.w * 0.78f, -c.h * 0.25f, c.h * 0.95f, ar, ag, ab, 0.10f * a);
    gfx_anel(c, N_RAIO / N_H, 1.2f, 1, 1, 1, 0.07f * a); }

  // Na pagina de apoio a previa recua (um veu, nao transparencia: a cena
  // translucida mostraria uma camada atraves da outra): os codigos sao o assunto.
  desenhaPrevia(N_X + N_PAD, y0 + N_PAD, a);
  if (pag > 0.004f)
    gfx_cor((GfxRect){ N_X + N_PAD, y0 + N_PAD, PV_W, PV_H }, PV_RAIO / PV_H, 0.052f, 0.055f, 0.064f, 0.55f * sai3(pag) * a);

  { float p = sai3(pag);
    paginaNovidades(y0, a * (1.0f - anim_clamp(pag * 1.6f, 0.0f, 1.0f)), -28.0f * p);
    paginaApoio(y0, a * anim_clamp((pag - 0.35f) / 0.65f, 0.0f, 1.0f), 28.0f * (1.0f - p)); }
  rodape(y0, a);
}

// Cartao de tela quase cheia: ampliado so se ainda couber (escala.h).
void novidades202_desenhar(Uint32 agora) {
  (void)agora;
  ESCALA_SE_COUBER_INI(N_W, N_H);
  desenharCorpo();
  ESCALA_SE_COUBER_FIM();
}
