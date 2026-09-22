// Politica de arte de tela cheia: cada regra com a medida que a justifica.
#include "artehero.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Duble: "toda arte ja falhou", para exercitar a reserva.
static int falhouSempre(const char *u) { (void)u; return 1; }
static int falhouLogoAntiga(const char *u) {
  return u && strstr(u, "logo-old.png") != NULL;
}

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

  // O hero pode escolher uma origem real já preservada no item: catálogo /
  // Cinemeta, IMDb / Metahub, TMDB ou Trakt. Nenhuma delas usa fundo genérico.
  { CatItem c = item("https://images.metahub.space/background/medium/tt1/img", "", "tt1");
    snprintf(c.backdropCatalogo, sizeof c.backdropCatalogo, "%s", "https://catalogo/bg.jpg");
    snprintf(c.backdropTmdb, sizeof c.backdropTmdb, "%s", "https://image.tmdb.org/t/p/w1280/tmdb.jpg");
    snprintf(c.backdropTrakt, sizeof c.backdropTrakt, "%s", "https://media.trakt.tv/fanart.jpg");
    assert(!strcmp(artehero_url_fonte(&c, 1), "https://catalogo/bg.jpg"));
    assert(!strcmp(artehero_url_fonte(&c, 2), "https://images.metahub.space/background/medium/tt1/img"));
    assert(!strcmp(artehero_url_fonte(&c, 3), "https://image.tmdb.org/t/p/w1280/tmdb.jpg"));
    assert(!strcmp(artehero_url_fonte(&c, 4), "https://media.trakt.tv/fanart.jpg")); }
  puts("ok  fontes reais do hero: catalogo, IMDb, TMDB e Trakt");

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

  // A seleção visual prende a primeira URL canônica do mesmo IMDb/tipo. O
  // detalhe pode receber w500 e o player original depois, mas os três caminhos
  // continuam com w1280. O stub abaixo conta mudanças de chave; ele prova o
  // contrato do seletor, não substitui a telemetria do downloader real.
  { CatItem a = item("", "", "tt-logo-1");
    int gets = 0; char ultima[512] = "";
    snprintf(a.tipo, sizeof a.tipo, "%s", "movie");
    snprintf(a.logo, sizeof a.logo,
             "%s", "https://image.tmdb.org/t/p/w500/logo-a.png");
    { const char *home = artehero_logo_sessao_observar(&a);
      assert(home && !strcmp(home, "https://image.tmdb.org/t/p/w1280/logo-a.png"));
      if (strcmp(ultima, home)) { snprintf(ultima, sizeof ultima, "%s", home); gets++; } }
    { CatItem enriquecido = a;
      snprintf(enriquecido.logo, sizeof enriquecido.logo,
               "%s", "https://image.tmdb.org/t/p/original/logo-a.png");
      const char *detail = artehero_logo_sessao(&enriquecido);
      const char *player = artehero_logo_sessao(&enriquecido);
      assert(detail && player && !strcmp(detail, player) && !strcmp(detail, ultima));
      if (strcmp(ultima, player)) { snprintf(ultima, sizeof ultima, "%s", player); gets++; }
    }
    assert(gets == 1);
    { CatItem b = item("", "", "tt-logo-2");
      snprintf(b.tipo, sizeof b.tipo, "%s", "movie");
      snprintf(b.logo, sizeof b.logo,
               "%s", "https://images.metahub.space/logo/medium/tt-logo-2/img");
      artehero_logo_sessao_iniciar(&b);
      assert(!strcmp(artehero_logo_sessao(&b), b.logo));
      assert(strcmp(artehero_logo_sessao(&a), b.logo));
    }
  }
  puts("ok  logo: sessão compartilha chave; identidade impede vazamento");

  // Contratos de transição: sem logo na abertura, a primeira boa entra; uma
  // URL enriquecida diferente não troca a arte já escolhida; identidade/tipo
  // diferentes não herdam a seleção; e falha definitiva libera a nova URL.
  { CatItem vazio = item("", "", "tt-logo-empty");
    snprintf(vazio.tipo, sizeof vazio.tipo, "%s", "movie");
    artehero_logo_sessao_iniciar(&vazio);
    assert(artehero_logo_sessao(&vazio) == NULL);
    snprintf(vazio.logo, sizeof vazio.logo,
             "%s", "https://image.tmdb.org/t/p/w500/logo-first.png");
    assert(!strcmp(artehero_logo_sessao(&vazio),
                   "https://image.tmdb.org/t/p/w1280/logo-first.png"));
    { CatItem enriquecida = vazio;
      snprintf(enriquecida.logo, sizeof enriquecida.logo,
               "%s", "https://image.tmdb.org/t/p/original/logo-second.png");
      assert(!strcmp(artehero_logo_sessao(&enriquecida),
                     "https://image.tmdb.org/t/p/w1280/logo-first.png")); }
    { CatItem outroTipo = vazio;
      snprintf(outroTipo.tipo, sizeof outroTipo.tipo, "%s", "series");
      snprintf(outroTipo.logo, sizeof outroTipo.logo,
               "%s", "https://image.tmdb.org/t/p/w500/logo-series.png");
      assert(!strcmp(artehero_logo_sessao(&outroTipo),
                     "https://image.tmdb.org/t/p/w1280/logo-series.png")); }
  }
  { CatItem falha = item("", "", "tt-logo-fail");
    snprintf(falha.tipo, sizeof falha.tipo, "%s", "movie");
    snprintf(falha.logo, sizeof falha.logo,
             "%s", "https://image.tmdb.org/t/p/w500/logo-old.png");
    artehero_logo_sessao_iniciar(&falha);
    artehero_definir_falhou(falhouLogoAntiga);
    snprintf(falha.logo, sizeof falha.logo,
             "%s", "https://image.tmdb.org/t/p/w500/logo-new.png");
    assert(!strcmp(artehero_logo_sessao(&falha),
                   "https://image.tmdb.org/t/p/w1280/logo-new.png"));
    artehero_definir_falhou(NULL);
  }
  puts("ok  logo: vazio/enriquecimento/identidade/falha definitiva");

  puts("artehero: tudo ok");
  return 0;
}
