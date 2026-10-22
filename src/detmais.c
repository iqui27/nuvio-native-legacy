// "Mais opcoes" da pagina do titulo — ver detmais.h.
#include "detmais.h"
#include "ajustes.h"
#include "anim.h"
#include "idioma.h"
#include "layout.h"
#include "plrui.h"
#include "ponteiro.h"
#include "text.h"
#include <string.h>

// A ILHA E A DO MENU DO CARTAZ (ctxmenu.c, CTX_*): 420 de largura, linhas de
// 60 com 4 de vao, 14 de respiro, raio 32, icone de 24 a 20 da borda e 18 ate
// o rotulo. Mesma medida para a pessoa reconhecer a peca de outra tela.
#define DM_W      420.0f
#define DM_LINHA   60.0f
#define DM_GAP      4.0f
#define DM_PAD     14.0f
#define DM_RAIO    32.0f
#define DM_ICONE   24.0f
#define DM_VAO     16.0f    // do circular a ilha
#define DM_BORDA   40.0f    // margem minima da tela

static int aberto, n, foco, okArmado;
static int linhas[DMAIS_N];
static float vis, visV, focoAnim[DMAIS_N];
static Uint32 tickAnt;

void detmais_abrir(const int disp[DMAIS_N]) {
  int i;
  n = 0;
  for (i = 0; i < DMAIS_N; i++) if (disp && disp[i]) linhas[n++] = i;
  if (!n) return;
  aberto = 1; foco = 0; okArmado = 0;
  for (i = 0; i < DMAIS_N; i++) focoAnim[i] = 0.0f;
}
void detmais_fechar(void) { aberto = 0; okArmado = 0; }
void detmais_zerar(void) { detmais_fechar(); vis = visV = 0.0f; }
int detmais_aberto(void) { return aberto; }
float detmais_visivel(void) { return vis; }
int detmais_n(void) { return n; }
int detmais_acao(int l) { return l >= 0 && l < n ? linhas[l] : -1; }
int detmais_foco(void) { return foco; }

const char *detmais_rotulo(int acao) {
  switch (acao) {
    case DMAIS_TRAILER:    return "Assistir trailer";
    case DMAIS_EXPLORAR:   return "Explorar a partir daqui";
    case DMAIS_ARTE:       return "Trocar arte";
    case DMAIS_RECOMENDAR: return "Recomendar a um amigo";
  }
  return "";
}
// Os mesmos glifos que os circulares tinham na linha: quem ja conhecia o
// trailer pela claquete o acha pela claquete.
const char *detmais_icone(int acao) {
  switch (acao) {
    case DMAIS_TRAILER:    return "aj_clapperboard";
    case DMAIS_EXPLORAR:   return "aj_compass";
    case DMAIS_ARTE:       return "arte";
    case DMAIS_RECOMENDAR: return "recomendar";
  }
  return "";
}

int detmais_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return -1;
  k = e->type == SDL_KEYDOWN || e->type == SDL_KEYUP ? e->key.keysym.sym : 0;
  if (e->type == SDL_KEYUP && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) {
    // So o OK descido AQUI conta: o KEYUP do OK que abriu a ilha nao escolhe.
    if (!okArmado) return -1;
    okArmado = 0; aberto = 0;
    return detmais_acao(foco);
  }
  if (e->type != SDL_KEYDOWN) return -1;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { okArmado = 1; return -1; }
  if (k == SDLK_UP && foco > 0) foco--;
  else if (k == SDLK_DOWN && foco + 1 < n) foco++;
  else if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
           k == SDLK_DELETE || k == SDLK_LEFT ||
           e->key.keysym.scancode == NV_SCANCODE_BACK) detmais_fechar();
  return -1;
}

