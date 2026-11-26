// CARTOES DOS ARQUIVOS DE UMA FONTE (partescartao.h): a contagem para a parte
// seguinte de um filme em partes, nos creditos da parte, e a fileira com os
// outros arquivos da fonte no fim de um especial/OVA aberto pela lista.
// O molde e o do cartao de proximo episodio (posplay.c): ilha sobre o video
// cheio, veu suave na base, botoes e dicas do plrui.
#include "partescartao.h"
#include "idioma.h"
#include "gfx.h"
#include "text.h"
#include "layout.h"
#include "anim.h"
#include "ajustes.h"
#include "plrui.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

enum { PC_NADA = 0, PC_CONTAGEM, PC_ARQUIVOS };
static int modo, visivel, foco, pedido = PC_PEDIDO_NENHUM;
static float anim, rolar;
// Contagem: "Parte N de M", dispara em fimEm (0 = sem contagem).
static int parteN, parteTotal, contagemSeg;
static Uint32 fimEm;
// Arquivos: a lista da fonte (so o que a fileira mostra).
#define PC_MAX 96
static struct { char nome[PARTES_NOME]; int tipo; uint64_t bytes; } arq[PC_MAX];
static int nArq, atual;

int partescartao_visivel(void) { return visivel; }
int partescartao_modo_contagem(void) { return visivel && modo == PC_CONTAGEM; }
int partescartao_modo_arquivos(void) { return visivel && modo == PC_ARQUIVOS; }

void partescartao_contagem(int n, int total, Uint32 agora, int seg) {
  if (seg < 3) seg = 10;
  modo = PC_CONTAGEM; visivel = 1; foco = 0;
  parteN = n; parteTotal = total; contagemSeg = seg;
  fimEm = (agora + (Uint32)seg * 1000u) | 1u;
  pedido = PC_PEDIDO_NENHUM;
}

void partescartao_arquivos(const PartesLista *l, int k) {
  int i;
  if (!l || l->n <= 1) return;
  nArq = 0;
  for (i = 0; i < l->n && nArq < PC_MAX; i++, nArq++) {
    snprintf(arq[nArq].nome, sizeof arq[nArq].nome, "%s", l->arq[i].nome);
    arq[nArq].tipo = l->arq[i].tipo; arq[nArq].bytes = l->arq[i].bytes;
  }
  atual = k;
  modo = PC_ARQUIVOS; visivel = 1; rolar = 0;
  // Foco no arquivo seguinte da lista (o atual e o ultimo: nele mesmo).
  foco = k + 1 < nArq ? k + 1 : (k >= 0 && k < nArq ? k : 0);
  fimEm = 0;
  pedido = PC_PEDIDO_NENHUM;
}

void partescartao_fechar(void) {
  visivel = 0; fimEm = 0;
}

void partescartao_zerar(void) {
  visivel = 0; modo = PC_NADA; fimEm = 0; anim = 0; pedido = PC_PEDIDO_NENHUM; nArq = 0;
}

int partescartao_pedido(void) { int p = pedido; pedido = PC_PEDIDO_NENHUM; return p; }

