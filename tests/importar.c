// Import parsers (Stremio addons/library, AIOManager link body, host display). No network.
#include "importar.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define CK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

int main(void) {
  static AddonRemoto a[8];
  static ImpTitulo t[8];
  const char *addons =
    "{\"result\":{\"addons\":["
    "{\"transportUrl\":\"https://torrentio.strem.fun/realdebrid=SEGREDO/manifest.json\",\"manifest\":{\"id\":\"x\",\"name\":\"Torrentio\"}},"
    "{\"transportUrl\":\"stremio://cometfun.example/abc/manifest.json\",\"manifest\":{\"name\":\"Comet\"}},"
    "{\"transportUrl\":\"https://v3-cinemeta.strem.io/manifest.json\",\"manifest\":{\"name\":\"Cinemeta\"}},"
    "{\"transportUrl\":\"http://127.0.0.1:1/x.json\",\"manifest\":{\"name\":\"Bad\"}}]}}";
  int n = importar_stremio_addons(addons, a, 8);
  CK(n == 3);
  CK(!strcmp(a[0].nome, "Torrentio"));
  CK(!strncmp(a[1].url, "https://cometfun.example/abc/manifest.json", 20));
  CK(!strcmp(a[1].nome, "Comet"));
  CK(a[0].ativo == 1);
  CK(importar_stremio_addons("{\"result\":{}}", a, 8) == 0);
  CK(importar_stremio_addons("", a, 8) == 0);

  const char *lib =
    "{\"result\":[{\"_id\":\"tt0111161\",\"name\":\"Shawshank\",\"type\":\"movie\",\"removed\":false,\"temp\":false},"
    "{\"_id\":\"tt0903747\",\"name\":\"Breaking Bad\",\"type\":\"series\",\"removed\":false},"
    "{\"_id\":\"tt0000001\",\"name\":\"Gone\",\"type\":\"movie\",\"removed\":true},"
    "{\"_id\":\"tt0000002\",\"name\":\"Temp\",\"type\":\"movie\",\"temp\":true},"
    "{\"_id\":\"kitsu:1\",\"name\":\"Anime\",\"type\":\"series\"},"
    "{\"_id\":\"yt_id:abc\",\"name\":\"Clip\",\"type\":\"channel\"}]}";
  n = importar_stremio_biblioteca(lib, t, 8);
  CK(n == 2);
  CK(!strcmp(t[0].imdb, "tt0111161") && !strcmp(t[0].tipo, "movie"));
  CK(!strcmp(t[1].imdb, "tt0903747") && !strcmp(t[1].nome, "Breaking Bad"));
  CK(importar_stremio_biblioteca(lib, t, 1) == 1);

  // AIOManager: JSON object, JSON array of urls, plain lines, stremio:// scheme, junk.
  n = importar_aiom_addons("{\"addons\":[{\"transportUrl\":\"https://a.example/x/manifest.json\",\"name\":\"A\"},"
                           "{\"manifestUrl\":\"stremio://b.example/manifest.json\"}]}", a, 8);
  CK(n == 2);
  CK(!strcmp(a[0].nome, "A"));
  CK(!strcmp(a[1].url, "https://b.example/manifest.json"));
  n = importar_aiom_addons("[\"https://c.example/manifest.json\",\"https://d.example/m/manifest.json\"]", a, 8);
  CK(n == 2 && !strcmp(a[1].url, "https://d.example/m/manifest.json"));
  CK(a[0].nome[0] != 0);
  n = importar_aiom_addons("https://e.example/manifest.json\nstremio://f.example/manifest.json\nlixo\n\n", a, 8);
  CK(n == 2 && !strcmp(a[1].url, "https://f.example/manifest.json"));
  CK(importar_aiom_addons("<html>nope</html>", a, 8) == 0);
  CK(importar_aiom_addons("", a, 8) == 0);

  char h[64];
  importar_host("https://torrentio.strem.fun/realdebrid=SEGREDO/manifest.json", h, sizeof h);
  CK(!strcmp(h, "torrentio.strem.fun"));
  importar_host("stremio://x.example:8080/k/manifest.json", h, sizeof h);
  CK(!strcmp(h, "x.example"));
  importar_host("lixo", h, sizeof h);
  CK(h[0] == 0);

  printf(falhas ? "FAIL %d\n" : "OK\n", falhas);
  return falhas != 0;
}
