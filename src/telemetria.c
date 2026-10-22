// Ver telemetria.h. Mesmo molde do cartao de novidades (novidades134.c): uma
// pagina, entrada por rampa, marca em arquivo para nao perguntar de novo.
#include "telemetria.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "idiomacod.h"
#define NV_ESCALA_TELA_ATIVA   // mede pela tela do fator ativo (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include <stdio.h>
#include <stdlib.h>

#define TL_ARQ  "telemetria-perguntado.txt"
#define TL_ABRIR_MS  280.0f
#define TL_FECHAR_MS 160.0f

static int   aberto, decidido;
static float entrada;

int telemetria_aberto(void) { return aberto; }

void telemetria_primeira_vez(void) {
#ifndef __EMSCRIPTEN__
  decidido = 1;   // so no Tizen
  return;
#else
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(TL_ARQ);
  if (s) { free(s); return; }
  aberto = 1; entrada = 0.0f;
#endif
}

static void fechar(int sim) {
  aberto = 0;
  ajustes_definir_envio_auto(sim);
  dados_gravar(TL_ARQ, sim ? "1\n" : "0\n");   /* marca, nao texto de tela */
  printf("[telemetria] envio automatico: %s\n", sim ? "ligado" : "desligado");
  fflush(stdout);
}

static int foco;   // 0 = "Sim, pode mandar", 1 = "Agora nao"

void telemetria_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT) { foco = 0; return; }
  if (k == SDLK_RIGHT) { foco = 1; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { fechar(foco == 0); return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) { fechar(0); return; }
}

void telemetria_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? TL_ABRIR_MS : TL_FECHAR_MS);
}

// O CARTAO NO GLASS UI (mockup do registro, quadro 13, 03/10): a ilha modal,
// kicker + pergunta + o texto inteiro, e tres cartoes do que vai, do que nao
// vai e de quando — a mesma informacao do texto, legivel de longe. "Sim, pode
// mandar" e o botao em foco, cheio no acento; "Agora nao" tambem e o Voltar.
#define TL_TX 243, 242, 239
static TxtLinha tlT(TxtEstilo e, const char *s) { return txt_linha(e, s, TL_TX, 255); }
static float tlCaps(TxtEstilo e, const char *s, float track, float x, float y, float a) {
  char up[160];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(e, up, TL_TX, x, y, a, track);
}
static void tlNeutro(GfxRect r, float raioPx, float vidA, float sr, float sg, float sb, float a) {
  if (ajustes_vidro()) gfx_cor(r, raioPx / r.h, 1, 1, 1, vidA * a);
  else gfx_cor(r, raioPx / r.h, sr, sg, sb, a);
}
// PONTEIRO (#99): foco pela mesma variavel das setas; o OK do clique fecha
// pelo caminho de sempre (telemetria_evento).
static void ponteiroFoco(int i, int b) { (void)b; if (aberto && (i == 0 || i == 1)) foco = i; }
int telemetria_teste_foco(void) { return foco; }

