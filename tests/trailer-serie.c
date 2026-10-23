// #123: serie sem trailer. A ficha /tv/<id> nao pedia `videos`, o parse
// pulava serie, e com language=pt-BR sem include_video_language o TMDB so
// devolve video em portugues — trailer de serie costuma ser en ou sem idioma.
#include "../src/extras.h"
#include "../src/trailerfonte.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void extras_teste_url_ficha(char *dst, unsigned cap, int serie, long id,
                            const char *idioma, int trailers);
int extras_teste_trailers_ficha(const char *corpo, int serie, const char *idioma,
                                char yt[][16], int max);
int extras_teste_hero_serie(const char *corpo, const char *idioma, char *dst, unsigned cap);
void extras_teste_url_hero(char *dst, unsigned cap, int serie, long id, const char *idioma);

void extras_teste_trailer_audio(const char *cod);
void extras_teste_audio_pref(const char *v);
void extras_teste_idiomas_video(const char *idioma, const char *original, char *dst, unsigned cap);
void extras_teste_publicar_hero(const char *id, const char *corpo, const char *idioma);
const char *desc_tmdb_idioma(void) { return "en-US"; }

static int falhas;
static void check(const char *nome, int ok) {
  printf("%s %s\n", ok ? "ok" : "FALHOU:", nome);
  if (!ok) falhas++;
}

static char *ler(const char *c) {
  FILE *f = fopen(c, "rb");
  long n;
  char *s;
  if (!f) { perror(c); exit(2); }
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  s = malloc((size_t)n + 1);
  if (fread(s, 1, (size_t)n, f) != (size_t)n) exit(2);
  s[n] = 0; fclose(f);
  return s;
}

