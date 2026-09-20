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
#include <stdio.h>
#include <stdlib.h>

#define TL_ARQ  "telemetria-perguntado.txt"
#define TL_W    1180.0f
#define TL_H     520.0f
#define TL_X    ((NV_TELA_W - TL_W) * 0.5f)
#define TL_Y    ((NV_TELA_H - TL_H) * 0.5f)
#define TL_PAD    64.0f
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

void telemetria_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { fechar(1); return; }
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

void telemetria_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, x, y;
  (void)agora;
  if (entrada < 0.002f) return;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  dy = (1.0f - a) * 36.0f;
  { GfxRect p = { TL_X, TL_Y + dy, TL_W, TL_H };
    gfx_cor(p, 0.030f, 0.075f, 0.078f, 0.088f, 0.98f * a); }
  x = TL_X + TL_PAD; y = TL_Y + dy + TL_PAD;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("SAMSUNG"), 150, 154, 165, 255);
    txt_desenhar_alpha(t, x, y, a * 0.92f); y += 34.0f; }
  { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n("Ajude a deixar o app fluido na Samsung"),
                                 246, 247, 252, 255, TL_W - TL_PAD * 2);
    txt_desenhar_alpha(t, x, y, a); y += 76.0f; }
  y += txt_bloco(TXT_BODY,
        i18n("Nas TVs Samsung ainda há travamentos que só aparecem no registro do app. "
             "Se você deixar, o app manda esse registro sozinho: o da sessão anterior ao abrir "
             "e o desta a cada minuto. Vai sem senhas nem chaves; tem só o que o app fez e quanto demorou. "
             "Dá para desligar a qualquer hora em Ajustes › Sobre › Enviar registros sozinho."),
        200, 203, 210, x, y, TL_W - TL_PAD * 2, 34.0f, a, 6);
  // Rodape: OK = sim, na cor de realce; Voltar = agora nao.
  { float by = TL_Y + dy + TL_H - TL_PAD - 64.0f, fr, fg, fb, ti;
    ti = ajustes_acento_tinta(&fr, &fg, &fb);
    { int c = (int)(ti * 255.0f + 0.5f);
      TxtLinha t = txt_linha(TXT_CALLOUT, i18n("OK   Sim, pode mandar"), c, c, c, 255);
      GfxRect b = { x, by, (float)t.w + 64.0f, 64.0f };
      gfx_cor(b, NV_RAIO_PILL, fr, fg, fb, a);
      txt_desenhar_alpha(t, b.x + 32.0f, b.y + (64.0f - (float)t.h) * 0.5f, a);
      { TxtLinha n = txt_linha(TXT_CALLOUT, i18n("Voltar   Agora não"), 200, 203, 210, 255);
        txt_desenhar_alpha(n, b.x + b.w + 40.0f, by + (64.0f - (float)n.h) * 0.5f, a * 0.9f); } } }
}
