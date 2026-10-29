// Real screens with the live theme clock; no network or device required.
#ifndef NV_TESTS_VIDRO_C9_CACHE_H
#define NV_TESTS_VIDRO_C9_CACHE_H
#include "home.h"
#include "addons.h"
#include "fundo.h"

static void vc_frame(int screen, int frame) {
  Uint32 now = 30000u + (Uint32)frame * 16u;
  corviva_quadro(1.0f / 60, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
  txt_novo_quadro(); tex_bombear(16);
  // Match main.c: updates may warm invisible text before draw accounting
  // resets. Counting those warmups closes the backdrop before it is painted.
  if (screen == 0) {
    home_atualizar(1.0f / 60, now);
    menu_pilula_mostrar(1); menu_atualizar(1.0f / 60, now);
  } else if (screen == 1) detail_atualizar(1.0f / 60, now);
  else ajustes_atualizar(1.0f / 60, now);
  gfx_novo_quadro(); tex_novo_quadro();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  if (screen == 0) { home_desenhar(now); menu_desenhar(now); }
  else if (screen == 1) detail_desenhar(now);
  else ajustes_desenhar(now);
  ilha_desenhar(now);
}
static int vc_run(void) {
  int failures = 0;
  const char *names[] = {"home", "detail", "settings"};
  ajustes_shot_valor("fundoLocal", FUNDO_FROST);
  ajustes_shot_valor("relogioTelaLocal", 1);
  assert(ajustes_vidro() && ajustes_vidro_fosco());
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
  assert(addons_definir_lista(&addon, 1));
  montarCatalogo();
  menu_iniciar(); ilha_relogio_visivel(1);
  // Warm artwork before measuring; no texture decode should enter static work.
  for (int i = 0; i < 160; i++) { vc_frame(0, i); SDL_Delay(2); }
  for (int screen = 0; screen < 3; screen++) {
    if (screen == 0) menu_abrir();
    if (screen == 1) { menu_fechar(); abrir(1, 0, 0, 0); }
    if (screen == 2) { ajustes_iniciar(); ajustes_shot_valor("fundoLocal", FUNDO_FROST); }
    for (int i = 0; i < 180; i++) { vc_frame(screen, 200 + i); SDL_Delay(16); }
    for (int phase = 0; phase < 2; phase++) {
      int captures = 0, passes = 0, sources = 0;
      double fill = 0, copy = 0;
      int baked = gfx_n_fundo_assados;
      if (phase) {
        SDL_Event e = {.type = SDL_KEYDOWN};
        e.key.keysym.sym = screen == 2 ? SDLK_DOWN : SDLK_RIGHT;
        if (!screen) menu_evento(&e);
        else if (screen == 1) detail_evento(&e);
        else ajustes_evento(&e);
        // Let the accent-focus transition settle before measuring cache reuse.
        for (int i = 0; i < 180; i++) { vc_frame(screen, 380 + i); SDL_Delay(16); }
      }
      for (int i = 0; i < 30; i++) {
        // Text LRU retires labels while focus/clock/diagnostics change. This
        // texture never enters a backdrop: retiring it must not flush glass.
        GLuint label;
        glGenTextures(1, &label); glBindTexture(GL_TEXTURE_2D, label);
        unsigned char pixel[4] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        gfx_tex_esquecer(label); glDeleteTextures(1, &label);
        vc_frame(screen, 380 + phase * 180 + i);
        captures += gfx_vidro_capturas; passes += gfx_vidro_passadas;
        sources += gfx_vidro_com_fonte;
        fill += gfx_vidro_fill; copy += gfx_vidro_copia;
      }
      int ok = captures <= (phase ? 1 : 0) && passes <= (phase ? 3 : 0) && sources > 0 &&
               gfx_n_fundo_assados == baked;
      printf("%s: %s %s frames=30 captures=%d passes=%d blur-pixels=%.0f copied-pixels=%.0f bakes=%d sources=%d\n",
             ok ? "PASS" : "FAIL", names[screen], phase ? "focus" : "static", captures, passes,
             fill * 1920 * 1080, copy * 1920 * 1080, gfx_n_fundo_assados - baked, sources);
      failures += !ok;
      if (!phase) gravar(names[screen]);
    }
  }
  // Frost and blurred artwork already bake on change. Keep this invariant,
  // including glass disabled (no consumer may trigger a fallback capture).
  for (int mode = FUNDO_BORRADA; mode <= FUNDO_FROST; mode++) {
    for (int glass = 1; glass >= 0; glass--) {
      ajustes_definir_vidro(glass);
      for (int i = 0; i < 5; i++) {
        gfx_novo_quadro(); gfx_sem_recorte();
        fundo_desenhar_modo(mode, (GfxRect){0, 0, 1920, 1080}, 0, itens[1].backdrop, 1);
        gfx_vidro_ou_solido((GfxRect){100, 100, 400, 200}, .1f, .8f, 1, .1f, .1f, .1f, 1);
      }
      int baked = gfx_n_fundo_assados, captures = 0, passes = 0;
      double blur = 0;
      for (int i = 0; i < 30; i++) {
        gfx_novo_quadro(); gfx_sem_recorte();
        fundo_desenhar_modo(mode, (GfxRect){0, 0, 1920, 1080}, 0, itens[1].backdrop, 1);
        gfx_vidro_ou_solido((GfxRect){100, 100, 400, 200}, .1f, .8f, 1, .1f, .1f, .1f, 1);
        captures += gfx_vidro_capturas; passes += gfx_vidro_passadas;
        blur += gfx_fill_modo[GFX_BLUR];
      }
      int ok = !captures && !passes && !blur && baked == gfx_n_fundo_assados;
      printf("%s: background=%d glass=%d captures=%d passes=%d art-blur-fill=%.3f bakes=%d\n",
             ok ? "PASS" : "FAIL", mode, glass, captures, passes, blur, gfx_n_fundo_assados - baked);
      failures += !ok;
    }
  }
  ajustes_definir_vidro(1);
  // A texture actually used by the backdrop must still invalidate, including
  // reuse of the same GL name and explicit in-place upload notifications.
  GLuint source;
  glGenTextures(1, &source); glBindTexture(GL_TEXTURE_2D, source);
  unsigned char pixel[4] = {160, 110, 80, 255};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  for (int i = 0; i < 4; i++) {
    if (i == 2) gfx_tex_esquecer(source);
    if (i == 3) gfx_vidro_sujar();
    gfx_novo_quadro(); gfx_sem_recorte();
    gfx_rect((GfxRect){0, 0, 1920, 1080}, source, GFX_ARTE, 0, 0, 0, 0, 1, 1, 1, 1);
    gfx_vidro((GfxRect){100, 100, 400, 200}, .1f, GFX_VIDRO_PAINEL, .1f, .8f, 1);
    int ok = i == 1 ? gfx_vidro_capturas == 0 : gfx_vidro_capturas == 1;
    printf("%s: source invalidation frame=%d captures=%d passes=%d\n",
           ok ? "PASS" : "FAIL", i, gfx_vidro_capturas, gfx_vidro_passadas);
    failures += !ok;
  }
  gfx_tex_esquecer(source); glDeleteTextures(1, &source);
  failures += glGetError() != GL_NO_ERROR;
  return failures ? 1 : 0;
}
#endif