int main(void) {
  char url[512], yt[EX_TRAILER_MAX][16];
  char *corpo = ler("tests/fixtures/tmdb_tv_videos_ptbr.json");
  int n, i, temEp = 0;

  extras_teste_url_ficha(url, sizeof url, 1, 1399, "pt-BR", 1);
  check("ficha de serie pede videos", strstr(url, "append_to_response=") &&
                                      strstr(url, "videos") != NULL);
  check("ficha de serie pede pt,en,null",
        strstr(url, "include_video_language=pt,en,null") != NULL);
  extras_teste_url_ficha(url, sizeof url, 0, 603, "pt-BR", 1);
  check("ficha de filme pede pt,en,null",
        strstr(url, "videos") && strstr(url, "include_video_language=pt,en,null"));
  extras_teste_url_ficha(url, sizeof url, 0, 603, "en-US", 1);
  check("idioma en nao repete en", strstr(url, "include_video_language=en,null") != NULL);
  extras_teste_url_ficha(url, sizeof url, 1, 1399, "pt-BR", 0);
  check("trailers desligado nao pede videos",
        !strstr(url, "videos") && !strstr(url, "include_video_language"));

  n = extras_teste_trailers_ficha(corpo, 1, "pt-BR", yt, EX_TRAILER_MAX);
  printf("   serie: %d trailer(s):", n);
  for (i = 0; i < n; i++) printf(" %s", yt[i]);
  printf("\n");
  check("serie com videos en/null tem trailer", n > 0);
  check("primeiro e o da temporada mais recente",
        n > 0 && !strcmp(yt[0], "s3TrailerEN"));
  for (i = 0; i < n; i++)
    if (!strcmp(yt[i], "ep4Preview1")) temEp = 1;
  check("promo de episodio fica de fora", !temEp);
  check("so YouTube Trailer/Teaser (3 validos)", n == 3);

  // HERO: o mesmo /videos, a mesma escolha.
  extras_teste_url_hero(url, sizeof url, 1, 1399, "pt-BR");
  check("hero de serie pede /tv/<id>/videos com pt,en,null",
        strstr(url, "/tv/1399/videos?") &&
        strstr(url, "include_video_language=pt,en,null"));
  { char h[16];
    const char *bloco = strchr(strstr(corpo, "\"videos\""), '{');
    int nh = extras_teste_hero_serie(bloco, "pt-BR", h, sizeof h);
    check("hero de serie escolhe a temporada mais recente",
          nh == 3 && !strcmp(h, "s3TrailerEN")); }

  // AUDIO DO TRAILER: dublado pt-BR ganha do ingles e do pt-PT; Original fica no en.
  free(corpo);
  corpo = ler("tests/fixtures/tmdb_movie_videos_dub.json");
  n = extras_teste_trailers_ficha(corpo, 0, "en-US", yt, EX_TRAILER_MAX);
  check("auto com interface en: o ingles primeiro", n == 3 && !strcmp(yt[0], "trailerEN01"));
  extras_teste_trailer_audio("pt");
  n = extras_teste_trailers_ficha(corpo, 0, "en-US", yt, EX_TRAILER_MAX);
  check("audio Portugues: pt-BR dublado primeiro", n == 3 && !strcmp(yt[0], "trailerBR01"));
  check("depois o pt-PT, por ultimo o ingles", n == 3 && !strcmp(yt[1], "trailerPT01"));
  extras_teste_url_ficha(url, sizeof url, 0, 603, "en-US", 1);
  check("audio Portugues pede pt,en,null", strstr(url, "include_video_language=pt,en,null") != NULL);
  extras_teste_trailer_audio("~");
  extras_teste_url_ficha(url, sizeof url, 0, 603, "pt-BR", 1);
  check("Original pede en,null mesmo com interface pt-BR", strstr(url, "include_video_language=en,null") != NULL);
  n = extras_teste_trailers_ficha(corpo, 0, "pt-BR", yt, EX_TRAILER_MAX);
  check("Original: o ingles primeiro", n == 3 && !strcmp(yt[0], "trailerEN01"));
  extras_teste_trailer_audio("");
  n = extras_teste_trailers_ficha(corpo, 0, "pt-PT", yt, EX_TRAILER_MAX);
  check("auto com interface pt-PT: o pt-PT primeiro", n == 3 && !strcmp(yt[0], "trailerPT01"));

  free(corpo);
  corpo = strdup("{\"original_language\": \"ja\", \"videos\": {\"results\": [{\"site\": \"YouTube\", \"type\": \"Trailer\", \"key\": \"english001\", \"iso_639_1\": \"en\", \"iso_3166_1\": \"US\", \"published_at\": \"2026-01-01\"}, {\"site\": \"YouTube\", \"type\": \"Trailer\", \"key\": \"japanese01\", \"iso_639_1\": \"ja\", \"iso_3166_1\": \"JP\", \"published_at\": \"2024-01-01\"}, {\"site\": \"YouTube\", \"type\": \"Trailer\", \"key\": \"dubbedBR01\", \"iso_639_1\": \"pt\", \"iso_3166_1\": \"BR\", \"published_at\": \"2023-01-01\"}]}}");
  extras_teste_trailer_audio("~");
  n = extras_teste_trailers_ficha(corpo, 0, "pt-BR", yt, EX_TRAILER_MAX);
  extras_teste_idiomas_video("pt-BR", "ja", url, sizeof url);
  check("Original requests Japanese then English", !strcmp(url, "ja,en,null"));
  check("Original selects Japanese before English", n == 3 && !strcmp(yt[0], "japanese01"));
  extras_teste_trailer_audio("");
  n = extras_teste_trailers_ficha(corpo, 0, "fr-FR", yt, EX_TRAILER_MAX);
  extras_teste_idiomas_video("fr-FR", "ja", url, sizeof url);
  check("Auto requests UI, original, English", !strcmp(url, "fr,ja,en,null"));
  check("Auto falls back to Japanese before English", n == 3 && !strcmp(yt[0], "japanese01"));
  extras_teste_trailer_audio("pt");
  n = extras_teste_trailers_ficha(corpo, 1, "en-US", yt, EX_TRAILER_MAX);
  check("Series language wins before publication date", n == 3 && !strcmp(yt[0], "dubbedBR01"));
  extras_teste_trailer_audio("");
  free(corpo);
  // ONE RULE, real extras state + real trailerfonte: with Trailer audio set,
  // Apple answering first must wait for TMDB; the dubbed TMDB trailer then wins.
  {
    TrailerCandidatos c = {0};
    char hero[16];
    const char *u = NULL;
    int q = 0;
    TrailerDecisao d;
    corpo = ler("tests/fixtures/tmdb_movie_videos_dub.json");
    trailerfonte_definir_imdb_tizen(1);
    c.apple = "https://apple/english.m3u8"; c.appleRespondeu = 1;
    c.imdb = "https://imdb/english.mp4"; c.imdbRespondeu = 1;
    c.youtubeRespondeu = 1;

    extras_teste_trailer_audio("pt");
    c.youtubeIdiomaPreferido = extras_hero_trailer_idioma_preferido("tt0000205");
    d = trailerfonte_escolher_destaque(TRF_AUTO, 1, &c, &u, &q);
    check("home: Apple answered, TMDB pending -> wait", c.youtubeIdiomaPreferido < 0 && d == TRF_ESPERA);
    extras_teste_publicar_hero("tt0000205", corpo, "en-US");
    c.youtubeIdiomaPreferido = extras_hero_trailer_idioma_preferido("tt0000205");
    c.youtube = extras_hero_trailer_obter("tt0000205", hero, sizeof hero) ? hero : NULL;
    d = trailerfonte_escolher_destaque(TRF_AUTO, 1, &c, &u, &q);
    check("home: TMDB answers later -> dubbed pt-BR plays",
          d == TRF_ABRE && q == TRF_YOUTUBE && !strcmp(u, "trailerBR01"));
    d = trailerfonte_escolher(TRF_APPLE, 1, &c, &u, &q);
    check("home: fixed Apple source is respected", q == TRF_APPLE);
    check("dubbed YouTube fails (153/timeout) -> Apple, then IMDb",
          trailerfonte_depois_dublado(TRF_AUTO, 1, TRF_YOUTUBE) == TRF_APPLE &&
          trailerfonte_depois_dublado(TRF_AUTO, 1, TRF_APPLE) == TRF_IMDB &&
          trailerfonte_depois_dublado(TRF_AUTO, 1, TRF_IMDB) == 0);
    extras_teste_trailer_audio("~");
    check("Trailer audio change: cached pt-BR hero ranking is not reused",
          extras_hero_trailer_idioma_preferido("tt0000205") != 1 &&
          !extras_hero_trailer_obter("tt0000205", hero, sizeof hero));
    extras_teste_trailer_audio("pt");

    extras_teste_trailers_ficha(corpo, 0, "en-US", yt, EX_TRAILER_MAX);
    c.youtube = extras_trailer_yt(0);
    c.youtubeIdiomaPreferido = extras_trailer_idioma_preferido(0);
    d = trailerfonte_escolher_cheia(TRF_AUTO, 1, 1, &c, &u, &q);
    check("detail full screen: dubbed pt-BR plays", q == TRF_YOUTUBE && !strcmp(u, "trailerBR01"));
    d = trailerfonte_escolher(TRF_AUTO, 1, &c, &u, &q);
    check("detail autoplay: dubbed pt-BR plays", q == TRF_YOUTUBE && !strcmp(u, "trailerBR01"));

    extras_teste_trailer_audio("~");   // Original = English title: Apple as before
    extras_teste_publicar_hero("tt0000206", corpo, "en-US");
    c.youtubeIdiomaPreferido = extras_hero_trailer_idioma_preferido("tt0000206");
    d = trailerfonte_escolher_destaque(TRF_AUTO, 1, &c, &u, &q);
    check("Original English title keeps Apple", c.youtubeIdiomaPreferido == 0 && q == TRF_APPLE);
    c.youtubeIdiomaPreferido = -1;
    d = trailerfonte_escolher(TRF_AUTO, 0, &c, &u, &q);
    check("native (no YouTube): pending language never blocks", d == TRF_ABRE && q == TRF_APPLE);
    extras_teste_trailer_audio("");
    free(corpo);
  }

  // Test: automatic trailer audio follows playback audio preference, not UI language
  {
    char idiomas[64];
    extras_teste_trailer_audio("");       // Automatic mode
    extras_teste_audio_pref("pt");        // Playback audio preference is Portuguese
    extras_teste_idiomas_video("en-US", "", idiomas, sizeof idiomas);
    check("automatic: playback preference (pt) over UI (en-US)",
          strstr(idiomas, "pt,en,null") != NULL);
    
    extras_teste_audio_pref("");          // Clear playback preference
    extras_teste_idiomas_video("en-US", "", idiomas, sizeof idiomas);
    check("automatic: fallback to UI language when no playback preference",
          strstr(idiomas, "en,null") != NULL);
  }
  puts(falhas ? "trailer-serie: FALHOU" : "trailer-serie: tudo ok");
  return falhas ? 1 : 0;
}