static float tlBotao(const char *rot, float x, float y, int id, float a) {
  int f = foco == id;
  float ar, ag, ab, w;
  int t = ajustes_tinta_foco();
  TxtLinha l = f ? txt_linha(TXT_ILHA_ITEM, rot, t, t, t, 255) : tlT(TXT_ILHA_ITEM, rot);
  w = 28 + l.w + 28;
  if (aberto && entrada > 0.99f)   // so assentado (sem o deslize da entrada)
    ponteiro_alvo(x, y, w, 60, ponteiroFoco, NULL, id, 0);
  if (f) {
    ajustes_acento(&ar, &ag, &ab);
    gfx_rect((GfxRect){ x - 14, y - 2, w + 28, 60 + 30 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.32f * a);
    gfx_cor((GfxRect){ x, y, w, 60 }, 0.5f, ar, ag, ab, a);
  } else tlNeutro((GfxRect){ x, y, w, 60 }, 30, 0.08f, 0.141f, 0.149f, 0.173f, a);
  txt_desenhar_alpha(l, x + 28, y + (60 - l.h) * 0.5f, (f ? 1.0f : 0.88f) * a);
  return w;
}

static void telemetria_desenharCorpo_(Uint32 agora);
// Cartao de tela quase cheia: ampliado so se ainda couber (escala.h).
void telemetria_desenhar(Uint32 agora) {
  ESCALA_SE_COUBER_INI(1260.0f, 600.0f);
  telemetria_desenharCorpo_(agora);
  ESCALA_SE_COUBER_FIM();
}
static void telemetria_desenharCorpo_(Uint32 agora) {
  static const char *const IC[3] = { "aj_activity", "aj_shield-check", "aj_clock" };
  static const char *const K[3] = { "Vai", "Não vai", "Quando" };
  static const char *const T[3] = { "O que o app fez e quanto demorou",
                                    "Senhas, chaves e o caminho dos endereços",
                                    "A sessão anterior ao abrir; esta, a cada minuto" };
  const char *texto = "Nas TVs Samsung ainda há travamentos que só aparecem no registro do app. "
                      "Se você deixar, o app manda esse registro sozinho: o da sessão anterior ao abrir "
                      "e o desta a cada minuto. Vai sem senhas nem chaves; tem só o que o app fez e quanto demorou. "
                      "Dá para desligar a qualquer hora em Ajustes › Sobre › Enviar registros sozinho.";
  float a = anim_suave(entrada), dy, x, y, w = 1260, cw = w - 116, th, h, cartW, cartH = 0;
  int i;
  (void)agora;
  if (entrada < 0.002f) return;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, (ajustes_vidro() ? 0.40f : 0.42f) * entrada);
  dy = (1.0f - a) * 36.0f;
  th = txt_bloco(TXT_DET_META2, i18n(texto), 0, 0, 0, 0, -4000, cw, 31.5f, 0, 6);
  cartW = (cw - 32) / 3.0f;
  for (i = 0; i < 3; i++) {
    float hh = 22 + 16 + 6 + txt_bloco(TXT_AJ_SUB, i18n(T[i]), 0, 0, 0, 0, -4000, cartW - 48 - 64, 26.6f, 0, 4) + 22;
    if (hh < 22 + 48 + 22) hh = 22 + 48 + 22;
    if (hh > cartH) cartH = hh;
  }
  h = 52 + 18 + 8 + 48 + 14 + th + 30 + cartH + 34 + 60 + 48;
  // Centrado na tela do fator ativo (escala.h); 330, 228 em 100%.
  x = (NV_TELA_W - w) * 0.5f;
  y = (gfx_escala() == 1.0f ? 228.0f : (NV_TELA_H - h) * 0.5f) + dy;
  { GfxRect r = { x, y, w, h };
    gfx_rect((GfxRect){ r.x - 20, r.y - 6, r.w + 40, r.h + 46 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, (ajustes_vidro() ? 0.36f : 0.45f) * a);
    if (ajustes_vidro()) {
      gfx_cor(r, 36.0f / h, 0.055f, 0.059f, 0.071f, 0.86f * a);
      gfx_luz_canto(r, 36.0f / h, r.w * 0.22f, -r.h * 0.40f, r.h * 0.62f, 1, 1, 1, 0.10f * a);
    } else gfx_cor(r, 36.0f / h, 0.082f, 0.086f, 0.102f, a); }
  x += 58; y += 52;
  tlCaps(TXT_MINI, "Samsung", 2.1f, x, y, 0.45f * a);
  y += 18 + 8;
  { TxtLinha t = txt_linha_corta(TXT_ILHA_TITULO, i18n("Ajude a deixar o app fluido na Samsung"), TL_TX, 255, cw);
    txt_desenhar_alpha(t, x, y, a); y += 48 + 14; }
  y += txt_bloco(TXT_DET_META2, i18n(texto), TL_TX, x, y, cw, 31.5f, 0.62f * a, 6) + 30;
  for (i = 0; i < 3; i++) {
    float cx = x + i * (cartW + 16);
    tlNeutro((GfxRect){ cx, y, cartW, cartH }, 22, 0.05f, 0.106f, 0.110f, 0.129f, a);
    tlNeutro((GfxRect){ cx + 24, y + 22, 48, 48 }, 15, 0.08f, 0.141f, 0.149f, 0.173f, a);
    gfx_icone((GfxRect){ cx + 36, y + 34, 24, 24 }, IC[i], 0.953f, 0.949f, 0.937f, 0.85f * a);
    tlCaps(TXT_AJ_CAPS13, K[i], 1.82f, cx + 24 + 48 + 16, y + 22, 0.42f * a);
    txt_bloco(TXT_AJ_SUB, i18n(T[i]), TL_TX, cx + 24 + 48 + 16, y + 22 + 16 + 6, cartW - 48 - 64, 26.6f, 0.88f * a, 4);
  }
  y += cartH + 34;
  { float bx = x;
    bx += tlBotao(i18n("Sim, pode mandar"), bx, y, 0, a) + 12;
    tlBotao(i18n("Agora não"), bx, y, 1, a);
    { TxtLinha l = tlT(TXT_ILHA_GENERO, i18n("pergunta uma vez por instalação"));
      txt_desenhar_alpha(l, x + cw - l.w, y + 30 - l.h * 0.5f, 0.45f * a); } }
}

#ifdef TELEMETRIA_TESTE
void telemetria_teste_abrir(void) { aberto = 1; entrada = 1.0f; foco = 0; }
#endif
