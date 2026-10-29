// DICAS DO CONTROLE v2 (dono, 14/10: "nao ta apple like; quero que tenha
// animacao mostrando o botao, abra o componente, explique o que e").
//
// Cada dica e uma sequencinha no estilo do onboarding da Apple:
//   1. o controle DE VERDADE da plataforma entra (desliza e cresce) e a tecla
//      da dica acende; tecla de toque faz a prensa (encolhe + onda) e tecla de
//      segurar mostra o ANEL enchendo pelo tempo do gesto;
//   2. o COMPONENTE DE VERDADE abre pela mesma funcao da tecla de verdade
//      (spainel_abrir, spot_abrir, central_abrir, ctx_abrir, menu_abrir),
//      enquanto o controle encolhe para o canto;
//   3. a legenda (titulo + uma frase) esmaece ao lado do componente;
//   4. o componente fecha e a proxima dica entra.
//
// Avanca sozinho (~5-6 s por dica), ← → pula, OK/Voltar termina e grava a
// marca. Com animacoes reduzidas nada desliza nem pulsa: cada dica e um
// quadro parado na mesma ordem. Sem blur e sem FBO proprio (TV fraca): o
// app continua sendo desenhado por baixo e esta camada so esmaece o fundo.
#include "dicascontrole.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "catalogo.h"
#include "central.h"
#include "ctxmenu.h"
#include "dados.h"
#include "gfx.h"
#include "home.h"
#include "idioma.h"
#include "ilha.h"
#include "layout.h"
#include "menu.h"
#include "ponteiro.h"
#include "salvospainel.h"
#include "spotlight.h"
#include "telefoneui.h"
#include "text.h"
#include <math.h>
#include <stdlib.h>

// O QUE CADA DICA ABRE. A ordem e a verificada em docs/dicas-controle-205.md.
enum { COMP_SALVOS, COMP_SPOT, COMP_CENTRAL, COMP_CTX, COMP_MENU };
static int compDaDica(int modo, int pagina) {
  if (modo == DICAS_TOQUE) {
    static const int t[] = { COMP_SALVOS, COMP_SPOT, COMP_CTX, COMP_MENU };
    return t[pagina];
  }
  static const int t[] = { COMP_SALVOS, COMP_SPOT, COMP_CENTRAL, COMP_CTX };
  return t[pagina];
}
static int ehSegurar(int modo, int pagina) {
  if (modo == DICAS_TOQUE) return pagina == 1 || pagina == 2;
  return pagina == 2 || pagina == 3;
}

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#define OCIOSO_S 1.5f // Home quieta e desenhada antes de abrir sozinho

static int aberto, pagina, modoTeste = -1, secoTeste, compAberto = -1, automatico;
static float entrada, espera, tempo, ocioso;
static int compPedido; // ja mandou abrir o componente nesta dica

int dicascontrole_modo(void) {
  if (modoTeste >= 0) return modoTeste;
  if (telefoneui_ativo()) return DICAS_TOQUE;
#if defined(NV_WEBOS)
  return DICAS_TV_LG;
#elif defined(NV_TPK) || defined(NV_TPK40) || defined(__EMSCRIPTEN__)
  return DICAS_TV_SAM;
#else
  return DICAS_TV_GTV;
#endif
}
void dicascontrole_teste_modo(int modo) { modoTeste = modo; }
void dicascontrole_teste_seco(int seco) { secoTeste = seco; }
int dicascontrole_aberto(void) { return aberto; }
int dicascontrole_visivel(void) { return entrada > .002f || aberto; }
int dicascontrole_ocupado(void) { return aberto || entrada > .002f || espera > 0; }
int dicascontrole_pagina(void) { return pagina; }

// TEMPOS DE CADA DICA (segundos). Toque: 5,4; segurar: 6,0.
#define T_PRENSA   1.05f  // tecla encolhe (toque) / anel comeca (segurar)
#define T_ONDA     1.15f  // a onda sai da tecla
#define T_ANELFIM  1.95f  // so segurar: anel cheio = fim do gesto
#define T_ABRE_T   2.10f  // toque: o componente abre
#define T_ABRE_S   2.35f  // segurar: idem
#define T_LEGENDA  2.75f  // a legenda esmaece
#define T_FECHA_T  5.00f  // toque: o componente fecha
#define T_FECHA_S  5.40f  // segurar: idem
#define DUR_T      5.40f  // toque: proxima dica
#define DUR_S      6.00f  // segurar: proxima dica

static float tAbre(int m, int p) { return ehSegurar(m, p) ? T_ABRE_S : T_ABRE_T; }
static float tFecha(int m, int p) { return ehSegurar(m, p) ? T_FECHA_S : T_FECHA_T; }
static float duracao(int m, int p) { return ehSegurar(m, p) ? DUR_S : DUR_T; }

