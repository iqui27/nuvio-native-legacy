// Real settings persistence and production-renderer pixel proof, using the Home fixture.
#include "../src/ajustes.c"
#include <assert.h>

static double luminance(SDL_Surface *s, int y) {
  double sum = 0;
  assert(s && s->w == 1920 && s->h == 1080);
  for (int x = 20; x < 52; x++) {
    Uint32 pixel = 0;
    Uint8 r, g, b;
    memcpy(&pixel, (char *)s->pixels + y * s->pitch + x * s->format->BytesPerPixel,
           s->format->BytesPerPixel);
    SDL_GetRGB(pixel, s->format, &r, &g, &b);
    sum += .2126 * r + .7152 * g + .0722 * b;
  }
  return sum / 32;
}

int fundo_din_escuro_testar(const char *dir, void (*render)(int, const char *)) {
  const char *out = getenv("NV_FUNDO_ESCURO_OUT");
  double samples[2][3];
  const int ys[] = { 100, 540, 1000 };
  char path[1000];
  assert(out);
  for (int mode = 0; mode <= 3; mode++) {
    valor[AJ_FUNDO_DIN] = mode;
    assert(gravar());
    valor[AJ_FUNDO_DIN] = 0;
    ajustes_dir(dir);
    printf("reload background %d: %d\n", mode, ajustes_fundo_din());
    fflush(stdout);
    assert(valor[AJ_FUNDO_DIN] == mode && ajustes_fundo_din() == mode);
  }
  assert(OPCOES[AJ_FUNDO_DIN].n == 4);
  assert(!strcmp(V_FUNDO_DIN[0], "Cinza") && !strcmp(V_FUNDO_DIN[1], "Cor") &&
         !strcmp(V_FUNDO_DIN[2], "Imersivo") && !strcmp(V_FUNDO_DIN[3], "Cinza escuro"));
  assert(strstr(ajudaOpcaoBase(AJ_FUNDO_DIN), "Cinza escuro"));
  valor[AJ_FUNDO_DIN] = -1; assert(ajustes_fundo_din() == 0);
  valor[AJ_FUNDO_DIN] = 4; assert(ajustes_fundo_din() == 0);
  valor[AJ_FUNDO_DIN] = 3;
  assert(limita(AJ_FUNDO_DIN, -1) == 3 && limita(AJ_FUNDO_DIN, 4) == 3);
  assert(somenteDesteAparelho(AJ_FUNDO_DIN) && !dePerfil(AJ_FUNDO_DIN));
  assert(ajustes_aplicar_blob("{\"fundoDinamicaLocal\":0}") >= 0);
  assert(ajustes_fundo_din() == 3);
  for (int i = 0; i < 2; i++) {
    valor[AJ_FUNDO_DIN] = i ? 3 : 0;
    valor[AJ_HOME_LAYOUT] = HOME_LAYOUT_DINAMICA;
    valor[AJ_HERO] = 1; // Hide the hero so the background remains visible.
    valor[AJ_ANIM] = 1;
    assert(gravar());
    valor[AJ_FUNDO_DIN] = 0;
    ajustes_dir(dir);
    assert(ajustes_fundo_din() == (i ? 3 : 0));
    snprintf(path, sizeof path, "%s/%s.bmp", out, i ? "cinza-escuro" : "cinza");
    render(180, path);
    SDL_Surface *s = SDL_LoadBMP(path);
    for (int j = 0; j < 3; j++) samples[i][j] = luminance(s, ys[j]);
    SDL_FreeSurface(s);
  }
  for (int j = 0; j < 3; j++) {
    printf("y=%d: gray %.2f, dark gray %.2f\n", ys[j], samples[0][j], samples[1][j]);
    assert(samples[1][j] < samples[0][j] - 1);
  }
  assert(samples[1][0] > samples[1][1] && samples[1][1] > samples[1][2]);
  assert(samples[1][0] >= 25 && samples[1][0] <= 29);
  assert(samples[1][2] >= 13 && samples[1][2] <= 18);
  puts("PASS dark gray: append-only picker, local save/reload, cloud isolation, darker gradient pixels");
  return 0;
}
