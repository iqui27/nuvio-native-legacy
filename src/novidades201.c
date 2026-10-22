// Cartao de NOVIDADES DA 2.0.1. Ver novidades201.h.
//
// O PADRAO E O DA 1.4.8 (dono: "ficou lindo"): um cartao, a previa viva a
// esquerda, o texto a direita, as pilulas no canto de baixo. O que muda aqui:
//  - a previa tem TRES cenas montadas com as pecas de verdade do app, nao
//    figuras: a Central de controle crescendo da pilula do relogio, com os
//    atalhos de fabrica (centrallista_padrao) e os rotulos e valores que
//    Ajustes daria (ajustes_rapido_*); o menu de Ajustes em cartoes com o
//    foco andando; a linha "Velocidade" da folha de Audio com o valor no
//    formato do player (vel_rotulo, plrui_decimal);
//  - a lista e agrupada por assunto (kicker com fio), um icone por item e UMA
//    linha apagada dizendo o que mudou e onde fica;
//  - a ultima pagina e "Apoie o projeto": os dois QRs (apoio.h). "Agora nao"
//    na primeira pagina fecha direto: a pagina de apoio so aparece para quem
//    escolhe Continuar.
//
// CUSTO DE GPU: nenhuma passada de desfoque. A previa e uma arte do pacote
// (uma passada do tamanho da previa), veus de cor e retangulos pequenos; na
// troca de cena sao duas artes por ~0,5 s. O resto e texto em cache.
#include "novidades201.h"
#include "novidades202.h"
#include "apoio.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "central.h"
#include "centrallista.h"
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
#include "velocidade.h"
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

enum { P0_DEPOIS = 0, P0_CENTRAL, P0_CONTINUAR, P0_N };
enum { P1_VOLTAR = 0, P1_CONCLUIR, P1_N };

static int   aberto, decidido, pagina, foco = P0_CONTINUAR, pedido;
static float entrada, pag, relogio;
static char  dirArte[512] = "deploy/app/art";
static char  arte[CENAS][600];
// Folga entre o fim da lista e o topo do rodape no ultimo quadro desenhado,
// e quantas frases precisaram de reticencias (testes).
static float folgaLista = 999.0f;
// Rotulos longos (russo, hungaro): os tres botoes nao cabem na coluna de
// texto e invadiriam a previa. Ai "Abrir a Central" sai (a Central continua
// no CH+) e o rodape fica com dois.
static int   semCentral;
static int   frasesCortadas;

// A arte de fundo de cada cena (do pacote, sem rede). A da Central e uma
// home; a dos Ajustes nao tem arte (o Frost e cor); a do player e uma cena.
static const int ARTE_N[CENAS] = { 12, -1, 15 };

void novidades201_dir(const char *d) {
  int i;
  if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d);
  for (i = 0; i < CENAS; i++)
    if (ARTE_N[i] >= 0) snprintf(arte[i], sizeof arte[i], "%s/%02d.jpg", dirArte, ARTE_N[i]);
    else arte[i][0] = 0;
}

static void pedirArtes(void) {
  int i;
  if (!arte[0][0]) novidades201_dir(NULL);
  for (i = 0; i < CENAS; i++) if (arte[i][0]) tex_obter_hero(arte[i]);
}

int novidades201_previa_pronta(void) {
  int i;
  for (i = 0; i < CENAS; i++) if (arte[i][0] && !tex_obter_hero(arte[i])) return 0;
  return 1;
}

int novidades201_aberto(void) { return aberto; }
int novidades201_pagina(void) { return pagina; }
int novidades201_foco(void) { return foco; }
void novidades201_teste_relogio(float s) { relogio = s; }
void novidades201_teste_esquecer(void) { decidido = 0; aberto = 0; }
float novidades201_teste_folga(void) { return folgaLista; }
int novidades201_teste_cortadas(void) { return frasesCortadas; }