// AS MESMAS FUNCOES DAS TECLAS DE VERDADE (app.c/menu.c/home.c chamam estas
// mesmas). Abrir e fechar por aqui deixa o app exatamente como estava.
static void compAbrir(int comp) {
  if (secoTeste) return;
  switch (comp) {
    case COMP_SALVOS: {
      float ix, iy, iw, ih;
      if (spainel_aberto()) break;
      if (ajustes_relogio_ligado() && ilha_rect(&ix, &iy, &iw, &ih))
        spainel_abrir_de(ix, iy, iw, ih);
      else spainel_abrir();
      break;
    }
    // Demo: abre sem o teclado nativo (spot_abrir chamaria o IME no Android).
    case COMP_SPOT: if (!spot_aberto()) spot_abrir_demo(); break;
    case COMP_CENTRAL: if (!central_aberta()) central_abrir(); break;
    case COMP_CTX: {
      if (ctx_aberto()) break;
      HomeItem it;
      int i = home_item_focado(&it) ? it.indice : (cat_n() > 0 ? 0 : -1);
      if (i >= 0) ctx_abrir(i);
      break;
    }
    case COMP_MENU: if (!menu_aberto()) menu_abrir(); break;
  }
}
static void compFechar(int comp) {
  if (secoTeste) return;
  switch (comp) {
    case COMP_SALVOS: if (spainel_aberto()) spainel_fechar(); break;
    case COMP_SPOT: if (spot_aberto()) spot_fechar(); break;
    case COMP_CENTRAL: if (central_aberta()) central_fechar(); break;
    case COMP_CTX: if (ctx_aberto()) {
        SDL_Event e = {0};
        e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_AC_BACK; ctx_evento(&e);
      }
      break;
    case COMP_MENU: if (menu_aberto()) menu_fechar(); break;
  }
}
void dicascontrole_abrir(void) {
  aberto = 1; automatico = 0; pagina = 0; entrada = 0; tempo = 0; espera = 0;
  compAberto = -1; compPedido = 0;
}
// Marker holds the app version that last showed the tips (per device).
static int jaVisto(void) {
  char *v = dados_ler(DICASCONTROLE_ARQUIVO);
  char atual[64];
  snprintf(atual, sizeof atual, "%s\n", NV_VERSAO);
  int igual = v && !strcmp(v, atual);
  free(v);
  return igual;
}
void dicascontrole_depois_novidades(void) {}
// Called every frame with "Home idle, nothing else open"; opens once per
// install/version after the screen stayed quiet for OCIOSO_S. The marker is
// read once per idle entry (-1 = ainda nao conferido), nao a cada quadro.
void dicascontrole_home_livre(int livre, float dt) {
  static int visto = -1;
  if (!livre || dicascontrole_ocupado()) { ocioso = 0; visto = -1; return; }
  if (visto < 0) visto = jaVisto();
  if (visto) return;
  ocioso += dt;
  if (ocioso >= OCIOSO_S) { ocioso = 0; dicascontrole_abrir(); automatico = 1; }
}
static void concluir(int a, int b) {
  (void)a; (void)b;
  if (!aberto) return;
  if (compAberto >= 0) { compFechar(compAberto); compAberto = -1; }
  aberto = 0; automatico = 0;
  {
    char v[64];
    snprintf(v, sizeof v, "%s\n", NV_VERSAO);
    dados_gravar(DICASCONTROLE_ARQUIVO, v);
  }
  ponteiro_cancelar_toque();
}
static void ir(int p, int b) {
  (void)b;
  if (!aberto || p < 0 || p > 3 || p == pagina) return;
  if (compAberto >= 0) { compFechar(compAberto); compAberto = -1; }
  pagina = p; tempo = 0; compPedido = 0;
}
void dicascontrole_evento(const SDL_Event *e) {
  if (!aberto || e->type != SDL_KEYDOWN || e->key.repeat) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (automatico) { concluir(0, 0); return; } // any key closes, none reaches Home
  if (k == SDLK_LEFT) ir(pagina - 1, 0);
  else if (k == SDLK_RIGHT) ir(pagina + 1, 0);
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
           k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
           k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) concluir(0, 0);
}
void dicascontrole_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (espera > 0) {
    espera -= dt;
    if (espera <= 0) dicascontrole_abrir();
  }
  int red = ajustes_animacoes_reduzidas();
  entrada = red ? (float)aberto : anim_rampa(entrada, (float)aberto, dt, aberto ? 320 : 180);
  if (!aberto) return;
  int m = dicascontrole_modo();
  tempo += dt;
  float abre = tAbre(m, pagina);
  if (!compPedido && tempo >= abre) {
    compPedido = 1;
    compAberto = compDaDica(m, pagina);
    compAbrir(compAberto);
  }
  if (compAberto >= 0 && tempo >= tFecha(m, pagina)) {
    compFechar(compAberto);
    compAberto = -1;
  }
  if (tempo >= duracao(m, pagina)) {
    if (pagina == 3) concluir(0, 0);
    else ir(pagina + 1, 0);
  }
}

