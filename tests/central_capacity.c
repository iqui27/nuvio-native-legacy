// Regression: all catalog tiles, legacy saves, editor persistence and focus viewport.
#include "../src/central.c"
#include <assert.h>

int main(void) {
  CentralLista l = {0}, loaded;
  char buf[4096];
  int i;
  const char *old = "qualidade\ndolbyVision\ndolbyAtmos\nescolherFonteManual\n"
                    "trailerAuto\ncorDestaque\nesmaecerLocal\nvidroLocal\n";
  centrallista_ler(&loaded, old);
  assert(loaded.n == 8);
  centrallista_escrever(&loaded, buf, sizeof buf);
  assert(!strcmp(buf, old));
  for (i = 0; i < central_catalogo_n(); i++) {
    if (centrallista_alternar(&l, i) != 1) {
      fprintf(stderr, "FAIL: cannot add catalog tile %d (count=%d)\n", i + 1, l.n);
      return 1;
    }
  }
  centrallista_escrever(&l, buf, sizeof buf);
  centrallista_ler(&loaded, buf);
  assert(loaded.n == l.n && !memcmp(loaded.item, l.item, l.n * sizeof(int)));
  assert(centrallista_alternar(&l, -1) == -1);
  assert(centrallista_alternar(&l, central_catalogo_n()) == -1);
  ajustes_dir(getenv("NUVIO_DADOS"));
  dados_iniciar(getenv("NUVIO_DADOS"));
  central_teste_lista("-\n");
  central_teste_foco(-1, 1);
  {
    int offered[CENTRAL_MAX], count = ofertas(offered);
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN;
    for (i = 0; i < count; i++) {
      focoEd = i;
      e.key.keysym.sym = SDLK_RETURN;
      central_evento(&e);
    }
    assert(count > 8 && lista.n == count);
    {
      char name[40], *saved;
      nomeArquivo(name, sizeof name);
      saved = dados_ler(name);
      assert(saved);
      centrallista_ler(&loaded, saved);
      free(saved);
      assert(loaded.n == lista.n && !memcmp(loaded.item, lista.item, lista.n * sizeof(int)));
    }
  }
  // The production viewport scroll must retain focus, including Edit and live stats.
  editando = 0;
  central_teste_estatisticas(1);
  central_teste_tocando("Playing", "1080p");
  assert(altura(1) <= CC_TETO);
  {
    float h = altura(1) - CC_INFO_H - 6 - CC_TOC_H - 10 - CC_PAD - 6;
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_DOWN;
    foco = 0;
    quadroDt = 0.016f;
    rolaBotoes = 0;
    for (i = 0; i <= botoes(NULL, NULL); i++) {
      int frame;
      central_evento(&e);
      for (frame = 0; frame < 60; frame++) {
        float top = (foco / 3) * 114.0f;
        rolaBotoes = rolagemBotoes(h);
        assert(top - rolaBotoes >= -0.01f);
        assert(top + 104 - rolaBotoes <= h + 0.01f);
      }
    }
    assert(foco / CC_COLS == botoes(NULL, NULL) / CC_COLS);
    e.key.keysym.sym = SDLK_RIGHT;
    for (i = 0; i < CC_COLS; i++) central_evento(&e);
    assert(foco == botoes(NULL, NULL));
    e.key.keysym.sym = SDLK_UP;
    for (i = 0; i <= botoes(NULL, NULL); i++) {
      float top;
      central_evento(&e);
      top = (foco / 3) * 114.0f;
      rolaBotoes = rolagemBotoes(h);
      assert(top - rolaBotoes >= -0.01f && top + 104 - rolaBotoes <= h + 0.01f);
    }
    assert(foco < 3);
    // Visible focus in a short viewport; centering continues smoothly.
    foco = 9;
    rolaBotoes = 100;
    h = 400;
    {
      float first = rolagemBotoes(h);
      assert(first > 100 && first < 194);
      rolaBotoes = first;
      assert(rolagemBotoes(h) > first);
      anim_politica_reduzida = 1;
      assert(fabsf(rolagemBotoes(h) - 194) < 0.01f);
    }
  }
  puts("PASS: full catalog, legacy save, editor save/load, focus viewport, smooth/reduced scroll");
  return 0;
}