int novidades201_pedido(void) {
  int p = pedido;
  pedido = N201_PEDIU_NADA;
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

void novidades201_abrir(void) { comecar(0.0f); }

void novidades201_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N201_ARQ);
  if (s) { free(s); return; }
  // A 2.0.2 SUBSTITUI este cartao: quem vai ve-la (ou ja viu) nao recebe a
  // 2.0.1 depois. Fica visto.
  if (novidades202_aberto() || (s = dados_ler(N202_ARQ))) { free(s); dados_gravar(N201_ARQ, "1\n"); return; }
  // Sem a marca do guia da 2.0 o guia abre agora (novidades20_primeira_vez,
  // logo depois desta): ele ja conta o app inteiro, e um segundo cartao em
  // seguida seria demais. Fica visto.
  s = dados_ler("novidades-20-guia.txt");
  if (!s) { dados_gravar(N201_ARQ, "1\n"); return; }
  free(s);
  comecar(0.0f);
}

static void fechar(int oQue) {
  aberto = 0;
  pedido = oQue;
  dados_gravar(N201_ARQ, "1\n");
}

static void irPagina(int p) {
  pagina = p;
  foco = p ? P1_CONCLUIR : P0_CONTINUAR;
  if (ajustes_animacoes_reduzidas()) pag = (float)p;
}

static void ok(void) {
  if (!pagina) {
    if (foco == P0_DEPOIS) fechar(N201_PEDIU_NADA);
    else if (foco == P0_CENTRAL) fechar(N201_PEDIU_CENTRAL);
    else irPagina(1);
  } else {
    if (foco == P1_VOLTAR) irPagina(0);
    else fechar(N201_PEDIU_NADA);
  }
}

void novidades201_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int n;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  n = pagina ? P1_N : P0_N;
  if (k == SDLK_LEFT)  {
    if (foco > 0) foco--;
    if (!pagina && semCentral && foco == P0_CENTRAL) foco--;
    return;
  }
  if (k == SDLK_RIGHT) {
    if (foco < n - 1) foco++;
    if (!pagina && semCentral && foco == P0_CENTRAL) foco++;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { ok(); return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (pagina) irPagina(0);
    else fechar(N201_PEDIU_NADA);
  }
}