const char *dicascontrole_tecla(int p) {
  int m = dicascontrole_modo();
  if (m == DICAS_TOQUE) {
    static const char *const toque[] = { "Relógio na tela", "Buscar", "Segure para opções", "Barra lateral" };
    return toque[p];
  }
  if (p == 0) return "CH+";
  if (p == 1) return "CH-";
  return p == 2 ? "Segurar CH+" : "Segure OK";
}
static const char *titulo(int p) {
  static const char *const tv[] = { "Painel de Salvos", "Spotlight", "Central de controle", "Segurar OK num cartaz" };
  static const char *const toque[] = { "Painel de Salvos", "Spotlight", "Segure para opções", "Barra lateral" };
  return dicascontrole_modo() == DICAS_TOQUE ? toque[p] : tv[p];
}
static const char *frase(int p) {
  if (dicascontrole_modo() == DICAS_TOQUE) {
    static const char *const toque[] = {
      "Toque no relógio para abrir Salvos e Avisos.",
      "Segure Buscar no menu para abrir o Spotlight.",
      "Toque e segure um cartaz para abrir o menu.",
      "Deslize da borda esquerda para abrir o menu."
    };
    return toque[p];
  }
  static const char *const tv[] = {
    "CH+ abre Salvos e Avisos na lateral direita.",
    "CH- abre o Spotlight, a busca de qualquer tela.",
    "Segure CH+ e a ilha vira um painel de ajustes rápidos, com atalhos que você escolhe.",
    "Segure OK num cartaz para ver opções do título."
  };
  return tv[p];
}

// ---- desenho ---------------------------------------------------------------

static void texto(const char *s, TxtEstilo estilo, float x, float y, float w, float a) {
  TxtLinha l = txt_linha_corta(estilo, i18n(s), 243, 242, 239, 255, w);
  txt_desenhar_alpha(l, x + (w - l.w) * .5f, y, a);
}

// Desenha uma forma miuda sem depender de cobertura de fonte:
// s=0 seta para tras, 1 casa (Inicio), 2 play, 3 asterisco (Assistente).
static void forma(float cx, float cy, float r, int s, float a) {
  switch (s) {
    case 0:
      gfx_cor((GfxRect){cx - r * .5f, cy - 1.6f, r, 3.2f}, .5f, 1, 1, 1, a);
      gfx_girar(.7854f, cx - r * .5f, cy);
      gfx_cor((GfxRect){cx - r * .78f, cy - 1.6f, r * .8f, 3.2f}, 0, 1, 1, 1, a);
      gfx_sem_girar();
      gfx_girar(-.7854f, cx - r * .5f, cy);
      gfx_cor((GfxRect){cx - r * .78f, cy - 1.6f, r * .8f, 3.2f}, 0, 1, 1, 1, a);
      gfx_sem_girar();
      break;
    case 1:
      gfx_cor((GfxRect){cx - r * .55f, cy - r * .1f, r * 1.1f, r * .7f}, 0, 1, 1, 1, a);
      gfx_girar(.7854f, cx, cy - r * .35f);
      gfx_cor((GfxRect){cx - r * .32f, cy - r * .67f, r * .64f, r * .64f}, 0, 1, 1, 1, a);
      gfx_sem_girar();
      break;
    case 2:
      gfx_cor((GfxRect){cx - r * .3f, cy - r * .5f, 3.2f, r}, 0, 1, 1, 1, a);
      gfx_cor((GfxRect){cx + r * .3f - 1.6f, cy - r * .5f, 3.2f, r}, 0, 1, 1, 1, a);
      break;
    default:
      gfx_cor((GfxRect){cx - 1.6f, cy - r * .55f, 3.2f, r * 1.1f}, 0, 1, 1, 1, a);
      gfx_cor((GfxRect){cx - r * .55f, cy - 1.6f, r * 1.1f, 3.2f}, 0, 1, 1, 1, a);
      break;
  }
}

