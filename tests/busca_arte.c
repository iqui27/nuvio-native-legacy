// Offline regression: reuse the resolver's existing network stubs.
#define main reserva_tests
#include "artereserva.c"
#undef main
#include "artehero.h"
#include <assert.h>

static int failed(const char *url) {
  return url && (strstr(url, "images.metahub.space/logo/") ||
                 (strstr(url, "tmdblogo/") && respostasColisao));
}
int main(int argc, char **argv) {
  (void)argv;
  CatItem c = {0};
  char resolved[512], request[512];
  snprintf(c.imdb, sizeof c.imdb, "tmdb:1796809");
  snprintf(c.tipo, sizeof c.tipo, "movie");
  snprintf(c.poster, sizeof c.poster, "https://fixture/poster.jpg");
  snprintf(c.backdrop, sizeof c.backdrop, "%s", c.poster);
  snprintf(c.logo, sizeof c.logo, "https://images.metahub.space/logo/medium/tt49058294/img");
  artehero_qualidade(1);
  artehero_definir_falhou(failed);
  if (argc > 1) {
    const char *bg = artehero_url(&c);
    assert(bg && strstr(bg, "/tmdb/") && strcmp(bg, c.poster));
    return 0;
  }
  artehero_logo_sessao_iniciar(&c);
  const char *u = artehero_logo_sessao(&c);
  assert(u && strstr(u, "/tmdblogo/"));
  snprintf(request, sizeof request, "%s", u);
  resposta = "{\"backdrop_path\":\"/remain-bg.jpg\",\"images\":{\"logos\":[{\"file_path\":\"/remain-logo.png\",\"iso_639_1\":\"en\"}]}}";
  assert(arte_fonte_resolver(request, resolved, sizeof resolved) == 1);
  assert(strstr(resolved, "/w1280/remain-logo.png"));
  assert(strstr(ultimaUrl, "/movie/1796809?"));
  int before = pedidos;
  assert(arte_fonte_resolver(request, resolved, sizeof resolved) == 1 && pedidos == before);
  respostasColisao = 1;
  assert(artehero_logo_sessao(&c) == NULL); // renderer must use title text
  respostasColisao = 0;
  u = artehero_url(&c);
  assert(u && strstr(u, "/tmdb/") && strcmp(u, c.poster));
  assert(arte_fonte_resolver(u, resolved, sizeof resolved) == 1);
  assert(strstr(resolved, "/remain-bg.jpg"));
  c.backdrop[0] = 0;
  assert(strstr(artehero_url(&c), "/tmdb/"));
  snprintf(c.imdb, sizeof c.imdb, "tt49058294"); c.tmdb = 1796809;
  artehero_logo_sessao_iniciar(&c);
  assert(strstr(artehero_logo_sessao(&c), "/tmdblogo/"));
  c.logo[0] = 0;
  assert(strstr(artehero_logo_sessao(&c), "/tmdblogo/"));
  snprintf(c.imdb, sizeof c.imdb, "tmdb:1796810"); c.tmdb = 1796810;
  resposta = "{\"images\":{\"logos\":[]}}";
  artehero_logo_sessao_iniciar(&c);
  snprintf(request, sizeof request, "%s", artehero_logo_sessao(&c));
  assert(arte_fonte_resolver(request, resolved, sizeof resolved) == -1);
  before = pedidos;
  assert(arte_fonte_resolver(request, resolved, sizeof resolved) == -1 && pedidos == before);
  puts("PASS: metahub 404 -> TMDB logo; negative cache -> title; tmdb Search backdrop");
  return 0;
}
