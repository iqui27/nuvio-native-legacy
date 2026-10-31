// Real offline screens. Freeze the test clock only during settled pixel comparisons.
// Production animations stay enabled; focus updates run before each comparison.
#include "biblioteca.h"
static void fs_frame(int screen, int n, int update) {
  Uint32 now = update ? SDL_GetTicks() : (Uint32)n;
  if (update) corviva_quadro(1.f/60, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
  txt_novo_quadro(); tex_bombear(16);
  if (update) {
    if (!screen) { home_atualizar(1.f/60, now); menu_atualizar(1.f/60, now); }
    else if (screen == 1) detail_atualizar(1.f/60, now);
    else biblioteca_atualizar(1.f/60, now);
  }
  gfx_novo_quadro(); tex_novo_quadro(); gfx_sem_recorte();
  glClearColor(.05f, .05f, .05f, 1); glClear(GL_COLOR_BUFFER_BIT);
  if (!screen) { home_desenhar(now); menu_desenhar(now); }
  else if (screen == 1) detail_desenhar(now);
  else biblioteca_desenhar(now);
}
static int fs_run(void) {
  const char *names[] = {"home", "detail", "library"};
  size_t bytes = 1920u * 1080 * 4;
  unsigned char *prev = malloc(bytes), *cur = malloc(bytes);
  assert(prev && cur);
  int failures = 0;
  ajustes_shot_valor("fundoLocal", FUNDO_FROST);
  ajustes_shot_valor("relogioTelaLocal", 0);
  ajustes_shot_valor("vidroQualidadeLocal", 2);
  assert(home_iniciar("deploy/app/art")); menu_iniciar();
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
  assert(addons_definir_lista(&addon, 1)); montarCatalogo();
  cat_definir_na_lista(0, 1); cat_definir_na_lista(1, 1);
  for (int screen = 0; screen < 3; screen++) {
    if (!screen) menu_abrir();
    else if (screen == 1) { menu_fechar(); abrir(1, 0, 0, 0); }
    else assert(biblioteca_iniciar());
    for (int glass = 1; glass >= 0; glass--) {
      ajustes_definir_vidro(glass);
      for (int level = 0; level < 3; level++) {
        gfx_definir_efeitos_leves(level == 1);
        gfx_definir_efeitos_minimos(level == 2);
        for (int n = 0; n < 240; n++) { fs_frame(screen, n, 1); SDL_Delay(8); }
        for (int phase = 0; phase < 2; phase++) {
          if (phase) {
            SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = SDLK_RIGHT;
            if (!screen) menu_evento(&e);
            else if (screen == 1) detail_evento(&e);
            else biblioteca_evento(&e);
            for (int n = 180; n < 360; n++) { fs_frame(screen, n, 1); SDL_Delay(8); }
          }
          unsigned maxdelta = 0; size_t changed = 0;
          Uint32 settled = SDL_GetTicks();
          for (int n = 0; n < 12; n++) {
            if (n & 1) gfx_vidro_sujar();
            fs_frame(screen, settled, 0);
            glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, cur);
            if (n) for (size_t i = 0; i < bytes; i++) {
              unsigned d = abs((int)cur[i] - prev[i]);
              if (d > maxdelta) maxdelta = d;
              changed += d > 1;
            }
            memcpy(prev, cur, bytes);
            if (glass && !level && n < 4) {
              char name[100]; snprintf(name, sizeof name, "%s-%s-%d", names[screen], phase ? "focus" : "static", n);
              gravar(name);
            }
          }
          // Allow at most one hundred channel samples to round by two LSBs
          // across eleven full-HD comparisons; larger changes are flicker.
          int ok = maxdelta <= 2 && changed <= 100;
          printf("%s: %s glass=%d level=%d focus=%d frames=12 maxdelta=%u changed=%zu\n",
                 ok ? "PASS" : "FAIL", names[screen], glass, level, phase, maxdelta, changed);
          failures += !ok;
        }
      }
    }
  }
  free(prev); free(cur);
  return failures ? 1 : 0;
}
