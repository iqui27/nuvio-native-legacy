// CARTAZ DO ADDON LIDO NA RAIZ DO ITEM (#200, mnguzd, LG G5, 1.6.4).
//
// O relato: com a URL de poster personalizada no AIOMetadata (BetterPosters,
// PostersPlus), alguns titulos (Breaking Bad, Ted Lasso) apareciam com o
// cartaz do TMDB no idioma da TV e SEM a nota; os outros com o cartaz do
// servico, com a nota. No app oficial, nao.
//
// A CAUSA (lida no codigo do AIOMetadata, addon/lib/getCache.ts): o meta que
// sai do CACHE dele e remontado com o bloco "basic" primeiro — e o basic leva
// `_providerArt: { poster, background, logo }`, a arte do provedor (TMDB, no
// idioma configurado). O `poster` da raiz, ja trocado pela URL do servico de
// cartaz, entra DEPOIS. deMeta lia "poster" com js_texto, que acha a PRIMEIRA
// chave com esse nome em qualquer nivel: o cartaz do _providerArt. Titulo que
// o AIOMetadata montou na hora (poster antes do _providerArt) saia certo — dai
// o "nao da para prever quais".
//
// O teste usa a forma reduzida das duas ordens e confere que o cartaz, o fundo
// e o logo sao os da raiz; e que um item sem cartaz na raiz ainda aproveita o
// aninhado (o comportamento antigo), em vez de sumir da fileira.
//
//   bash tests/posterprov.sh   (roda este tambem)
#include "tex_cache.h"
#define main detalheanime_main
#include "detalheanime.c"
#undef main