GfxRect detmais_caixa(GfxRect anc) {
  GfxRect r;
  float h = DM_PAD * 2.0f + (float)n * DM_LINHA + (float)(n > 0 ? n - 1 : 0) * DM_GAP;
  r.w = DM_W; r.h = h;
  r.x = anc.x;
  if (r.x + r.w > NV_TELA_W - DM_BORDA) r.x = NV_TELA_W - DM_BORDA - r.w;
  // ABAIXO do circular, como um menu que desce do botao; perto do pe da tela
  // (filme com as quatro acoes) sobe e cobre o logo, que nao e lido agora.
  r.y = anc.y + anc.h + DM_VAO;
  if (r.y + r.h > NV_TELA_H - DM_BORDA) r.y = anc.y - DM_VAO - r.h;
  return r;
}

static void ptrFocar(int a, int b) { (void)b; if (aberto && a >= 0 && a < n) foco = a; }
static void ptrFora(int a, int b) { (void)a; (void)b; detmais_fechar(); }

void detmais_desenhar(GfxRect anc, float a) {
  Uint32 agora = SDL_GetTicks();
  float dt = tickAnt ? (float)(Uint32)(agora - tickAnt) * 0.001f : 0.0f;
  float p, pc, raio;
  GfxRect fim, r;
  int i;
  tickAnt = agora;
  vis = ajustes_animacoes_reduzidas() ? (aberto ? 1.0f : 0.0f)
                                      : anim_mola2(&visV, vis, aberto ? 1.0f : 0.0f, dt, 22.0f);
  for (i = 0; i < DMAIS_N; i++) {
    float alvo = aberto && i == foco ? 1.0f : 0.0f;
    focoAnim[i] = ajustes_animacoes_reduzidas() ? alvo
                  : focoAnim[i] + (alvo - focoAnim[i]) * anim_clamp(dt * 18.0f, 0.0f, 1.0f);
  }
  if (vis < 0.003f || n == 0 || a <= 0.002f) return;
  // O CIRCULAR VIRA A ILHA: o retangulo sai do "..." (um disco) e cresce ate
  // a caixa final, e o raio vai de meia altura ate o da ilha. O conteudo so
  // entra na segunda metade, quando ja ha lugar para ele.
  p = anim_clamp(vis, 0.0f, 1.0f);
  fim = detmais_caixa(anc);
  r.x = anc.x + (fim.x - anc.x) * p;
  r.y = anc.y + (fim.y - anc.y) * p;
  r.w = anc.w + (fim.w - anc.w) * p;
  r.h = anc.h + (fim.h - anc.h) * p;
  raio = anc.h * 0.5f + (DM_RAIO - anc.h * 0.5f) * p;
  ponteiro_camada();
  if (aberto) ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ptrFora, 0, 0);
  plrui_material(r, raio, 1, a * anim_clamp(p * 2.0f, 0.0f, 1.0f));
  pc = anim_clamp((p - 0.45f) / 0.55f, 0.0f, 1.0f) * a;
  if (pc <= 0.002f) return;
  gfx_recorte(r.x, r.y, r.w, r.h);
  for (i = 0; i < n; i++) {
    GfxRect l = { fim.x + DM_PAD, fim.y + DM_PAD + (float)i * (DM_LINHA + DM_GAP),
                  fim.w - DM_PAD * 2.0f, DM_LINHA };
    float f = focoAnim[i], lum = .72f + .28f * f;
    TxtLinha t;
    if (f > 0.01f) plrui_linha_foco(l, DM_LINHA * 0.5f, f * pc);
    gfx_icone((GfxRect){ l.x + 20.0f, l.y + (l.h - DM_ICONE) * 0.5f, DM_ICONE, DM_ICONE },
              detmais_icone(linhas[i]), .953f, .949f, .937f, lum * pc);
    t = txt_linha_corta(TXT_PG_ROTULO, i18n(detmais_rotulo(linhas[i])), 243, 242, 239, 255,
                        l.w - 20.0f - DM_ICONE - 18.0f - 20.0f);
    txt_desenhar_alpha(t, l.x + 20.0f + DM_ICONE + 18.0f, l.y + (l.h - t.h) * 0.5f, lum * pc);
    if (aberto && p > 0.85f) ponteiro_alvo(l.x, l.y, l.w, l.h, ptrFocar, NULL, i, 0);
  }
  gfx_sem_recorte();
}