// A TECLA: apagada some no corpo do controle (sem preenchimento, so o rotulo
// fraco); acesa vira o acento com halo e aro firme, inconfundivel mesmo em
// corpo claro. `prensa` 0..1 encolhe ela (a prensa do toque e o fim do segurar).
static float corpoLum = 0;   // luminancia do corpo do controle sendo desenhado
static struct { char rot[16]; int on; } tlog[8];
static int tlogN = -1;       // >=0: teste esta gravando as teclas desenhadas
static void tecla(GfxRect r, const char *s, int on, float prensa, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  float encolhe = on ? prensa * .08f : 0;
  GfxRect k = {r.x + r.w * encolhe, r.y + r.h * encolhe,
               r.w * (1 - 2 * encolhe), r.h * (1 - 2 * encolhe)};
  int claro = corpoLum > .5f;
  if (on) {
    gfx_anel((GfxRect){k.x - 8, k.y - 8, k.w + 16, k.h + 16}, .45f, 3, ar, ag, ab, .6f * a);
    gfx_anel((GfxRect){k.x - 3, k.y - 3, k.w + 6, k.h + 6}, .45f, 3,
             claro ? .1f : 1, claro ? .1f : 1, claro ? .12f : 1, .9f * a);
    gfx_cor(k, .45f, ar, ag, ab, a);
  }
  if (tlogN >= 0 && tlogN < 8) {
    snprintf(tlog[tlogN].rot, sizeof tlog[0].rot, "%s", s);
    tlog[tlogN++].on = on;
  }
  if (r.w < 56) return;   // remoto encolhido no canto: a tecla acesa basta, sem rotulo cortado
  float t = ajustes_tinta_foco();
  TxtLinha l = txt_linha_corta(TXT_CAPTION, i18n(s), on ? t : claro ? 40 : 243,
                             on ? t : claro ? 42 : 242, on ? t : claro ? 48 : 239, 255, r.w - 10);
  txt_desenhar_alpha(l, r.x + (r.w - l.w) * .5f, r.y + (r.h - l.h) * .5f, on ? a : .6f * a);
}
// Teste: grava as teclas desenhadas no proximo quadro (rotulo + acesa).
void dicascontrole_teste_gravar(int ligar) { tlogN = ligar ? 0 : -1; }
int dicascontrole_teste_teclas(const char **rot, int *on, int max) {
  int n = tlogN < max ? tlogN : max;
  for (int i = 0; i < n; i++) { rot[i] = tlog[i].rot; on[i] = tlog[i].on; }
  return n;
}
// Bandeja do rocker CH: as duas teclas dentro de uma pilula so, como no controle real.
static void rockerCH(GfxRect a1, GfxRect a2, float r, float g, float b, float a) {
  gfx_cor((GfxRect){a1.x, a1.y, a1.w, a2.y + a2.h - a1.y}, .22f, r, g, b, a);
}
// A ONDA do toque: dois aneis crescendo e sumindo (barato, sem blur).
static void onda(GfxRect r, float p, float a) {
  if (p <= 0 || p >= 1) return;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  for (int i = 0; i < 2; i++) {
    float pi = p - i * .18f;
    if (pi <= 0 || pi >= 1) continue;
    float cresce = 8 + pi * 44;
    GfxRect o = {r.x - cresce, r.y - cresce, r.w + 2 * cresce, r.h + 2 * cresce};
    gfx_anel(o, .45f, 2.5f, ar, ag, ab, (1 - pi) * .5f * a);
  }
}
// O ANEL DO SEGURAR: 14 pontos em volta da tecla, acesos na proporcao p.
static void anelSegurar(GfxRect r, float p, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  float cx = r.x + r.w * .5f, cy = r.y + r.h * .5f;
  float rx = r.w * .5f + 17, ry = r.h * .5f + 17;
  for (int i = 0; i < 14; i++) {
    float fi = (float)i / 14;
    float ang = -1.5708f + fi * 6.2832f;
    GfxRect d = {cx + cosf(ang) * rx - 5, cy + sinf(ang) * ry - 5, 10, 10};
    int aceso = fi <= p;
    gfx_cor(d, .5f, aceso ? ar : 1, aceso ? ag : 1, aceso ? ab : 1,
            (aceso ? .95f : .18f) * a);
  }
}

// Geometria da dica: qual tecla acende nesta pagina. As coordenadas sao de
// uma prancheta de 260x640 e cada controle as usa do seu jeito.
enum { TK_CHMAIS, TK_CHMENOS, TK_OK, TK_NENHUMA };
static int teclaDaDica(int modo, int p) {
  if (modo == DICAS_TOQUE) return TK_NENHUMA;
  if (p == 0 || p == 2) return TK_CHMAIS;
  if (p == 1) return TK_CHMENOS;
  return TK_OK;
}