void partescartao_atualizar(float dt, Uint32 agora) {
  anim = anim_mola(anim, visivel ? 1.0f : 0.0f, dt, visivel ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  if (visivel && modo == PC_CONTAGEM && fimEm && (Sint32)(agora - fimEm) >= 0) {
    fimEm = 0;
    pedido = PC_PEDIDO_AGORA;
    visivel = 0;
  }
}

int partescartao_evento(const SDL_Event *e) {
  int k;
  if (!visivel || e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    // Contagem: Voltar = continuar assistindo; a parte ainda troca no fim.
    if (modo == PC_CONTAGEM) pedido = PC_PEDIDO_FICAR;
    visivel = 0; fimEm = 0;
    return 1;
  }
  if (k == SDLK_DOWN) {
    if (modo == PC_CONTAGEM) pedido = PC_PEDIDO_FICAR;
    visivel = 0; fimEm = 0;
    return 2;
  }
  if (modo == PC_CONTAGEM) {
    if (k == SDLK_RIGHT && foco == 0) { foco = 1; return 1; }
    if (k == SDLK_LEFT && foco == 1) { foco = 0; return 1; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      pedido = foco == 0 ? PC_PEDIDO_AGORA : PC_PEDIDO_FICAR;
      visivel = 0; fimEm = 0;
      return 1;
    }
    return 1;   // a contagem e um convite: o resto das teclas nao vaza ao OSD
  }
  if (k == SDLK_RIGHT) { if (foco + 1 < nArq) foco++; return 1; }
  if (k == SDLK_LEFT) { if (foco > 0) foco--; return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (foco >= 0 && foco < nArq && foco != atual) { pedido = foco; visivel = 0; }
    return 1;
  }
  return 1;
}

// O anel da contagem, como o do posplay.
static void anel(float cx, float cy, float frac, float a) {
  float ar, ag, ab;
  int k, n = 36;
  ajustes_acento(&ar, &ag, &ab);
  gfx_anel((GfxRect){ cx - 12.0f, cy - 12.0f, 24.0f, 24.0f }, 0.5f, 3.0f, 1, 1, 1, 0.16f * a);
  for (k = 0; k < n; k++) {
    float t = (float)k / (float)n, ang = t * 6.2831853f;
    if (t > frac) break;
    gfx_cor((GfxRect){ cx + sinf(ang) * 10.5f - 1.5f, cy - cosf(ang) * 10.5f - 1.5f, 3.0f, 3.0f }, 0.5f, ar, ag, ab, a);
  }
}

static void ponteiroBotao(int i, int b) { (void)b; if (visivel) foco = i; }

static const char *rotuloTipo(int tipo) {
  if (tipo == PARTE_ESPECIAL) return i18n("Especial");
  if (tipo == PARTE_EXTRA) return i18n("Extra");
  return i18n("Vídeo");
}

#define PC_CHIP_W 340.0f
#define PC_CHIP_H 92.0f
#define PC_CHIP_GAP 16.0f

static void desenharCorpo(Uint32 agora) {
  float a = anim, x = NV_ILHA_MARGEM_D, sobe = (1.0f - a) * 20.0f;
  if (a < 0.01f || modo == PC_NADA) return;
  if (visivel) ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);   // anteparo
  gfx_rect((GfxRect){ 0.0f, NV_TELA_H - 420.0f, NV_TELA_W, 420.0f }, 0, GFX_VEU_BAIXO,
           0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.62f * a);
  if (modo == PC_CONTAGEM) {
    GfxRect ilha = { x, NV_TELA_H - 344.0f + sobe, 1040.0f, 232.0f };
    int resta = fimEm > agora ? (int)((fimEm - agora + 999) / 1000) : 0;
    char cab[64], tit[64];
    float tx, ty, by, b0, b1;
    if (ilha.x + ilha.w > NV_TELA_W - 40.0f) ilha.x = (NV_TELA_W - ilha.w) * 0.5f;
    plrui_material_video(ilha, 36.0f, a);
    tx = ilha.x + 28.0f; ty = ilha.y + 24.0f;
    if (resta > 0) snprintf(cab, sizeof cab, i18n("A seguir em %d s"), resta);
    else snprintf(cab, sizeof cab, "%s", i18n("A seguir"));
    anel(tx + 13.0f, ty + 13.0f, resta > 0 ? (float)resta / (float)contagemSeg : 1.0f, a);
    plrui_kicker(cab, tx + 38.0f, ty + 4.0f, 243, 242, 239, a * 0.62f);
    snprintf(tit, sizeof tit, i18n("Parte %d de %d"), parteN, parteTotal);
    { TxtLinha t = txt_linha_corta(TXT_ILHA_TITULO, tit, 243, 242, 239, 255, ilha.w - 56.0f);
      txt_desenhar_alpha(t, tx, ty + 38.0f, a); }
    by = ilha.y + ilha.h - 24.0f - 60.0f;
    b0 = plrui_botao_video(tx, by, "Começar agora", "pl_play-f", foco == 0 ? 1.0f : 0.0f, a);
    b1 = plrui_botao_video(tx + b0 + 16.0f, by, "Continuar assistindo", NULL, foco == 1 ? 1.0f : 0.0f, a);
    if (visivel && a > 0.99f && a < 1.01f) {
      ponteiro_alvo(tx, by, b0, 60.0f, ponteiroBotao, NULL, 0, 0);
      ponteiro_alvo(tx + b0 + 16.0f, by, b1, 60.0f, ponteiroBotao, NULL, 1, 0);
    }
    { const char *k[2] = { "\xe2\x86\x93", "Voltar" }, *r[2] = { "Voltar ao player", "Ficar nesta parte" };
      plrui_dicas_video(k, r, 2, tx + b0 + 16.0f + b1 + 22.0f, by + 30.0f, 0, a); }
    return;
  }
  // ARQUIVOS DESTA FONTE: a fileira, o atual marcado e o foco no seguinte.
  {
    GfxRect ilha = { x, NV_TELA_H - 300.0f + sobe, NV_TELA_W - 2.0f * x, 188.0f };
    float cy = ilha.y + 24.0f, cx0 = ilha.x + 28.0f, vista = ilha.w - 56.0f, cx;
    int i;
    plrui_material_video(ilha, 36.0f, a);
    { float kw = plrui_kicker("Arquivos desta fonte", cx0, cy + 4.0f, 243, 242, 239, a * 0.62f);
      const char *k[3] = { "OK", "\xe2\x86\x93", "Voltar" }, *r[3] = { "Assistir", "Voltar ao player", "Fechar" };
      (void)kw;
      plrui_dicas_video(k, r, 3, ilha.x + ilha.w - 28.0f, cy + 12.0f, 1, a); }
    cy += 44.0f;
    // Rolagem: o chip em foco sempre inteiro na vista.
    { float esq = foco * (PC_CHIP_W + PC_CHIP_GAP), dir = esq + PC_CHIP_W;
      if (esq < rolar) rolar = esq;
      if (dir > rolar + vista) rolar = dir - vista;
      if (rolar < 0) rolar = 0; }
    gfx_recorte(cx0, cy - 8.0f, vista, PC_CHIP_H + 16.0f);
    cx = cx0 - rolar;
    for (i = 0; i < nArq; i++, cx += PC_CHIP_W + PC_CHIP_GAP) {
      GfxRect r = { cx, cy, PC_CHIP_W, PC_CHIP_H };
      char sub[96];
      if (r.x + r.w < cx0 - 1.0f) continue;
      if (r.x > cx0 + vista) break;
      if (i == foco) plrui_pilula_foco_video(r, a);
      else plrui_botao_repouso_video(r, a);
      // Na pilula de foco (clara) o texto e escuro, como nos botoes do plrui.
      { int c = i == foco ? 22 : 243;
        TxtLinha l = txt_linha_corta(TXT_ILHA_SUB, arq[i].nome, c, c, c + (i == foco ? 2 : -4), 255, r.w - 40.0f);
        txt_desenhar_alpha(l, r.x + 22.0f, r.y + 16.0f, a); }
      if (i == atual) snprintf(sub, sizeof sub, "%s", i18n("Tocando agora"));
      else if (arq[i].bytes > 0)
        snprintf(sub, sizeof sub, "%s \xc2\xb7 %.1f GB", rotuloTipo(arq[i].tipo), (double)arq[i].bytes / 1e9);
      else snprintf(sub, sizeof sub, "%s", rotuloTipo(arq[i].tipo));
      { int c = i == foco ? 22 : 243;
        TxtLinha l = txt_linha_corta(TXT_MINI, sub, c, c, c + (i == foco ? 2 : -4), i == foco ? 190 : 150, r.w - 40.0f);
        txt_desenhar_alpha(l, r.x + 22.0f, r.y + r.h - 16.0f - (float)l.h, a); }
      if (visivel && a > 0.99f && a < 1.01f) {
        float ax = fmaxf(r.x, cx0), aw = fminf(r.x + r.w, cx0 + vista) - ax;
        if (aw > 0) ponteiro_alvo(ax, r.y, aw, r.h, ponteiroBotao, NULL, i, 0);
      }
    }
    gfx_sem_recorte();
  }
}

void partescartao_desenhar(Uint32 agora) {
  ESCALA_INI();
  desenharCorpo(agora);
  ESCALA_FIM();
}
