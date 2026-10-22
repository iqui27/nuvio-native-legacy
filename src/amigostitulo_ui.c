// A lista "O que os amigos acharam". Ver amigostitulo_ui.h.
#include "amigostitulo_ui.h"
#include "svdesenho.h"
#include "plrui.h"
#include "text.h"
#include "gfx.h"
#include "layout.h"
#include "anim.h"
#include "ajustes.h"
#include "idioma.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

// A FOLHA mora a direita, como as outras folhas da pagina (fontes, episodios):
// a arte do titulo continua visivel a esquerda e a pessoa sabe onde esta.
#define AMTUI_W      820.0f
#define AMTUI_MARGEM  48.0f
#define AMTUI_PAD     40.0f
#define AMTUI_TOPO   150.0f   // primeira linha (abaixo do titulo da folha)
#define AMTUI_LIN_H  128.0f
#define AMTUI_AV      72.0f

static AmigosTitulo lista;
static int aberta, foco, ultT, ultE;
static char titulo[160];
static float rolagem, rolagemVel, entrada;
static Uint32 ultimoQuadro;
#ifdef NV_TOUCH_UI
static ToqueRolagem toque;
static int toqueRolar(const PonteiroRolagem *e) {
  int r = toquerol_evento(&toque, e);
  if (r && e->fase == PONT_ROL_INICIO) rolagemVel = 0.0f;
  return r;
}
#endif

void amtui_abrir(const AmigosTitulo *t, int t2, int e2, const char *tit) {
  if (!t || t->n <= 0) return;
  lista = *t;
  ultT = t2; ultE = e2;
  snprintf(titulo, sizeof titulo, "%s", tit ? tit : "");
  aberta = 1; foco = 0;
  rolagem = rolagemVel = 0.0f;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toque);
#endif
  entrada = 0.0f; ultimoQuadro = 0;
}

int  amtui_aberta(void) { return aberta; }
void amtui_fechar(void) { aberta = 0; }

// PONTEIRO (#99). A linha sob o cursor vira o foco (a MESMA variavel das
// setas). O clique e o OK, que aqui fecha a folha — como no controle. A
// camada e de detail.c, que chama ponteiro_camada antes de amtui_desenhar.
static void ponteiroLinha(int i, int b) {
  (void)b;
  if (!aberta || i < 0 || i >= lista.n) return;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toque);
#endif
  foco = i;
}
int amtui_teste_foco(void) { return aberta ? foco : -1; }

void amtui_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberta || e->type != SDL_KEYDOWN) return;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) {
    if (toque.livre) {
      foco = (int)((rolagem + toque.regiao.h * 0.35f) / AMTUI_LIN_H);
      if (foco >= lista.n) foco = lista.n - 1;
    }
    toquerol_limpar(&toque);
  }
#endif
  k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK ||
      k == SDLK_RETURN || k == SDLK_KP_ENTER) { aberta = 0; return; }
  if (k == SDLK_DOWN && foco + 1 < lista.n) foco++;
  else if (k == SDLK_UP && foco > 0) foco--;
}

void amtui_desenhar(Uint32 agora) {
  float dt, a, x, y0, h, areaH, alvo, maxY;
  long long agoraS = (long long)time(NULL);
  int i;
  if (!aberta) return;
  dt = ultimoQuadro ? (float)(Uint32)(agora - ultimoQuadro) * 0.001f : 0.0f;
  if (dt > 0.08f) dt = 0.08f;
  ultimoQuadro = agora;
  entrada = ajustes_animacoes_reduzidas() ? 1.0f : entrada + (1.0f - entrada) * (1.0f - expf(-dt * 14.0f));
  if (entrada > 0.995f) entrada = 1.0f;
  a = entrada;

  // O veu escurece a pagina: a folha e modal.
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.0f, 0.0f, 0.0f, 0.45f * a);
  h = NV_TELA_H - 2.0f * AMTUI_MARGEM;
  x = NV_TELA_W - AMTUI_MARGEM - AMTUI_W + (1.0f - a) * 60.0f;
  y0 = AMTUI_MARGEM;
  plrui_material((GfxRect){ x, y0, AMTUI_W, h }, 36.0f, 1, a);

  { TxtLinha k = txt_linha_corta(TXT_ILHA_SEG, titulo, 243, 242, 239, 255, AMTUI_W - 2 * AMTUI_PAD);
    TxtLinha t = txt_linha(TXT_ILHA_TITULO, i18n("O que os amigos acharam"), 243, 242, 239, 255);
    txt_desenhar_alpha(k, x + AMTUI_PAD, y0 + 34.0f, a * 0.5f);
    txt_desenhar_alpha(t, x + AMTUI_PAD, y0 + 34.0f + k.h + 6.0f, a); }

  // A linha em foco fica dentro da area visivel (mola de 2a ordem, a mesma
  // curva das outras listas).
  areaH = h - AMTUI_TOPO - 24.0f;
  alvo = (float)(foco + 1) * AMTUI_LIN_H - areaH;
  maxY = (float)lista.n * AMTUI_LIN_H - areaH;
  if (alvo < 0.0f) alvo = 0.0f;
  if (maxY < 0.0f) maxY = 0.0f;
  if (alvo > maxY) alvo = maxY;
