// Politica de arte de tela cheia: cada regra com a medida que a justifica.
#include "artehero.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static CatItem item(const char *backdrop, const char *poster, const char *imdb) {
  static CatItem c;
  memset(&c, 0, sizeof c);
  snprintf(c.backdrop, sizeof c.backdrop, "%s", backdrop);
  snprintf(c.poster, sizeof c.poster, "%s", poster);
  snprintf(c.imdb, sizeof c.imdb, "%s", imdb);
  return c;
}

int main(void) {
  // TMDB w1280 (1280x720) sobe para original (3840x2160); o teto do cache
  // reduz para 1920 no decode.
  { CatItem c = item("https://image.tmdb.org/t/p/w1280/abc.jpg", "", "tt1");
    assert(!strcmp(artehero_url(&c),
                   "https://image.tmdb.org/t/p/original/abc.jpg")); }
  puts("ok  TMDB w1280 -> original");

  { CatItem c = item("https://image.tmdb.org/t/p/w780/abc.jpg", "", "");
    assert(!strcmp(artehero_url(&c),
                   "https://image.tmdb.org/t/p/original/abc.jpg")); }
  puts("ok  TMDB w780 -> original");

  // Trakt /medium/ (1280x720) -> /full/ (1920x1080). Medido com curl.
  { CatItem c = item("https://media.trakt.tv/images/shows/000/1/fanarts/medium/x.jpg.webp", "", "");
    assert(!strcmp(artehero_url(&c),
                   "https://media.trakt.tv/images/shows/000/1/fanarts/full/x.jpg.webp")); }
  puts("ok  Trakt medium -> full");

  // Metahub ja e 1920 e tem um tamanho so: passa intacta.
  { const char *u = "https://images.metahub.space/background/medium/tt6723592/img";
    CatItem c = item(u, "", "tt6723592");
    assert(!strcmp(artehero_url(&c), u)); }
  puts("ok  metahub passa intacta");

  // SEM FUNDO E COM ID: monta a url do metahub em vez de cair no cartaz — e a
  // diferenca entre "fundo de verdade" e "poster esticado".
  { CatItem c = item("", "https://p/poster.jpg", "tt0111161");
    assert(!strcmp(artehero_url(&c),
                   "https://images.metahub.space/background/medium/tt0111161/img")); }
  puts("ok  sem fundo, com tt: monta o metahub");

  // Sem fundo e SEM id do IMDb (item de addon com id proprio): o cartaz e a
  // ultima reserva, e o desenho o trata como arte CONTIDA, nao esticada.
  { CatItem c = item("", "https://p/poster.jpg", "kitsu:42");
    assert(!strcmp(artehero_url(&c), "https://p/poster.jpg")); }
  puts("ok  sem fundo e sem tt: cartaz");

  // Sem nada: NULL, e quem desenha mostra o estado neutro.
  { CatItem c = item("", "", "");
    assert(artehero_url(&c) == NULL);
    assert(artehero_url(NULL) == NULL); }
  puts("ok  sem arte nenhuma devolve NULL");

  // O CARD NAO MUDA: a fileira continua pedindo a url guardada, que foi
  // dimensionada para ela. Subir tudo para `original` seria decodificar
  // 8,3 MP para desenhar 212 px.
  { CatItem c = item("https://image.tmdb.org/t/p/w1280/abc.jpg", "", "tt1");
    assert(!strcmp(artehero_url_card(&c),
                   "https://image.tmdb.org/t/p/w1280/abc.jpg")); }
  puts("ok  a url do card fica como esta");

  // O ID DO EPISODIO VEM COLADO. "Continuar assistindo" de serie guarda
  // "tt26545992:1:1" em `imdb` — id, temporada e episodio na mesma string. O
  // metahub responde por id PURO; com a string inteira a url sai
  // ".../tt26545992:1:1/1/1/original.jpg" e o download volta vazio. Foi assim
  // que este teste nasceu: o log local mostrou as tres falhas.
  { CatItem c = item("https://b/back.jpg", "", "tt26545992:1:1");
    c.temporada = 1; c.episodio = 6;
    assert(!strcmp(artehero_url_episodio(&c),
                   "https://episodes.metahub.space/tt26545992/1/6/original.jpg")); }
  puts("ok  id de episodio: o :S:E nao entra na url");

  // Mesmo corte no fundo montado por id.
  { CatItem c = item("", "", "tt26545992:1:1");
    assert(!strcmp(artehero_url(&c),
                   "https://images.metahub.space/background/medium/tt26545992/img")); }
  puts("ok  fundo por id tambem corta o :S:E");

  // Filme (sem episodio) e serie sem id do IMDb nao montam url de still.
  { CatItem c = item("https://b/back.jpg", "", "tt0111161");
    assert(artehero_url_episodio(&c) == NULL); }
  { CatItem c = item("https://b/back.jpg", "", "kitsu:42");
    c.temporada = 1; c.episodio = 2;
    assert(artehero_url_episodio(&c) == NULL); }
  puts("ok  sem episodio ou sem tt: nao ha still a pedir");

  puts("artehero: tudo ok");
  return 0;
}
