// Parser de links externos (src/deeplink.c, docs/deeplink.md).
#include "deeplink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHA %s:%d %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

static DeepLink ok(const char *u) {
  DeepLink d;
  if (!deeplink_parse(u, &d)) { printf("FALHA nao aceitou: %s\n", u); falhas++; }
  return d;
}
static void ruim(const char *u) {
  DeepLink d;
  if (deeplink_parse(u, &d)) { printf("FALHA aceitou lixo: %.80s\n", u); falhas++; }
  CHECK(d.tipo == DL_NENHUM);
}

int main(void) {
  DeepLink d;
  d = ok("nuvio-legacy://meta/movie/tt0111161");
  CHECK(d.tipo == DL_TITULO && !strcmp(d.imdb, "tt0111161") && !d.serie && !d.tmdb);
  d = ok("nuvio-legacy://meta/series/tt0944947/");
  CHECK(d.tipo == DL_TITULO && d.serie);
  d = ok("NUVIO-LEGACY://meta/series/603");
  CHECK(d.tipo == DL_TITULO && d.tmdb == 603 && !d.imdb[0] && d.serie);
  d = ok("nuvio-legacy://meta/movie/tmdb:550?x=1#y");
  CHECK(d.tmdb == 550);
  d = ok("nuvio-legacy://play/movie/tt0111161");
  CHECK(d.tipo == DL_TOCAR && d.episodio == 0);
  d = ok("nuvio-legacy://play/series/tt0944947/2/5");
  CHECK(d.tipo == DL_TOCAR && d.temporada == 2 && d.episodio == 5);
  // Explicit season zero must not silently play the default/resume episode.
  ruim("nuvio-legacy://play/series/tt0944947/0/1");
  ruim("nuvio-legacy://play/series/tt0944947/00/1");
  ruim("nuvio-legacy://play/movie/tt0111161/0/1");
  d = ok("nuvio-legacy://search?q=the+matrix%20reloaded");
  CHECK(d.tipo == DL_BUSCA && !strcmp(d.consulta, "the matrix reloaded"));
  d = ok("nuvio-legacy://search?a=1&q=caf%C3%A9%0A%09x");
  CHECK(!strcmp(d.consulta, "caf\xC3\xA9  x"));
  d = ok("stremio:///detail/movie/tt0111161");
  CHECK(d.tipo == DL_TITULO && !strcmp(d.imdb, "tt0111161"));
  d = ok("stremio://detail/series/tt0944947/tt0944947:1:2");
  CHECK(d.tipo == DL_TITULO && d.serie);
  d = ok("https://www.imdb.com/title/tt0111161/?ref_=x");
  CHECK(d.tipo == DL_TITULO && !strcmp(d.imdb, "tt0111161"));
  d = ok("https://m.imdb.com/title/tt0111161");
  CHECK(!strcmp(d.imdb, "tt0111161"));
  d = ok("https://imdb.com/title/tt0111161/reviews");
  CHECK(!strcmp(d.imdb, "tt0111161"));

  ruim(NULL);
  ruim("");
  ruim("nuvio-legacy://");
  ruim("nuvio-legacy://meta");
  ruim("nuvio-legacy://meta/movie");
  ruim("nuvio-legacy://meta/anime/tt0111161");
  ruim("nuvio-legacy://meta/movie/tt");
  ruim("nuvio-legacy://meta/movie/tt12ab");
  ruim("nuvio-legacy://meta/movie/tt0111161;rm -rf");
  ruim("nuvio-legacy://meta/movie/../tt0111161");
  ruim("nuvio-legacy://meta/movie//tt0111161");
  ruim("nuvio-legacy://meta/movie/0");
  ruim("nuvio-legacy://meta/movie/-5");
  ruim("nuvio-legacy://meta/movie/99999999999");
  ruim("nuvio-legacy://meta/movie/tt0111161/extra");
  ruim("nuvio-legacy://play/movie/tt0111161/1/2");
  ruim("nuvio-legacy://play/series/tt0944947/2");
  ruim("nuvio-legacy://play/series/tt0944947/x/1");
  ruim("nuvio-legacy://play/series/tt0944947/1/0");
  ruim("nuvio-legacy://play/series/tt0944947/99999/1");
  ruim("nuvio-legacy://search");
  ruim("nuvio-legacy://search?q=");
  ruim("nuvio-legacy://search?q=++");
  ruim("nuvio-legacy://search?xq=a");
  ruim("nuvio-legacy://search/extra?q=a");
  ruim("stremio:///meta/movie/tt0111161");
  ruim("stremio:///detail/movie");
  ruim("https://www.imdb.com/name/nm0000209");
  ruim("https://www.imdb.com/title/");
  ruim("https://www.imdb.com/title/nm0000209");
  ruim("https://evil.com/title/tt0111161");
  ruim("http://www.imdb.com/title/tt0111161");
  ruim("file:///etc/passwd");
  ruim("javascript:alert(1)");

  // busca: %XX malformado, limite de tamanho, corte sem meia sequencia UTF-8
  d = ok("nuvio-legacy://search?q=100%+%zz%2");
  CHECK(!strcmp(d.consulta, "100% %zz%2"));
  d = ok("nuvio-legacy://search?q=%zz");
  CHECK(!strcmp(d.consulta, "%zz"));
  { char big[900]; size_t i;
    strcpy(big, "nuvio-legacy://search?q=");
    for (i = strlen(big); i < 520; i++) big[i] = 'a';
    big[i] = 0;
    ruim(big);                                   // URL passa de DL_URL_MAX
    strcpy(big, "nuvio-legacy://search?q=");
    for (i = strlen(big); i < 24 + 94; i++) big[i] = 'a';
    strcpy(big + i, "%C3%A9%C3%A9%C3%A9"); d = ok(big);   // 94 'a' + e-agudo cortado ao meio
    CHECK(strlen(d.consulta) < sizeof d.consulta);
    CHECK((unsigned char)d.consulta[strlen(d.consulta) - 1] < 0x80 ||
          ((unsigned char)d.consulta[strlen(d.consulta) - 2] == 0xC3));
  }

  // postar/pegar: invalido nao enfileira, o mais novo vence, entrega uma vez
  deeplink_postar("lixo://x");
  CHECK(!deeplink_pegar(&d));
  ruim("https://www.imdb.com");
  ruim("https://m.imdb.com");
  ruim("https://imdb.com");
  deeplink_postar("https://www.imdb.com");
  CHECK(!deeplink_pegar(&d));
  deeplink_postar("nuvio-legacy://meta/movie/tt0000001");
  deeplink_postar("nuvio-legacy://meta/movie/tt0000002");
  CHECK(deeplink_pegar(&d) && !strcmp(d.imdb, "tt0000002"));
  CHECK(!deeplink_pegar(&d));

  printf(falhas ? "deeplink: %d falha(s)\n" : "deeplink: ok\n", falhas);
  return falhas != 0;
}