int main(void) {
  CatItem d;
  // 1) Ordem do meta remontado do cache do AIOMetadata: _providerArt antes.
  { const char *js =
      "{\"id\":\"tt0903747\",\"name\":\"Во все тяжкие\",\"type\":\"series\","
      "\"imdb_id\":\"tt0903747\",\"posterShape\":\"poster\",\"_hasPoster\":true,"
      "\"_metaProvider\":\"tmdb\","
      "\"_providerArt\":{\"poster\":\"https://image.tmdb.org/t/p/w500/ru.jpg\","
      "\"background\":\"https://image.tmdb.org/t/p/original/ru-bg.jpg\","
      "\"logo\":\"https://image.tmdb.org/t/p/original/ru-logo.png\",\"anime\":false},"
      "\"poster\":\"https://btttr.cc/K/imdb/poster-default/tt0903747.jpg?lang=ru\","
      "\"background\":\"https://cdn.exemplo/bg/tt0903747.jpg\","
      "\"logo\":\"https://cdn.exemplo/logo/tt0903747.png\"}";
    assert(deMeta(js, js + strlen(js), "series", &d));
    assert(!strcmp(d.poster, "https://btttr.cc/K/imdb/poster-default/tt0903747.jpg?lang=ru"));
    assert(!strcmp(d.backdrop, "https://cdn.exemplo/bg/tt0903747.jpg"));
    assert(!strcmp(d.logo, "https://cdn.exemplo/logo/tt0903747.png"));
    assert(!strcmp(d.imdb, "tt0903747")); }
  puts("ok  _providerArt antes do poster: vale o poster da raiz (servico de cartaz)");

  // 2) Ordem do meta montado na hora: ja saia certo e continua.
  { const char *js =
      "{\"id\":\"tt10986410\",\"name\":\"Тед Лассо\",\"type\":\"series\","
      "\"poster\":\"https://btttr.cc/K/imdb/poster-default/tt10986410.jpg\","
      "\"_providerArt\":{\"poster\":\"https://image.tmdb.org/t/p/w500/ru2.jpg\"}}";
    assert(deMeta(js, js + strlen(js), "series", &d));
    assert(!strcmp(d.poster, "https://btttr.cc/K/imdb/poster-default/tt10986410.jpg")); }
  puts("ok  poster antes do _providerArt: sem mudanca");

  // 3) Sem cartaz na raiz (null): o aninhado ainda serve, o item nao some.
  { const char *js =
      "{\"id\":\"tt1\",\"name\":\"X\",\"_providerArt\":{\"poster\":\"https://p/aninhado.jpg\"},"
      "\"poster\":null}";
    assert(deMeta(js, js + strlen(js), "movie", &d));
    assert(!strcmp(d.poster, "https://p/aninhado.jpg")); }
  puts("ok  sem poster na raiz: usa o aninhado, como antes");

  // 4) A ficha (/meta) passa pelo mesmo deMeta com o ponteiro em "meta":{...}.
  { const char *js =
      "{\"meta\":{\"id\":\"tt0903747\",\"name\":\"BB\","
      "\"_providerArt\":{\"poster\":\"https://image.tmdb.org/t/p/w500/ru.jpg\"},"
      "\"poster\":\"https://btttr.cc/K/p.jpg\"}}";
    const char *m = strstr(js, "\"meta\"");
    assert(deMeta(m, NULL, "series", &d));
    assert(!strcmp(d.poster, "https://btttr.cc/K/p.jpg")); }
  puts("ok  /meta do addon: mesma regra");

  // 5) URL LONGA DO POSTERSPLUS (#200, segunda parte). O modelo da pessoa,
  // preenchido pelo AIOMetadata com as chaves do TMDB (32) e do MDBList,
  // passa de 700 bytes; o `logo_language=ru` fica depois do byte 512. Com o
  // poster[512] de antes a URL era cortada calada ali e o PostersPlus, sem
  // idioma, desenhava o cartaz em ingles. As chaves aqui sao de mentira.
  { char url[900], js[1400];
    snprintf(url, sizeof url,
      "https://postersplus.slokker.cc/poster?tmdb_id=1396&imdb_id=tt0903747"
      "&stremio_id=tt0903747&type=series&primary_client=stremio_tv_nuvio"
      "&tmdb_key=%s&mdblist_key=%s&top_gradient=off&bottom_gradient=low"
      "&vignette_poster_color_bottom=true&vignette_color_blur=0.50"
      "&vignette_color_local=false&vignette_color_style=muted&sash_mode=notch"
      "&fallback_to_imdb=true&tmdb_rating_source=direct&score_color_mode=0"
      "&movie_weights=imdb%%3A1.00&tv_weights=imdb%%3A1.00&use_original_art=true"
      "&logo_language=ru&logo_priority=native%%2Cenglish%%2Coriginal%%2Cneutral%%2Ctext"
      "&fallback_bg_style=photoreal&meta_order=rating%%2Cyear%%2Cgenre"
      "&sash_badge_size_w=1.40&sash_badge_size_h=1.20&badge_display_mode=0",
      "00000000000000000000000000000000", "xxxxxxxxxxxxxxxxxxxxxxxx");
    assert(strlen(url) > 700);
    assert(strstr(url, "logo_language=ru") - url > 512);
    snprintf(js, sizeof js,
      "{\"id\":\"tt0903747\",\"name\":\"BB\",\"type\":\"series\","
      "\"_providerArt\":{\"poster\":\"https://image.tmdb.org/t/p/w500/ru.jpg\"},"
      "\"poster\":\"%s\"}", url);
    assert(deMeta(js, js + strlen(js), "series", &d));
    assert(!strcmp(d.poster, url));
    assert(strstr(d.poster, "&logo_language=ru&"));
    // O cache de textura guarda o caminho com o mesmo teto: cortar la seria
    // o mesmo defeito um passo adiante.
    assert(NV_TEX_URL_MAX >= sizeof d.poster); }
  puts("ok  URL de ~720 bytes do PostersPlus chega inteira (logo_language=ru)");

  // #433: 2047 cabe; 2048/mais nao pode virar prefixo valido.
  { char url[2201], json[2500];
    for (size_t n = 2047; n <= 2200; n += n == 2047 ? 1 : 152) {
      memset(url, 'a', n); memcpy(url, "https://posters.invalid/", 24); url[n] = 0;
      snprintf(json, sizeof json, "{\"name\":\"Matchbox\",\"poster\":\"%s\"}", url);
      assert(deMeta(json, json + strlen(json), "movie", &d));
      assert(n == 2047 ? !strcmp(d.poster, url) : !d.poster[0]);
    }
  }
  puts("ok  limite do cartaz: 2047 inteiro, 2048/2200 descartados");
  // Pictorium addon/meta picks use the same extensionless endpoint as templates.
  { const char *js =
      "{\"meta\":{\"id\":\"tt0903747\",\"name\":\"BB\","
      "\"_providerArt\":{\"poster\":\"https://image.tmdb.org/t/p/w500/default.jpg\"},"
      "\"poster\":\"https://pictorium.elfhosted.com/api/poster/series/1396?u=test-space&live=1\"}}";
    assert(deMeta(strstr(js, "\"meta\""), NULL, "series", &d));
    assert(!strcmp(d.poster,
      "https://pictorium.elfhosted.com/api/poster/series/1396?u=test-space&live=1")); }
  puts("ok  Pictorium per-title addon poster: root extensionless URL preserved");
  puts("posteraddon: tudo ok");
  return 0;
}
