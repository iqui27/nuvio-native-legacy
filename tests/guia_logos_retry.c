// Real texture-cache retry scheduling, without starting network/decode threads.
#include "../src/tex_cache.c"
#include <assert.h>

int guia_logos_retry_check(const char *name, const char *bad, const char *fallback,
                           GLuint texture, const char *(*draw)(void *), void *channel) {
  memset(itens, 0, sizeof itens);
  nMax = 8; filaIni = filaFim = decIni = decFim = 0;
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  assert(mtx && cond);
  for (int i = 0; i < 2; i++) {
    Item *item = &itens[i];
    const char *url = i ? fallback : bad;
    if (!url[0]) continue;
    snprintf(item->caminho, sizeof item->caminho, "%s", url);
    item->hash = hashCaminho(url);
    item->estado = PRONTO; item->tex = texture;
    item->w = item->fonteW = item->tetoUsado = 640; item->h = 320;
    item->corR = -2;
  }
  int ok = 1;
  for (int attempt = 1; attempt <= 4; attempt++) {
    itens[0].estado = FALHOU; itens[0].tex = 0;
    itens[0].falhas = attempt;
    itens[0].tentarEm = SDL_GetTicks() + 60000;
    filaIni = filaFim = 0;
    const char *selected = draw(channel);
    ok &= !strcmp(selected ? selected : "", fallback);
    ok &= itens[0].estado == FALHOU && filaIni == filaFim;
    // Advance the deadline instead of sleeping through 2/10/60-second backoffs.
    itens[0].tentarEm = SDL_GetTicks();
    draw(channel);
    if (attempt < 4) {
      ok &= itens[0].estado == PENDENTE && filaFim == 1 && fila[0] == 0;
      draw(channel);
      ok &= filaFim == 1; // Repeated visible draws must not duplicate retries.
    } else {
      ok &= itens[0].estado == FALHOU && filaIni == filaFim;
    }
  }
  // A completed retry restores the preferred logo without republishing channels.
  itens[0].estado = PRONTO; itens[0].tex = texture;
  const char *selected = draw(channel);
  ok &= selected && !strcmp(selected, bad);
  printf("%s retry: %s (backoff, queue, dedup, cap, recovery)\n", ok ? "PASS" : "FAIL", name);
  SDL_DestroyCond(cond); SDL_DestroyMutex(mtx); cond = NULL; mtx = NULL;
  return !ok;
}
