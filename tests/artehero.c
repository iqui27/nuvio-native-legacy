// Politica de arte de tela cheia: cada regra com a medida que a justifica.
#include "artehero.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Duble: "toda arte ja falhou", para exercitar a reserva.
static int falhouSempre(const char *u) { (void)u; return 1; }

static CatItem item(const char *backdrop, const char *poster, const char *imdb) {
  static CatItem c;
  memset(&c, 0, sizeof c);
  snprintf(c.backdrop, sizeof c.backdrop, "%s", backdrop);
  snprintf(c.poster, sizeof c.poster, "%s", poster);
  snprintf(c.imdb, sizeof c.imdb, "%s", imdb);
  return c;
}

int main(void) {
  // A MESMA IMAGEM DO CARD (19/09): com fundo guardado, o heroi e esse
  // arquivo — nem metahub por id, nem `original` — no padrao e na baixa.
  { CatItem c = item("https://image.tmdb.org/t/p/w1280/abc.jpg", "", "tt1");
    assert(!strcmp(artehero_url(&c), "https://image.tmdb.org/t/p/w1280/abc.jpg")); }
  puts("ok  com id do IMDb e fundo do TMDB: o proprio w1280");
  { CatItem c = item("https://image.tmdb.org/t/p/w1280/abc.jpg", "", "kitsu:9");
    assert(!strcmp(artehero_url(&c), "https://image.tmdb.org/t/p/w1280/abc.jpg")); }
  puts("ok  sem tt: o proprio w1280");
  { CatItem c = item("https://image.tmdb.org/t/p/w1280/abc.jpg", "", "tt1");
    artehero_definir_falhou(falhouSempre);
    assert(!strcmp(artehero_url(&c), "https://image.tmdb.org/t/p/w1280/abc.jpg"));
    artehero_definir_falhou(NULL); }
  puts("ok  com fundo, metahub nem entra em jogo");
  { CatItem c = item("https://image.tmdb.org/t/p/w780/abc.jpg", "", "");
    assert(!strcmp(artehero_url(&c), "https://image.tmdb.org/t/p/w780/abc.jpg")); }
  puts("ok  w780 fica w780 no padrao");
  // NA ALTA o TMDB sobe para `original` e o Trakt para /full/ — com o decode
  // escalado o 3840 sai em 1920 sem custar inteiro.
  artehero_qualidade(2);
  { CatItem c = item("https://image.tmdb.org/t/p/w1280/abc.jpg", "", "tt1");
    assert(!strcmp(artehero_url(&c), "https://image.tmdb.org/t/p/original/abc.jpg")); }
  { CatItem c = item("https://image.tmdb.org/t/p/w780/abc.jpg", "", "");
    assert(!strcmp(artehero_url(&c), "https://image.tmdb.org/t/p/original/abc.jpg")); }
  { CatItem c = item("https://media.trakt.tv/images/shows/000/1/fanarts/medium/x.jpg.webp", "", "");
    assert(!strcmp(artehero_url(&c),
                   "https://media.trakt.tv/images/shows/000/1/fanarts/full/x.jpg.webp")); }
  artehero_qualidade(1);
  puts("ok  alta: TMDB -> original, Trakt medium -> full");
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
                   "https://episodes.metahub.space/tt26545992/1/6/w1280.jpg")); }
  puts("ok  id de episodio: o :S:E nao entra na url");

  // O TAMANHO DO STILL SEGUE O NIVEL. Medido na C9: `original` chega a
  // 3840x2160 em varias series e custa 1,6 a 2,0 s de decodificacao; w1280
  // custa da ordem de 150 ms. So a Alta paga a espera.
  { CatItem c = item("", "", "tt1");
    c.temporada = 2; c.episodio = 3;
    artehero_qualidade(2);
    assert(!strcmp(artehero_url_episodio(&c),
                   "https://episodes.metahub.space/tt1/2/3/original.jpg"));
    artehero_qualidade(0);
    assert(!strcmp(artehero_url_episodio(&c),
                   "https://episodes.metahub.space/tt1/2/3/w780.jpg"));
    artehero_qualidade(1);
    assert(!strcmp(artehero_url_episodio(&c),
                   "https://episodes.metahub.space/tt1/2/3/w1280.jpg")); }
  puts("ok  still: w780 na baixa, w1280 no padrao, original na alta");

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

  // LOGO. O Cinemeta manda em `original`, que no TMDB e 4127x2000 para um
  // desenho de no maximo 1000 px — 1,3 a 1,5 s de decodificacao, medidos na C9.
  // Conferido com curl em 17/09: w500 e 499x242, w780 e 780x378, w1280 e
  // 1279x620 e `original` e 4127x2000. A escada publicada do TMDB para logo
  // para em w500, mas o CDN responde w780 e w1280 com o tamanho pedido.
  { const char *o = "https://image.tmdb.org/t/p/original/xSj9.png";
    artehero_qualidade(1);
    assert(!strcmp(artehero_url_logo(o),
                   "https://image.tmdb.org/t/p/w1280/xSj9.png"));
    artehero_qualidade(0);
    assert(!strcmp(artehero_url_logo(o),
                   "https://image.tmdb.org/t/p/w500/xSj9.png"));
    // Na alta a interface e desenhada em 2x: o logo chega a 2000 px e so
    // `original` tem pixel para isso.
    artehero_qualidade(2);
    assert(!strcmp(artehero_url_logo(o),
                   "https://image.tmdb.org/t/p/original/xSj9.png"));
    artehero_qualidade(1); }
  puts("ok  logo: w500 na baixa, w1280 no padrao, original na alta");

  // Um logo que ja veio pequeno tambem e normalizado — o catalogo do pacote e
  // o extras.c usam w92/w185 e nao ha motivo de o detalhe herdar w92 num
  // desenho de 1000 px.
  { assert(!strcmp(artehero_url_logo("https://image.tmdb.org/t/p/w92/a.png"),
                   "https://image.tmdb.org/t/p/w1280/a.png")); }
  puts("ok  logo: tamanho antigo na url e trocado, nao concatenado");

  // QUEM NAO E TMDB PASSA INTACTO: metahub, arquivo do pacote e url vazia.
  { const char *m = "https://images.metahub.space/logo/medium/tt1/img";
    assert(artehero_url_logo(m) == m);
    const char *l = "/media/developer/apps/.../arte/logo/0007.png";
    assert(artehero_url_logo(l) == l);
    assert(artehero_url_logo("") != NULL);
    assert(artehero_url_logo(NULL) == NULL); }
  puts("ok  logo: so o caminho do TMDB e reescrito");

  puts("artehero: tudo ok");
  return 0;
}