#ifdef NV_TOUCH_UI
  toquerol_vincular(&toque, (GfxRect){ x, y0 + AMTUI_TOPO, AMTUI_W, areaH }, gfx_escala(), 0.0f, maxY, 1, &rolagem);
  ponteiro_rolagem(toqueRolar);
  if (!toque.livre)
#endif
    rolagem = ajustes_animacoes_reduzidas() ? alvo : anim_mola2(&rolagemVel, rolagem, alvo, dt, NV_MOLA2_SCROLL);

  gfx_recorte(x, y0 + AMTUI_TOPO, AMTUI_W, areaH + 12.0f);
  for (i = 0; i < lista.n; i++) {
    const AmigoTit *am = &lista.a[i];
    float ly = y0 + AMTUI_TOPO + (float)i * AMTUI_LIN_H - rolagem;
    float tx = x + AMTUI_PAD + AMTUI_AV + 24.0f, tw = x + AMTUI_W - AMTUI_PAD - tx;
    char st[240], extra[240];
    TxtLinha ln, ls, le;
    float bloco, ty;
    if (ly + AMTUI_LIN_H < y0 + AMTUI_TOPO - 4.0f || ly > y0 + h) continue;
    if (a >= 1.0f)
      ponteiro_alvo_faixa(x + 16.0f, ly + 4.0f, AMTUI_W - 32.0f, AMTUI_LIN_H - 8.0f,
                          y0 + AMTUI_TOPO, y0 + AMTUI_TOPO + areaH + 12.0f, ponteiroLinha, NULL, i, 0);
    if (i == foco) plrui_linha_foco((GfxRect){ x + 16.0f, ly + 4.0f, AMTUI_W - 32.0f, AMTUI_LIN_H - 8.0f }, 24.0f, a);
    svd_avatar((GfxRect){ x + AMTUI_PAD, ly + (AMTUI_LIN_H - AMTUI_AV) * 0.5f, AMTUI_AV, AMTUI_AV },
               am->avatar, am->nome, am->id, a);
    if (am->agora) svd_ponto_vivo(x + AMTUI_PAD + AMTUI_AV - 9.0f, ly + (AMTUI_LIN_H + AMTUI_AV) * 0.5f - 9.0f,
                                  16.0f, 3.0f, a, agora);
    amigostitulo_frase_amigo(am, ultT, ultE, agoraS, st, sizeof st, extra, sizeof extra);
    // So recomendou (nao opinou nem viu, que se saiba): a recomendacao sobe
    // para a linha principal em vez de ficar sozinha na letra miuda.
    if (!st[0] && extra[0]) { snprintf(st, sizeof st, "%s", extra); extra[0] = 0; }
    ln = txt_linha_corta(TXT_ILHA_NOME, am->nome, 243, 242, 239, 255, tw);
    ls = txt_linha_corta(TXT_ILHA_SUB, st, 243, 242, 239, 255, tw);
    le = txt_linha_corta(TXT_ILHA_HORA, extra, 243, 242, 239, 255, tw);
    bloco = ln.h + (st[0] ? 6.0f + ls.h : 0.0f) + (extra[0] ? 6.0f + le.h : 0.0f);
    ty = ly + (AMTUI_LIN_H - bloco) * 0.5f;
    txt_desenhar_alpha(ln, tx, ty, a);
    ty += ln.h + 6.0f;
    if (st[0]) { txt_desenhar_alpha(ls, tx, ty, a * (i == foco ? 0.9f : 0.72f)); ty += ls.h + 6.0f; }
    if (extra[0]) txt_desenhar_alpha(le, tx, ty, a * (i == foco ? 0.7f : 0.5f));
  }
  gfx_sem_recorte();
  // Quantos ha alem dos guardados (o total real pode passar de AMT_MAX).
  if (lista.total > lista.n) {
    char b[48];
    TxtLinha l;
    snprintf(b, sizeof b, "+%d", lista.total - lista.n);
    l = txt_linha(TXT_ILHA_HORA, b, 243, 242, 239, 255);
    txt_desenhar_alpha(l, x + AMTUI_W - AMTUI_PAD - l.w, y0 + 40.0f, a * 0.5f);
  }
}

