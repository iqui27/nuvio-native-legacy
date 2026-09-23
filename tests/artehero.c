// Politica de arte de tela cheia: cada regra com a medida que a justifica.
#include "artehero.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Duble: "toda arte ja falhou", para exercitar a reserva.
__attribute__((unused)) static int falhouSempre(const char *u) { (void)u; return 1; }
__attribute__((unused)) static int falhouTmdbVirtual(const char *u) {
  return u && strstr(u, "nuvio.invalid/arte/tmdb") != NULL;   // tmdb e tmdbalt
}
// Duble de artereserva: "estas duas urls sao o mesmo arquivo".
static const char *igualA, *igualB;
__attribute__((unused)) static int igualDuble(const char *a, const char *b) {
  return igualA && a && b && ((!strcmp(a, igualA) && !strcmp(b, igualB)) ||
                              (!strcmp(a, igualB) && !strcmp(b, igualA)));
}
__attribute__((unused)) static int falhouLogoAntiga(const char *u) {
  return u && strstr(u, "logo-old.png") != NULL;
}

__attribute__((unused)) static int resolvidaDuble(const char *u, char *s, size_t n) {
  if (!u || !strstr(u, "arte/tmdbalt/")) return 0;
  snprintf(s, n, "https://image.tmdb.org/t/p/original/abc.jpg");
  return 1;
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
  // SAMSUNG: NUNCA `original` NEM /full/ no fundo, em fonte nenhuma, nem na
  // Alta (fundoOriginal(), OOM do registro 1450). tests/artehero.sh roda este
  // mesmo arquivo uma segunda vez com -D__EMSCRIPTEN__.
#ifdef __EMSCRIPTEN__
  { CatItem c = item("https://image.tmdb.org/t/p/w1280/abc.jpg", "", "tt7");
    int f, d;
    snprintf(c.backdropTrakt, sizeof c.backdropTrakt, "%s",
             "https://media.trakt.tv/images/movies/000/1/fanarts/medium/x.jpg.webp");
    artehero_qualidade(2);
    for (f = 0; f <= ARTEHERO_TRAKT; f++)
      for (d = 0; d <= 1; d++) {
        const char *u1 = artehero_url_fonte(&c, f);
        const char *u2 = artehero_url_destaque(&c, f, d);
        assert(!u1 || (!strstr(u1, "original") && !strstr(u1, "/full/")));
        assert(u2 && !strstr(u2, "original") && !strstr(u2, "/full/"));
      }
    c.backdropTrakt[0] = 0;
    assert(!strcmp(artehero_url_fonte(&c, ARTEHERO_TRAKT),
                   "https://nuvio.invalid/arte/trakt/medium/tt7"));
    c = item("https://images.metahub.space/background/medium/tt9/img", "", "tt9");
    assert(!strcmp(artehero_url_fonte(&c, ARTEHERO_TMDB),
                   "https://nuvio.invalid/arte/tmdb/w1280/tt9"));
    artehero_qualidade(1); }
  puts("ok  Tizen: nenhuma fonte pede original/full, nem na Alta");
  puts("artehero (tizen): tudo ok");
  return 0;
#endif

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

#ifndef __EMSCRIPTEN__
  // O AJUSTE QUE NAO FAZIA NADA (22/09). Item do Cinemeta como ele chega de
  // verdade (curl no top de filmes): `background` = metahub pelo id, e so. No
  // catalogo gravado do Mac (~/.nuvio/catalogo-rede.bin, 285 titulos) 222
  // eram assim, e para os 222 as cinco escolhas davam a MESMA url: TMDB e
  // Trakt devolviam NULL e o chamador caia na automatica.
  { const char *mh = "https://images.metahub.space/background/medium/tt0111161/img";
    CatItem c = item(mh, "https://p/poster.jpg", "tt0111161");
    snprintf(c.backdropCatalogo, sizeof c.backdropCatalogo, "%s", mh);
    const char *tm = artehero_url_fonte(&c, ARTEHERO_TMDB);
    const char *tr = artehero_url_fonte(&c, ARTEHERO_TRAKT);
    assert(tm && strcmp(tm, mh));
    assert(tr && strcmp(tr, mh));
    assert(!strcmp(tm, "https://nuvio.invalid/arte/tmdb/w1280/tt0111161"));
    assert(!strcmp(tr, "https://nuvio.invalid/arte/trakt/medium/tt0111161"));
    // O CARD SEGUE A FONTE (antes lia item->backdrop cru): mesma foto do
    // destaque com "outra arte" desligado.
    assert(!strcmp(artehero_url_card_fonte(&c, ARTEHERO_TMDB, 0), tm));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_TMDB, 0), tm));
    assert(!strcmp(artehero_url_card_fonte(&c, ARTEHERO_AUTO, 0), mh));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_AUTO, 0), mh));
    // "OUTRA ARTE" LIGADO: card no catalogo, destaque noutra foto — e com
    // TMDB o OUTRO backdrop (23/09: o padrao do TMDB e o fundo do metahub
    // costumam ser a mesma foto). Sem ano no item nao ha busca na Apple.
    { const char *alt = "https://nuvio.invalid/arte/tmdbalt/w1280/tt0111161";
    assert(!strcmp(artehero_url_card_fonte(&c, ARTEHERO_TMDB, 1), mh));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_TMDB, 1), alt));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_AUTO, 1), alt));
    // A fonte escolhida repete o card (metahub = o proprio Cinemeta): pula
    // para a primeira outra.
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_METAHUB, 1), alt));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_CATALOGO, 1), alt));
    // Com ano, a Apple vem primeiro (busca pelo titulo, codificado).
    snprintf(c.meta, sizeof c.meta, "1994 · 142 min");
    snprintf(c.titulo, sizeof c.titulo, "The Shawshank Redemption");
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_AUTO, 1),
                   "https://nuvio.invalid/arte/apple/1920/tt0111161/m/1994/The%20Shawshank%20Redemption"));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_APPLE, 0),
                   "https://nuvio.invalid/arte/apple/1920/tt0111161/m/1994/The%20Shawshank%20Redemption"));
    // Com o id do TMDB no item (moviedb_id do Cinemeta) a virtual o carrega
    // e o resolvedor pula o /find.
    c.tmdb = 278; snprintf(c.tipo, sizeof c.tipo, "movie");
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_TMDB, 1),
                   "https://nuvio.invalid/arte/tmdbalt/w1280/tt0111161/m278"));
    assert(!strcmp(artehero_url_fonte(&c, ARTEHERO_TMDB),
                   "https://nuvio.invalid/arte/tmdb/w1280/tt0111161/m278"));
    // A MESMA IMAGEM POR BYTES: se o outro do TMDB baixou igual ao card, a
    // proxima diferente da ordem (Apple).
    igualA = "https://nuvio.invalid/arte/tmdbalt/w1280/tt0111161/m278"; igualB = mh;
    artehero_definir_igual(igualDuble);
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_TMDB, 1),
                   "https://nuvio.invalid/arte/apple/1920/tt0111161/m/1994/The%20Shawshank%20Redemption"));
    artehero_definir_igual(NULL);
    // fanart.tv so com chave.
    assert(artehero_url_fonte(&c, ARTEHERO_FANART) == NULL);
    artehero_fanart_disponivel(1);
    assert(!strcmp(artehero_url_fonte(&c, ARTEHERO_FANART),
                   "https://nuvio.invalid/arte/fanart/full/tt0111161/m/278"));
    artehero_fanart_disponivel(0);
    // Anime so para anime: nao e.
    assert(artehero_url_fonte(&c, ARTEHERO_ANIME) == NULL);
    c.meta[0] = 0; c.titulo[0] = 0; c.tmdb = 0; c.tipo[0] = 0; }
    // Com o TMDB ja FALHADO no cache, o Trakt; nunca preso num 404.
    artehero_definir_falhou(falhouTmdbVirtual);
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_AUTO, 1), tr));
    assert(!strcmp(artehero_url_card_fonte(&c, ARTEHERO_TMDB, 0), mh));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_TMDB, 0), mh));
    artehero_definir_falhou(falhouSempre);
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_AUTO, 1), mh));
    artehero_definir_falhou(NULL); }
  puts("ok  fonte do fundo: TMDB/Trakt virtuais pelo id; card e destaque seguem");

  // Card com fundo do TMDB: o `original` do MESMO arquivo nao conta como
  // "outra arte" na Alta — seria a mesma foto maior.
  { CatItem c = item("https://image.tmdb.org/t/p/w1280/abc.jpg", "", "tt7");
    artehero_qualidade(2);
    // Com outra arte o TMDB e o OUTRO backdrop, por definicao outro arquivo.
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_TMDB, 1),
                   "https://nuvio.invalid/arte/tmdbalt/original/tt7"));
    // ...mas se ele RESOLVEU para a foto do card (abc.jpg noutro tamanho), e
    // a mesma foto: vale a proxima, Trakt.
    artehero_definir_resolvida(resolvidaDuble);
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_TMDB, 1),
                   "https://nuvio.invalid/arte/trakt/full/tt7"));
    artehero_definir_resolvida(NULL);
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_TMDB, 0),
                   "https://image.tmdb.org/t/p/original/abc.jpg"));
    assert(!strcmp(artehero_url_fonte(&c, ARTEHERO_TMDB),
                   "https://image.tmdb.org/t/p/original/abc.jpg"));
    // O card nao sobe: continua o w1280 que o catalogo dimensionou.
    assert(!strcmp(artehero_url_card_fonte(&c, ARTEHERO_TMDB, 0),
                   "https://image.tmdb.org/t/p/w1280/abc.jpg"));
    assert(!strcmp(artehero_url_card_fonte(&c, ARTEHERO_TRAKT, 0),
                   "https://nuvio.invalid/arte/trakt/medium/tt7"));
    artehero_qualidade(1); }
  puts("ok  outra arte: o original da propria foto do card nao conta");

  // Sem id do IMDb nao ha como montar metahub nem virtual: fica a do card.
  { CatItem c = item("https://addon/bg.jpg", "", "cs:channel:globo");
    assert(artehero_url_fonte(&c, ARTEHERO_TMDB) == NULL);
    assert(artehero_url_fonte(&c, ARTEHERO_ANIME) == NULL);
    assert(!strcmp(artehero_url_card_fonte(&c, ARTEHERO_TMDB, 0), "https://addon/bg.jpg"));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_AUTO, 1), "https://addon/bg.jpg")); }
  puts("ok  sem tt: nenhuma fonte inventada, fica a do card");

  // ANIME: id do addon de anime (kitsu:) vira a fonte Kitsu/AniList mesmo sem
  // tt; um tt so e anime com genero Animacao E pais Japao.
  { CatItem c = item("https://addon/bg.jpg", "", "kitsu:7442");
    snprintf(c.titulo, sizeof c.titulo, "Attack on Titan");
    snprintf(c.meta, sizeof c.meta, "2013");
    snprintf(c.tipo, sizeof c.tipo, "series");
    assert(!strcmp(artehero_url_fonte(&c, ARTEHERO_ANIME),
                   "https://nuvio.invalid/arte/anime/large/kitsu:7442/s/2013/Attack%20on%20Titan"));
    assert(!strcmp(artehero_url_destaque(&c, ARTEHERO_AUTO, 1),
                   "https://nuvio.invalid/arte/anime/large/kitsu:7442/s/2013/Attack%20on%20Titan")); }
  { CatItem c = item("https://images.metahub.space/background/medium/tt2560140/img", "", "tt2560140");
    snprintf(c.titulo, sizeof c.titulo, "Attack on Titan");
    snprintf(c.genero, sizeof c.genero, "Programa de TV  ·  Animação  ·  Ação");
    assert(artehero_url_fonte(&c, ARTEHERO_ANIME) == NULL);     // sem pais: Pixar tambem e Animacao
    snprintf(c.pais, sizeof c.pais, "Japan");
    assert(artehero_url_fonte(&c, ARTEHERO_ANIME) != NULL); }
  puts("ok  anime: id de addon de anime, ou Animacao + Japao");
#endif


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