// LG MAGIC REMOTE (MR24): corpo alto com as pontas redondas, roda/OK com o
// anel direcional em cima, Voltar e Inicio em pilulas, e os dois ROCKERs —
// VOL a esquerda, CH a direita, o CH dividido em CH+ (cima) e CH- (baixo).
static void remotoLG(float x, float y, float s, int tk, float prensa, float a) {
  GfxRect corpo = {x, y, 250 * s, 600 * s};
  gfx_cor(corpo, .17f, .105f, .11f, .125f, a);
  gfx_anel(corpo, .17f, 1.5f, 1, 1, 1, .10f * a);
  gfx_cor((GfxRect){x + 18 * s, y + 22 * s, 26 * s, 26 * s}, .5f, .30f, .12f, .12f, .7f * a); // power
  gfx_cor((GfxRect){x + 206 * s, y + 22 * s, 26 * s, 26 * s}, .5f, .20f, .21f, .24f, a);     // microfone
  float cx = x + 125 * s;
  GfxRect roda = {cx - 88 * s, y + 82 * s, 176 * s, 176 * s};
  gfx_cor(roda, .5f, .19f, .20f, .23f, a);
  gfx_anel((GfxRect){roda.x + 12 * s, roda.y + 12 * s, roda.w - 24 * s, roda.h - 24 * s},
           .5f, 2, 1, 1, 1, .14f * a); // a roda de rolagem
  tecla((GfxRect){cx - 42 * s, y + 128 * s, 84 * s, 84 * s}, "OK", tk == TK_OK, prensa, a);
  GfxRect volta = {x + 22 * s, y + 292 * s, 92 * s, 48 * s}, inicio = {x + 136 * s, y + 292 * s, 92 * s, 48 * s};
  gfx_cor(volta, .5f, .16f, .17f, .20f, a); gfx_cor(inicio, .5f, .16f, .17f, .20f, a);
  forma(volta.x + volta.w * .5f, volta.y + volta.h * .5f, 14 * s, 0, .6f * a);
  forma(inicio.x + inicio.w * .5f, inicio.y + inicio.h * .5f, 13 * s, 1, .6f * a);
  GfxRect vol = {x + 22 * s, y + 372 * s, 86 * s, 186 * s};
  gfx_cor(vol, .22f, .16f, .17f, .20f, a);
  texto("+", TXT_CAPTION, vol.x, vol.y + 20 * s, vol.w, .5f * a);
  texto("−", TXT_CAPTION, vol.x, vol.y + vol.h - 62 * s, vol.w, .5f * a);
  corpoLum = .12f;
  rockerCH((GfxRect){x + 142 * s, y + 372 * s, 86 * s, 88 * s}, (GfxRect){x + 142 * s, y + 470 * s, 86 * s, 88 * s}, .16f, .17f, .20f, a);
  tecla((GfxRect){x + 142 * s, y + 372 * s, 86 * s, 88 * s}, "CH+", tk == TK_CHMAIS, prensa, a);
  tecla((GfxRect){x + 142 * s, y + 470 * s, 86 * s, 88 * s}, "CH-", tk == TK_CHMENOS, prensa, a);
}

// SAMSUNG SMART REMOTE SOLARCELL (TM2361E): fininho e minimo, d-pad em anel
// com OK no meio, Voltar/Inicio/Play em circulos, rockers VOL e CH lado a
// lado e a faixa solar embaixo (o detalhe do modelo novo).
static void remotoSAM(float x, float y, float s, int tk, float prensa, float a) {
  GfxRect corpo = {x, y, 230 * s, 620 * s};
  gfx_cor(corpo, .16f, .09f, .095f, .11f, a);
  gfx_anel(corpo, .16f, 1.5f, 1, 1, 1, .10f * a);
  gfx_cor((GfxRect){x + 20 * s, y + 24 * s, 24 * s, 24 * s}, .5f, .30f, .12f, .12f, .7f * a); // power
  gfx_cor((GfxRect){x + 186 * s, y + 24 * s, 24 * s, 24 * s}, .5f, .20f, .21f, .24f, a);      // microfone
  float cx = x + 115 * s;
  GfxRect dpad = {cx - 80 * s, y + 74 * s, 160 * s, 160 * s};
  gfx_cor(dpad, .5f, .18f, .19f, .22f, a);
  tecla((GfxRect){cx - 38 * s, y + 116 * s, 76 * s, 76 * s}, "OK", tk == TK_OK, prensa, a);
  for (int i = 0; i < 3; i++) {
    GfxRect b = {x + (24 + i * 68) * s, y + 266 * s, 52 * s, 52 * s};
    gfx_cor(b, .5f, .16f, .17f, .20f, a);
  }
  forma(x + 50 * s, y + 292 * s, 13 * s, 0, .6f * a);
  forma(x + 118 * s, y + 292 * s, 12 * s, 1, .6f * a);
  forma(x + 186 * s, y + 292 * s, 12 * s, 2, .6f * a);
  GfxRect vol = {x + 24 * s, y + 352 * s, 84 * s, 196 * s};
  gfx_cor(vol, .22f, .16f, .17f, .20f, a);
  texto("+", TXT_CAPTION, vol.x, vol.y + 20 * s, vol.w, .5f * a);
  texto("−", TXT_CAPTION, vol.x, vol.y + vol.h - 62 * s, vol.w, .5f * a);
  corpoLum = .1f;
  rockerCH((GfxRect){x + 122 * s, y + 352 * s, 84 * s, 94 * s}, (GfxRect){x + 122 * s, y + 454 * s, 84 * s, 94 * s}, .16f, .17f, .20f, a);
  tecla((GfxRect){x + 122 * s, y + 352 * s, 84 * s, 94 * s}, "CH+", tk == TK_CHMAIS, prensa, a);
  tecla((GfxRect){x + 122 * s, y + 454 * s, 84 * s, 94 * s}, "CH-", tk == TK_CHMENOS, prensa, a);
  GfxRect solar = {x + 24 * s, y + 570 * s, 182 * s, 30 * s};
  gfx_cor(solar, .5f, .05f, .055f, .07f, a);
  for (int i = 1; i < 6; i++)
    gfx_cor((GfxRect){solar.x + i * solar.w / 6, solar.y + 3 * s, 1.5f, solar.h - 6 * s}, 0, 1, 1, 1, .07f * a);
}