void novidades201_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (aberto) {
    pedirArtes();
    if (novidades201_previa_pronta()) relogio += dt;
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

// ------------------------------------------- cena 1: a Central de controle
// O painel e o da central de verdade (central_previa_desenhar): a mesma
// faixa de informacao, os mesmos botoes com os nomes curtos, os atalhos de
// fabrica. Aqui so a pilula do relogio que estica ate ele e o foco andando.
#define CC_PIL_W   118.0f
#define CC_PIL_H   NV_ILHA_H

// Os atalhos de fabrica, na ordem em que a central os mostra.
static int ccOps(int *op, int max) {
  CentralLista l;
  int i, n = 0;
  centrallista_padrao(&l);
  for (i = 0; i < l.n && n < max; i++) {
    const CentralItem *c = central_catalogo(l.item[i]);
    int o = c ? ajustes_rapido_op(c->chave) : -1;
    if (o < 0) continue;
    op[n++] = o;
  }
  return n;
}

static void cenaCentral(GfxRect p, float t, float a) {
  int red = ajustes_animacoes_reduzidas();
  float dir = p.x + p.w - 36.0f, topo = p.y + 36.0f;
  float m = red ? 1.0f : sai3(janela(t, 0.95f, 0.62f));
  float c = red ? 1.0f : sai3(janela(t, 1.35f, 0.45f));
  float ccH = central_previa_altura();
  GfxRect r;
  char hora[16];
  { time_t ag = time(NULL);
    struct tm tmv;
    localtime_r(&ag, &tmv);
    hora_tela(hora, sizeof hora, &tmv); }
  arteFundo(0, p, a);
  gfx_cor(p, PV_RAIO / p.h, 0.02f, 0.02f, 0.025f, 0.38f * a);

  // SEGURAR: a pilula do relogio estica com "Segure para abrir a central" e
  // a barra enchendo, como ilha_atividade faz; a tecla CH+ ao lado, apertada.
  { TxtLinha hl = txt_linha(TXT_W20_24B, hora, 243, 242, 239, 255);
    TxtLinha sl = txt_linha(TXT_ILHA_GENERO, i18n("Segure para abrir a central"), 243, 242, 239, 255);
    float seg = red ? 0.0f : janela(t, 0.10f, 0.25f) * (1.0f - janela(t, 0.95f, 0.25f));
    float pilW = lerp(CC_PIL_W, (float)hl.w + 44.0f + (float)sl.w + 24.0f, sai3(seg));
    float pr = janela(t, 0.25f, 0.70f);
    r.w = lerp(pilW, CENTRAL_PREVIA_W, m);
    r.h = lerp(CC_PIL_H, ccH, m);
    r.x = dir - r.w;
    r.y = topo;
    // A sombra do painel, que so existe quando ele ja e painel.
    if (m > 0.01f)
      gfx_rect((GfxRect){ r.x - 30.0f, r.y - 10.0f, r.w + 60.0f, r.h + 60.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.12f,
               0, 0, 0, 0.55f * m * a);
    vidro(r, lerp(CC_PIL_H * 0.5f, 30.0f, m), a);
    if (seg > 0.01f) {   // a tecla CH+ apertada, sempre a esquerda da pilula
      TxtLinha l = txt_linha(TXT_AJ_KBD, "CH+", 243, 242, 239, 255);
      float kw = (float)l.w + 26.0f, kh = 36.0f, ap = 1.0f - 0.06f * seg;
      GfxRect kr = { r.x - 14.0f - kw, topo + (CC_PIL_H - kh) * 0.5f, kw, kh };
      GfxRect kd = { kr.x + kr.w * (1.0f - ap) * 0.5f, kr.y + kr.h * (1.0f - ap) * 0.5f, kr.w * ap, kr.h * ap };
      vidro(kd, kh * 0.5f, seg * a);
      txt_desenhar_alpha(l, kd.x + (kd.w - (float)l.w) * 0.5f, kd.y + (kd.h - (float)l.h) * 0.5f, seg * a);
    }
    if (seg > 0.01f) {
      txt_desenhar_alpha(sl, r.x + 24.0f, topo + (CC_PIL_H - (float)sl.h) * 0.5f, 0.8f * seg * a);
      { GfxRect b = { r.x + 24.0f, r.y + r.h - 8.0f, (r.w - 48.0f) * pr, 3.0f };
        if (b.w > 1.0f) gfx_cor(b, 0.5f, 1, 1, 1, 0.85f * seg * a); }
    }
    // A hora fica onde estava: a pilula cresce em volta dela.
    txt_desenhar_alpha(hl, dir - 22.0f - (float)hl.w, topo + (CC_PIL_H - (float)hl.h) * 0.5f - 1.0f, a); }
  if (c <= 0.01f) return;

  // O foco entra no primeiro botao, anda ate um interruptor (de preferencia
  // desligado) e o OK o vira: o que a central faz, sem gravar nada aqui.
  { int op[CENTRAL_MAX], n = ccOps(op, CENTRAL_MAX), i, alvo = 1, f = -1, inv = -1;
    for (i = 1; i < n; i++) if (ajustes_rapido_ligado(op[i]) == 0) { alvo = i; break; }
    if (i == n) for (i = 1; i < n; i++) if (ajustes_rapido_ligado(op[i]) >= 0) { alvo = i; break; }
    if (red) f = alvo;
    else if (t >= 2.75f) f = alvo;
    else if (t >= 2.05f) f = 0;
    if (!red && t >= 3.25f) inv = alvo;
    central_previa_desenhar(dir - CENTRAL_PREVIA_W, topo + (1.0f - c) * 10.0f, c * a, f, inv); }
}

// ----------------------------------------------------- cena 2: os Ajustes
typedef struct { const char *nome, *icone, *chave; } AjCard;
static const AjCard CARDS[6] = {
  { "Tela inicial", "aj_layout-dashboard", NULL },
  { "Reprodução", "aj_circle-play", "qualidade" },
  { "Aparência", "aj_palette", "fundoLocal" },
  { "Fontes e addons", "aj_puzzle", NULL },
  { "Trailers", "aj_clapperboard", "trailerAuto" },
  { "Sobre e ajuda", "aj_info", NULL },
};
// O caminho do foco: Tela inicial, Reproducao, Fontes e addons, Aparencia.
static const int CAM[4] = { 0, 1, 3, 2 };
static const float CAM_T[4] = { 0.0f, 1.15f, 2.15f, 3.15f };

static float pesoFoco(int card, float t) {
  int i;
  float w = 0.0f;
  if (ajustes_animacoes_reduzidas()) return card == CAM[0] ? 1.0f : 0.0f;
  for (i = 0; i < 4; i++) {
    float in = i == 0 ? janela(t, 0.25f, 0.2f) : janela(t, CAM_T[i], 0.22f);
    float out = i < 3 ? janela(t, CAM_T[i + 1], 0.22f) : 0.0f;
    if (CAM[i] == card) w += in * (1.0f - out);
  }
  return anim_clamp(w, 0.0f, 1.0f);
}

static void cenaAjustes(GfxRect p, float t, float a) {
  float ar, ag, ab, x = p.x + 40.0f, w = p.w - 80.0f, y = p.y + 52.0f;
  int i, tinta = ajustes_tinta_foco();
  ajustes_acento(&ar, &ag, &ab);
  // O FROST: superficie fria, tingida pelo acento por cima e por baixo.
  gfx_cor(p, PV_RAIO / p.h, 0.085f, 0.092f, 0.110f, a);
  gfx_luz_canto(p, PV_RAIO / p.h, p.w * 0.18f, -p.h * 0.10f, p.h * 1.05f, ar, ag, ab, 0.34f * a);
  gfx_luz_canto(p, PV_RAIO / p.h, p.w * 1.05f, p.h * 0.95f, p.h * 0.75f, ar, ag, ab, 0.14f * a);

  { GfxRect b = { x, y, 330.0f, 54.0f };
    TxtLinha l = txt_linha(TXT_V2_LN, i18n("Buscar"), 243, 242, 239, 255);
    gfx_cor(b, 0.5f, 1, 1, 1, 0.09f * a);
    gfx_icone((GfxRect){ b.x + 20.0f, b.y + 15.0f, 24.0f, 24.0f }, "menu_search", 1, 1, 1, 0.8f * a);
    txt_desenhar_alpha(l, b.x + 58.0f, b.y + (b.h - (float)l.h) * 0.5f, 0.8f * a); }
  y += 54.0f + 34.0f;
  { float cw = (w - 16.0f) * 0.5f, ch = 140.0f;
    for (i = 0; i < 6; i++) {
      GfxRect c = { x + (float)(i % 2) * (cw + 16.0f), y + (float)(i / 2) * (ch + 18.0f), cw, ch };
      float f = pesoFoco(i, t);
      int tr = (int)lerp(243.0f, (float)tinta, f);
      const char *val = NULL;
      float tx = c.x + 22.0f + 56.0f + 18.0f, tw = c.x + c.w - 18.0f - tx;
      // Entrada em cascata: cada cartao sobe 8 px e aparece 40 ms depois do
      // anterior. So na primeira passagem pela cena.
      float e = ajustes_animacoes_reduzidas() ? 1.0f : sai3(janela(t, 0.04f * (float)i, 0.35f));
      c.y += (1.0f - e) * 8.0f;
      gfx_cor(c, 22.0f / c.h, 1, 1, 1, 0.055f * e * a);
      if (f > 0.01f) {
        gfx_rect((GfxRect){ c.x - 14.0f, c.y - 8.0f, c.w + 28.0f, c.h + 26.0f }, 0, GFX_SOMBRA, 0.9f, 0, 0, 0.3f,
                 ar, ag, ab, 0.45f * f * a);
        gfx_cor(c, 22.0f / c.h, ar, ag, ab, f * a);
      }
      { GfxRect ti = { c.x + 22.0f, c.y + (c.h - 56.0f) * 0.5f, 56.0f, 56.0f };
        float ci = (float)tr / 255.0f;
        gfx_cor(ti, 16.0f / 56.0f, 1, 1, 1, (0.10f - 0.04f * f) * e * a);
        gfx_icone((GfxRect){ ti.x + 14.0f, ti.y + 14.0f, 28.0f, 28.0f }, CARDS[i].icone, ci, ci, ci, e * a); }
      if (CARDS[i].chave) {
        int op = ajustes_rapido_op(CARDS[i].chave);
        if (op >= 0) val = ajustes_rapido_valor(op);
      }
      { TxtLinha n = txt_linha_corta(TXT_V2_LN_B, i18n(CARDS[i].nome), tr, tr, tr, 255, tw);
        if (val && val[0]) {
          TxtLinha v = txt_linha_corta(TXT_V2_18, i18n(val), tr, tr, tr, 255, tw);
          float th = (float)n.h + 2.0f + (float)v.h, ty = c.y + (c.h - th) * 0.5f;
          txt_desenhar_alpha(n, tx, ty, e * a);
          txt_desenhar_alpha(v, tx, ty + (float)n.h + 2.0f, 0.7f * e * a);
        } else txt_desenhar_alpha(n, tx, c.y + (c.h - (float)n.h) * 0.5f, e * a); }
    } }
}

// --------------------------------------------- cena 3: a velocidade no player
static void cenaVelocidade(GfxRect p, float t, float a) {
  int red = ajustes_animacoes_reduzidas();
  static const int PASSO[3] = { VEL_NORMAL, 125, 150 };
  int k = red ? 2 : t < 1.5f ? 0 : t < 2.5f ? 1 : 2, v = PASSO[k];
  float ar, ag, ab;
  GfxRect s = { p.x + p.w - 36.0f - 470.0f, p.y + 36.0f, 470.0f, 330.0f };
  float e = red ? 1.0f : sai3(janela(t, 0.15f, 0.45f));
  char val[16];
  ajustes_acento(&ar, &ag, &ab);
  arteFundo(2, p, a);
  gfx_cor(p, PV_RAIO / p.h, 0.02f, 0.02f, 0.025f, 0.28f * a);
  // A folha de Audio, entrando da direita como no player.
  s.x += (1.0f - e) * 40.0f;
  vidro(s, 30.0f, e * a);
  { float x = s.x + 26.0f, w = s.w - 52.0f, y = s.y + 26.0f, sa = e * a;
    TxtLinha tt = txt_linha(TXT_ILHA_PERGUNTA, i18n("Áudio"), 243, 242, 239, 255);
    txt_desenhar_alpha(tt, x, y, sa);
    y += (float)tt.h + 18.0f;
    // Duas faixas (a primeira escolhida) e, embaixo, a linha da velocidade.
    { static const char *const FX[2] = { "Português", "Inglês" };
      int i;
      for (i = 0; i < 2; i++) {
        TxtLinha l = txt_linha(TXT_ILHA_FORTE, i18n(FX[i]), 219, 219, 217, 255);
        txt_desenhar_alpha(l, x + 10.0f, y + (52.0f - (float)l.h) * 0.5f, sa * (i ? 0.6f : 1.0f));
        if (!i) gfx_icone((GfxRect){ x + w - 34.0f, y + 14.0f, 24.0f, 24.0f }, "aj_check", ar, ag, ab, sa);
        y += 56.0f;
      } }
    { GfxRect lr = { x - 6.0f, y + 6.0f, w + 12.0f, 86.0f };
      float yc = lr.y + lr.h * 0.5f, dx = lr.x + lr.w - 18.0f;
      TxtLinha lv, ln, ls;
      plrui_linha_foco(lr, 22.0f, sa);
      gfx_cor((GfxRect){ lr.x + 16.0f, yc - 24.0f, 48.0f, 48.0f }, 0.5f, 1, 1, 1, 0.12f * sa);
      gfx_icone((GfxRect){ lr.x + 28.0f, yc - 12.0f, 24.0f, 24.0f }, "aj_gauge", 1, 1, 1, sa);
      vel_rotulo(val, sizeof val, v);
      plrui_decimal(val);
      if (v != VEL_NORMAL)
        lv = txt_linha(TXT_ILHA_FORTE, val, (int)(ar * 255.0f), (int)(ag * 255.0f), (int)(ab * 255.0f), 255);
      else lv = txt_linha(TXT_ILHA_FORTE, val, 255, 255, 253, 255);
      { GfxRect d = { dx - 32.0f, yc - 16.0f, 32.0f, 32.0f };
        gfx_cor(d, 0.5f, 1, 1, 1, 0.10f * sa);
        gfx_icone((GfxRect){ d.x + 6.0f, d.y + 6.0f, 20.0f, 20.0f }, "pl_chevron-right", 1, 1, 1, sa);
        dx -= 32.0f + 10.0f; }
      { char larga[16]; float slot;
        vel_rotulo(larga, sizeof larga, 175); plrui_decimal(larga);
        slot = (float)txt_largura(TXT_ILHA_FORTE, larga);
        // O valor novo entra de baixo, o velho sai por cima.
        { float mv = red ? 1.0f : sai3(janela(t, k == 1 ? 1.5f : k == 2 ? 2.5f : 99.0f, 0.25f));
          txt_desenhar_alpha(lv, dx - slot + (slot - (float)lv.w) * 0.5f,
                             yc - (float)lv.h * 0.5f + (1.0f - mv) * 10.0f, sa * (k ? mv : 1.0f)); }
        dx -= slot + 10.0f; }
      { GfxRect d = { dx - 32.0f, yc - 16.0f, 32.0f, 32.0f };
        gfx_cor(d, 0.5f, 1, 1, 1, 0.10f * sa);
        gfx_icone((GfxRect){ d.x + 6.0f, d.y + 6.0f, 20.0f, 20.0f }, "pl_chevron-left", 1, 1, 1, sa);
        dx -= 32.0f + 18.0f; }
      ln = txt_linha_corta(TXT_ILHA_FORTE, i18n("Velocidade"), 255, 255, 253, 255, dx - (lr.x + 80.0f));
      ls = txt_linha_corta(TXT_ILHA_GENERO, i18n(v == VEL_NORMAL ? "Normal" : "Só neste vídeo"),
                           122, 122, 120, 255, dx - (lr.x + 80.0f));
      { float th = (float)ln.h + 5.0f + (float)ls.h, ty = yc - th * 0.5f;
        txt_desenhar_alpha(ln, lr.x + 80.0f, ty, sa);
        txt_desenhar_alpha(ls, lr.x + 80.0f, ty + (float)ln.h + 5.0f, sa); } } }
}

// ------------------------------------------------------------------ a previa
typedef struct { const char *kicker, *linha; } Legenda;
static const Legenda LEGENDA[CENAS] = {
  { "Central de controle", "Segure CH+ para abrir." },
  { "Ajustes", "Categorias em cartões, no fundo Frost." },
  { "Velocidade", "Na folha de Áudio, de 0,75x a 2x." },
};

static void cena(int i, GfxRect p, float t, float a) {
  if (a <= 0.004f) return;
  if (i == 0) cenaCentral(p, t, a);
  else if (i == 1) cenaAjustes(p, t, a);
  else cenaVelocidade(p, t, a);
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
  { "Navegar", "aj_layout-dashboard", "Central de controle",
    "Segure CH+ para abrir, com atalhos que você escolhe." },
  { NULL, "aj_settings-2", "Ajustes",
    "Categorias em cartões e uma busca que sugere." },
  { "Assistir", "aj_gauge", "Velocidade",
    "De 0,75x a 2x, na folha de Áudio." },
  { NULL, "aj_captions", "Legendas",
    "Versão exata por idioma. Add-ons acham mais legendas." },
  { NULL, "aj_puzzle", "Fontes e addons",
    "Tamanho mínimo e máximo ao tocar sozinho." },
  { "Correções", "aj_shield-check", "Estabilidade",
    "LG e Android não fecham sozinhos. Android TV mais leve." },
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
}

static float grupo(const char *g, float y, float a) {
  float kw = ajustes_ui_kicker(i18n(g), TX + ox, y, a);
  gfx_cor((GfxRect){ TX + ox + kw + 16.0f, y + 8.0f, TW - kw - 16.0f, 1.0f }, 0, 1, 1, 1, 0.08f * a);
  return GRUPO_H;
}

static void paginaNovidades(float y0, float a, float dx) {
  float y = y0 + N_PAD - 6.0f, vao, total, limite;
  char t[64];
  int i, g = 0, ng = 0, nota;
  if (a <= 0.004f) return;
  // O rodape (tracos e botoes) e reservado: a lista nunca desenha abaixo
  // de `limite`.
  limite = y0 + N_H - N_PAD - BOTAO_H_PRIMARIO - FOLGA_MIN;
  snprintf(t, sizeof t, i18n("Novidades da %s"), N201_VERSAO);
  { TxtLinha l = txt_linha_corta(TXT_NOV_TITULO, t, 248, 249, 252, 255, TW);
    txt_desenhar_alpha(l, TX + dx, y, a);
    y += 60.0f + 34.0f; }
  for (i = 0; i < N_ITENS; i++) if (ITENS[i].grupo) ng++;
  total = (float)N_ITENS * ITEM_H + (float)ng * GRUPO_H;
  nota = limite - y - total - NOTA_H - VAO_MIN >= ((float)(N_ITENS - 1) + 0.8f * (float)(ng - 1)) * VAO_MIN;
  if (nota) total += NOTA_H + VAO_MIN;
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
  if (nota && i == N_ITENS && y + VAO_MIN + NOTA_H <= limite) {
    float local = ajustes_animacoes_reduzidas() ? 1.0f
                : anim_clamp((anim_suave(entrada) - 0.04f * (float)N_ITENS) * 2.6f, 0.0f, 1.0f);
    // Nota final, apagada e sem icone, so quando couber com folga.
    const char *nt = i18n("Também: coleções grandes, links longos de addon e árabe.");
    TxtLinha l = txt_linha_corta(TXT_CAPTION, nt, 160, 166, 178, 255, TW);
    y += VAO_MIN;
    txt_desenhar_alpha(l, TX + dx, y + (1.0f - local) * 12.0f, 0.85f * a * local);
    if (txt_largura(TXT_CAPTION, nt) > (int)TW) frasesCortadas++;
    y += NOTA_H;
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
  if (!pagina) { rot[0] = i18n("Agora não"); rot[1] = i18n("Abrir a Central"); rot[2] = i18n("Continuar"); }
  else { rot[0] = i18n("Voltar"); rot[1] = i18n("Concluir"); }
  for (i = 0; i < n; i++) w[i] = botao_largura(rot[i], NULL, i == n - 1);
  if (!pagina) {
    // Os tres tem de caber na coluna de texto (os tracos somem antes).
    semCentral = w[0] + w[1] + w[2] + 2.0f * BOTAO_GAP > TW;
    if (semCentral && foco == P0_CENTRAL) foco = P0_CONTINUAR;
  }
  for (i = n - 1; i >= 0; i--) {
    int prim = i == n - 1;
    GfxRect r;
    if (!pagina && semCentral && i == P0_CENTRAL) continue;
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
void novidades201_desenhar(Uint32 agora) {
  (void)agora;
  ESCALA_SE_COUBER_INI(N_W, N_H);
  desenharCorpo();
  ESCALA_SE_COUBER_FIM();
}
