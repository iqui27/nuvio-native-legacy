// #650/#687: the user-language (TMDB) trailer on the NATIVE targets
// (Samsung .tpk / LG ipk). The YouTube id cannot play inside the video plane,
// but it must still reach the platform launcher instead of being dropped.
// extras_trailer_abrir is the production function exercised here, compiled
// with NV_TPK like the .tpk build.
#include "../src/extras.h"
#include "../src/trailerfonte.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void extras_teste_trailer_audio(const char *cod);
int  extras_teste_trailers_ficha(const char *corpo, int serie, const char *idioma,
                                 char yt[][16], int max);
const char *desc_tmdb_idioma(void) { return "de-DE"; }

// The external launcher extras_trailer_abrir uses on the .tpk build: the
// host registers it through nv_tpk_youtube_registrar (Apps.cs -> AppControl).
// Here we register an observer to see what the C side hands the platform.
void nv_tpk_youtube_registrar(void (*f)(const char *url));
static char lancado[128];
static int  nLancado;
static void observar(const char *url) {
  if (url) snprintf(lancado, sizeof lancado, "%s", url);
  nLancado++;
}

static int falhas;
static void check(const char *nome, int ok) {
  printf("%s %s\n", ok ? "ok" : "FALHOU:", nome);
  if (!ok) falhas++;
}

int main(void) {
  // German UI. Fixture: one dubbed de trailer, one en trailer, one pt trailer.
  const char *corpo =
    "{\"original_language\":\"en\",\"videos\":{\"results\":["
    "{\"site\":\"YouTube\",\"type\":\"Trailer\",\"key\":\"englishT01\",\"iso_639_1\":\"en\",\"iso_3166_1\":\"US\",\"published_at\":\"2026-01-01\",\"name\":\"English Trailer\"},"
    "{\"site\":\"YouTube\",\"type\":\"Trailer\",\"key\":\"germanT001\",\"iso_639_1\":\"de\",\"iso_3166_1\":\"DE\",\"published_at\":\"2025-01-01\",\"name\":\"Deutscher Trailer\"},"
    "{\"site\":\"YouTube\",\"type\":\"Trailer\",\"key\":\"ptbrT0001\",\"iso_639_1\":\"pt\",\"iso_3166_1\":\"BR\",\"published_at\":\"2024-01-01\",\"name\":\"Trailer PT\"}]}}";
  char yt[EX_TRAILER_MAX][16];
  int n, i, germanIdx = -1;

  extras_teste_trailer_audio("");            // Automatico = UI language (de-DE)
  n = extras_teste_trailers_ficha(corpo, 0, "de-DE", yt, EX_TRAILER_MAX);
  check("TMDB ranking puts the German trailer first",
        n == 3 && !strcmp(yt[0], "germanT001"));
  check("English second, pt last",
        n == 3 && !strcmp(yt[1], "englishT01") && !strcmp(yt[2], "ptbrT0001"));
  for (i = 0; i < n; i++) if (!strcmp(yt[i], "germanT001")) germanIdx = i;
  check("German trailer flagged as preferred-language",
        germanIdx >= 0 && extras_trailer_idioma_preferido(germanIdx));

  // The user taps that localized trailer on the detail page. On the native
  // build the video plane cannot take a YouTube id, so the platform launcher
  // must get it — today the C drops it on the floor instead.
  nv_tpk_youtube_registrar(observar);
  lancado[0] = 0; nLancado = 0;
  extras_trailer_abrir(germanIdx);
  check("native target launches the localized YouTube trailer",
        nLancado == 1 && strstr(lancado, "germanT001"));

  // Same localized choice on a native source decision: the candidate pipeline
  // (youtubeIdiomaPreferido) still marks the match so callers can prefer it.
  {
    TrailerCandidatos c = {0};
    c.apple = "https://apple/english.m3u8"; c.appleRespondeu = 1;
    c.imdb = NULL; c.imdbRespondeu = 1;
    c.youtube = yt[germanIdx];
    c.youtubeRespondeu = 1;
    c.youtubeIdiomaPreferido = extras_trailer_idioma_preferido(germanIdx);
    check("native candidates flag the preferred-language YouTube",
          c.youtubeIdiomaPreferido == 1);
  }

  puts(falhas ? "trailer-lang-native: FALHOU" : "trailer-lang-native: tudo ok");
  return falhas ? 1 : 0;
}