// GOOGLE TV (controle do Google TV Streamer): pilula clara com as pontas
// redondas, d-pad redondo em cima, Voltar/Assistente/Inicio em circulos e
// os rockers VOL e CH na metade de baixo.
static void remotoGTV(float x, float y, float s, int tk, float prensa, float a) {
  GfxRect corpo = {x, y, 240 * s, 600 * s};
  gfx_cor(corpo, .2f, .88f, .89f, .91f, a);
  gfx_cor((GfxRect){x + 3 * s, y + 3 * s, corpo.w - 6 * s, corpo.h - 6 * s}, .19f, .93f, .94f, .95f, a);
  float cx = x + 120 * s;
  GfxRect dpad = {cx - 86 * s, y + 26 * s, 172 * s, 172 * s};
  gfx_cor(dpad, .5f, .82f, .83f, .85f, a);
  tecla((GfxRect){cx - 40 * s, y + 72 * s, 80 * s, 80 * s}, "OK", tk == TK_OK, prensa, a);
  for (int i = 0; i < 3; i++) {
    GfxRect b = {x + (28 + i * 68) * s, y + 238 * s, 50 * s, 50 * s};
    gfx_cor(b, .5f, .82f, .83f, .85f, a);
  }
  forma(x + 53 * s, y + 263 * s, 13 * s, 0, .6f * a);
  forma(x + 121 * s, y + 263 * s, 12 * s, 3, .6f * a);
  forma(x + 189 * s, y + 263 * s, 12 * s, 1, .6f * a);
  GfxRect vol = {x + 28 * s, y + 330 * s, 84 * s, 190 * s};
  gfx_cor(vol, .22f, .82f, .83f, .85f, a);
  texto("+", TXT_CAPTION, vol.x, vol.y + 20 * s, vol.w, .5f * a);
  texto("−", TXT_CAPTION, vol.x, vol.y + vol.h - 62 * s, vol.w, .5f * a);
  corpoLum = .93f;
  rockerCH((GfxRect){x + 128 * s, y + 330 * s, 84 * s, 90 * s}, (GfxRect){x + 128 * s, y + 430 * s, 84 * s, 90 * s}, .82f, .83f, .85f, a);
  tecla((GfxRect){x + 128 * s, y + 330 * s, 84 * s, 90 * s}, "CH+", tk == TK_CHMAIS, prensa, a);
  tecla((GfxRect){x + 128 * s, y + 430 * s, 84 * s, 90 * s}, "CH-", tk == TK_CHMENOS, prensa, a);
  gfx_cor((GfxRect){x + 74 * s, y + 544 * s, 42 * s, 42 * s}, .5f, .82f, .83f, .85f, a); // power
  gfx_cor((GfxRect){x + 128 * s, y + 544 * s, 42 * s, 42 * s}, .5f, .82f, .83f, .85f, a); // entrada
}

