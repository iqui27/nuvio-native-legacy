#include "trailerio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define MOVIE "tt33764258"
#define SERIES "tt0903747"
#define MP4 "https://video.example.net/trailer.mp4?quality=high"
#define HLS "https://video.example.net/trailers/playlist.m3u8?lang=en"

static const char filme[] =
  "{\"meta\":{\"id\":\"" MOVIE "\",\"type\":\"movie\",\"links\":["
  "{\"provider\":\"Plex\",\"trailers\":\"" MP4 "\"},"
  "{\"provider\":\"Apple TV\",\"trailers\":\"" HLS "\"},"
  "{\"trailers\":\"" MP4 "\"},"
  "{\"trailers\":\"http://test.invalid/trailer.mp4\"},"
  "{\"trailers\":\"https://@test.invalid/a.mp4\"},"
  "{\"trailers\":\"https://test.invalid/player.html?q=.mp4\"},"
  "{\"irrelevant\":{\"trailers\":\"https://test.invalid/nested.mp4\"}}]}}";
static const char serie[] =
  "{\"meta\":{\"type\":\"series\",\"id\":\"" SERIES "\",\"links\":["
  "{\"provider\":\"MUBI\",\"trailers\":\"https://video.example.net/series.mp4\"}]}}";

int main(int argc, char **argv) {
  TrailerioLista lista;
  if (argc == 3) {
    char json[128 * 1024];
    const size_t n = fread(json, 1, sizeof json - 1, stdin);
    json[n] = 0;
    const int count = trailerio_ler(json, argv[1], argv[2], &lista);
    printf("Trailerio %s %s: %d sources\n", argv[2], argv[1], count);
    return count > 0 ? 0 : 1;
  }
  assert(trailerio_id(MOVIE, "movie"));
  assert(trailerio_id(SERIES, "series"));
  assert(!trailerio_id("tt0903747:1:1", "series"));
  assert(!trailerio_id("tt", "movie"));
  assert(trailerio_ler(filme, MOVIE, "movie", &lista) == 2);
  assert(!strcmp(lista.fontes[0].url, MP4) && !lista.fontes[0].hls);
  assert(!strcmp(lista.fontes[1].url, HLS) && lista.fontes[1].hls);
  assert(trailerio_ler(serie, SERIES, "series", &lista) == 1 && !lista.fontes[0].hls);
  assert(!trailerio_ler(filme, SERIES, "movie", &lista));
  assert(!trailerio_ler(filme, MOVIE, "series", &lista));
  assert(!trailerio_ler("{\"meta\":{\"type\":\"movie\",\"id\":\"" MOVIE "\",\"links\":[]}}",
                       MOVIE, "movie", &lista));
  assert(!trailerio_ler("{\"meta\":{\"type\":\"movie\",\"id\":\"" MOVIE "\",\"links\":[{}}",
                       MOVIE, "movie", &lista));
  puts("Trailerio: movie and series, HTTPS MP4/HLS, ordering and invalid responses passed.");
  return 0;
}