// O TOQUE: um celular com a ilha do relogio em cima. Cada dica mostra o
// gesto: toque no relogio, segurar a linha Buscar, segurar um cartaz e o
// deslize da borda esquerda (a trilha com o dedo andando).
static void celular(float x, float y, float s, int p, float g, float a) {
  GfxRect corpo = {x, y, 320 * s, 620 * s};
  gfx_cor(corpo, .12f, .24f, .25f, .28f, a);
  GfxRect tela = {x + 10 * s, y + 10 * s, 300 * s, 600 * s};
  gfx_cor(tela, .10f, .06f, .065f, .09f, a);
  GfxRect ilhaP = {x + 95 * s, y + 26 * s, 130 * s, 44 * s};
  gfx_cor(ilhaP, .5f, p == 0 ? .30f : .16f, p == 0 ? .31f : .17f, p == 0 ? .34f : .20f, a);
  texto("12:34", TXT_CAPTION, ilhaP.x, ilhaP.y + 8 * s, ilhaP.w, .8f * a);
  if (p == 1) {
    GfxRect busca = {x + 24 * s, y + 100 * s, 200 * s, 56 * s};
    gfx_cor(busca, .28f, .16f, .17f, .20f, a);
    texto("Buscar", TXT_CAPTION, busca.x, busca.y + 12 * s, busca.w, .8f * a);
    if (g > 0) anelSegurar(busca, g, a);
  }
  if (p == 2) {
    GfxRect cartaz = {x + 85 * s, y + 180 * s, 150 * s, 225 * s};
    gfx_cor(cartaz, .06f, .19f, .22f, .30f, a);
    gfx_cor((GfxRect){cartaz.x + 12 * s, cartaz.y + cartaz.h - 44 * s, cartaz.w - 24 * s, 10 * s}, .5f, 1, 1, 1, .25f * a);
    if (g > 0) anelSegurar(cartaz, g, a);
  }
  if (p == 3) {
    gfx_cor((GfxRect){x + 10 * s, y + 10 * s, 8 * s, 600 * s}, 0, .3f, .5f, .9f, (.25f + .35f * g) * a);
    float dedoX = x + (30 + g * 150) * s;
    for (int i = 0; i < 4; i++) {
      float al = .5f - i * .11f;
      if (al > 0) gfx_cor((GfxRect){dedoX - (28 + i * 26) * s, y + 300 * s, 18 * s, 18 * s}, .5f, 1, 1, 1, al * a);
    }
    gfx_cor((GfxRect){dedoX, y + 292 * s, 34 * s, 34 * s}, .5f, 1, 1, 1, .85f * a);
  } else if (p == 0 && g > 0) {
    onda(ilhaP, g, a);
  }
  gfx_cor((GfxRect){x + 120 * s, y + 592 * s, 80 * s, 5 * s}, .5f, 1, 1, 1, .5f * a);
}

static void remoto(int modo, float x, float y, float s, int p, float prensa, float gesto, float a) {
  int tk = teclaDaDica(modo, p);
  switch (modo) {
    case DICAS_TV_LG: remotoLG(x, y, s, tk, prensa, a); break;
    case DICAS_TV_SAM: remotoSAM(x, y, s, tk, prensa, a); break;
    case DICAS_TV_GTV: remotoGTV(x, y, s, tk, prensa, a); break;
    default: celular(x, y, s, p, gesto, a); break;
  }
}

// A pose do controle neste instante: entra de baixo crescendo (0-0,8 s),
// fica no centro ate o componente abrir e entao encolhe para o canto. Com
// animacoes reduzidas cada trecho vale o quadro parado do fim dele.
static void pose(int modo, float t, int red, float *x, float *y, float *s, float *a) {
  int toque = modo == DICAS_TOQUE;
  float rw = toque ? 320 : (modo == DICAS_TV_SAM ? 230 : 250);
  float rh = toque ? 620 : (modo == DICAS_TV_SAM ? 620 : 600);
  float sCheio = fminf(1.0f, (NV_TELA_H * .52f) / rh);
  float sCanto = sCheio * .38f;
  float entra = red ? 1 : anim_suave(fminf(t / .8f, 1));
  float sai = red ? (t > tAbre(modo, pagina) ? 1 : 0)
                  : anim_suave(anim_clamp((t - tAbre(modo, pagina)) / .9f, 0, 1));
  float cxCheio = toque ? NV_TELA_W * .5f : NV_TELA_W * .21f;
  float cxCanto = toque ? NV_TELA_W - rw * sCanto * .5f - 70
                        : rw * sCanto * .5f + 90;
  float yCheio = NV_TELA_H * .5f - rh * sCheio * .5f;
  float yCanto = NV_TELA_H - rh * sCanto - 96;
  float cx = anim_mistura(cxCheio, cxCanto, sai);
  *s = anim_mistura(sCheio, sCanto, sai);
  *x = cx - rw * *s * .5f;
  *y = anim_mistura(yCheio + (1 - entra) * 90, yCanto, sai);
  *a = entra;
}

static int deslizar(const PonteiroRolagem *e) {
  static float distancia;
  if (e->fase == PONT_ROL_INICIO) { distancia = 0; return aberto; }
  if (e->fase == PONT_ROL_MOVER && !e->eixoY) distancia += e->delta;
  if (e->fase == PONT_ROL_SOLTAR && !e->eixoY && fabsf(distancia) > 50)
    ir(pagina + (distancia < 0 ? 1 : -1), 0);
  return 1;
}

void dicascontrole_desenhar(Uint32 agora) {
  (void)agora;
  if (entrada <= .002f) return;
  int modo = dicascontrole_modo();
  int red = ajustes_animacoes_reduzidas();
  float a = anim_suave(entrada);
  float t = tempo;

  gfx_sem_recorte();
  // O VEU recua quando o componente abre: ele fica nítido (como no
  // onboarding da Apple, o que se explica nunca fica sob o esmaecer).
  float veu = red ? (compPedido ? .10f : .38f)
                  : anim_mistura(.38f, .10f,
                        anim_suave(anim_clamp((t - tAbre(modo, pagina)) / .9f, 0, 1)));
  gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0, 0, 0, 0, veu * a);
  if (aberto) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
    if (modo == DICAS_TOQUE) ponteiro_rolagem(deslizar);
  }

  // A TECLA E O GESTO DESTA DICA: a prensa do toque (ou o enchimento do
  // segurar) acontece antes de o componente abrir.
  float prensa = 0, gesto = 0;
  if (aberto) {
    if (ehSegurar(modo, pagina)) {
      float p = anim_clamp((t - T_PRENSA) / (T_ANELFIM - T_PRENSA), 0, 1);
      gesto = p;
      prensa = p >= 1 ? .8f : 0;
    } else if (modo == DICAS_TOQUE && pagina == 3) {
      // O deslize da borda: o dedo cruza a tela uma vez, devagar.
      gesto = anim_clamp((t - .9f) / 1.3f, 0, 1);
    } else if (t >= T_PRENSA && t < T_ONDA + .55f) {
      float f = (t - T_PRENSA) / .2f;
      prensa = f < 1 ? anim_suave(f) : anim_suave(anim_clamp(1 - (t - T_PRENSA - .2f) / .3f, 0, 1));
      gesto = anim_clamp((t - T_ONDA) / .55f, 0, 1);
    }
    if (red) { prensa = 0; gesto = modo == DICAS_TOQUE && pagina == 3 ? 1 : 0; }
  }
  float rx, ry, rs, ra;
  pose(modo, t, red, &rx, &ry, &rs, &ra);
  remoto(modo, rx, ry, rs, pagina, prensa, gesto, a * ra);

  // A LEGENDA, ao lado do componente aberto (o painel de Salvos fica a
  // direita, entao a legenda dele vai a esquerda; a da Central vai embaixo
  // da ilha; as demais a esquerda do centro).
  int comp = compDaDica(modo, pagina);
  float ca = red ? (t >= T_LEGENDA ? 1 : 0) * a
                 : a * anim_suave(anim_clamp((t - T_LEGENDA) / .5f, 0, 1)) *
                       (1 - anim_suave(anim_clamp((t - tFecha(modo, pagina)) / .4f, 0, 1)));
  if (ca > .003f) {
    float cw = fminf(560, NV_TELA_W - 96);
    float cx2 = NV_TELA_W * .5f - cw * .5f, cy2 = NV_TELA_H * .62f;
    if (modo != DICAS_TOQUE) {
      if (comp == COMP_SALVOS) { cx2 = 130; cy2 = NV_TELA_H * .5f - 150; }
      else if (comp == COMP_CENTRAL) { cx2 = NV_TELA_W * .5f - cw * .5f + 260; cy2 = NV_TELA_H * .56f; }
      else { cx2 = 130; cy2 = NV_TELA_H * .5f - 150; }
    }
    float th = txt_bloco_corta(TXT_ILHA_TITULO, i18n(titulo(pagina)), 243, 242, 239,
                               cx2, cy2, cw, 52, ca, 0);
    txt_bloco_corta(TXT_BODY, i18n(frase(pagina)), 198, 202, 212,
                    cx2, cy2 + th + 16, cw, 40, .92f * ca, 0);
  }

  // PONTINHOS E O BOTAO DE CONCLUIR (toque precisa de um alvo de toque para
  // terminar; na TV OK/Voltar do controle fazem isso).
  float dy = NV_TELA_H - 92;
  for (int i = 0; i < 4; i++) {
    GfxRect dot = {NV_TELA_W * .5f - 66 + i * 36, dy, 12, 12};
    gfx_cor(dot, .5f, 1, 1, 1, (i == pagina ? .95f : .25f) * a);
    if (aberto) ponteiro_alvo(dot.x - 16, dot.y - 16, 44, 44, NULL, ir, i, 0);
  }
  GfxRect fim = {NV_TELA_W * .5f - 130, NV_TELA_H - 78, 260, 58};
  botao_pilula(fim, "Concluir", NULL, 1, 1, 0, a);
  if (aberto) ponteiro_alvo(fim.x, fim.y, fim.w, fim.h, NULL, concluir, 0, 0);
  texto(modo == DICAS_TOQUE ? "Deslize para trocar · Concluir termina"
                            : "← → páginas · OK ou Voltar conclui",
        TXT_CAPTION, 0, NV_TELA_H - 132, NV_TELA_W, .5f * a);
}
