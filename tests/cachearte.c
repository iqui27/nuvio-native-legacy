#include "cachearte.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(x, msg) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } } while (0)

static void cache_name(const char *url, char *dst, size_t cap) {
  unsigned long h = 2166136261UL;
  const unsigned char *p = (const unsigned char *)url;
  for (; *p; p++) { h ^= *p; h *= 16777619UL; }
  snprintf(dst, cap, "%08lx.jpg", h);
}

int main(void) {
  char dir[] = "/tmp/nuvio-cachearte-XXXXXX", path[512], profilePath[512], name[64], ignored[512];
  const char *home = "https://images.example/home.jpg?sig=private";
  const char *profile = "https://images.example/profile.jpg?sig=private";
  NvCacheArteStats stats;
  FILE *f;

  CHECK(mkdtemp(dir) != NULL, "temporary cache directory");
  cachearte_nativo_configurar_diretorio(dir);
  cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_HOME, home,
                         NV_CACHE_ARTE_SMALL, 1, 0);
  cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_PERFIL, profile,
                         NV_CACHE_ARTE_MEDIUM, 1, 0);
  cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_HOME, "https://images.example/visible.jpg",
                         NV_CACHE_ARTE_SMALL, 0, 1);
  cachearte_estatisticas(&stats);
  CHECK(stats.essenciais_esperados == 2, "expected count deduplicates by URL across groups");

  cache_name(home, name, sizeof name);
  snprintf(path, sizeof path, "%s/%s", dir, name);
  f = fopen(path, "wb"); CHECK(f != NULL, "create cached image");
  CHECK(fwrite("jpeg-image", 1, 10, f) == 10, "write cached image"); fclose(f);
  cache_name(profile, name, sizeof name);
  snprintf(profilePath, sizeof profilePath, "%s/%s", dir, name);
  f = fopen(profilePath, "wb"); CHECK(f != NULL, "create profile cached image");
  CHECK(fwrite("jpeg-image", 1, 10, f) == 10, "write profile cached image"); fclose(f);
  cache_name(home, name, sizeof name);
  snprintf(ignored, sizeof ignored, "%s/%s.parcial", dir, name);
  f = fopen(ignored, "wb"); CHECK(f != NULL, "create partial file");
  fputs("partial", f); fclose(f);
  CHECK(cachearte_nativo_protegido(path), "HOME essential pin protects its image");
  CHECK(cachearte_nativo_essencial(path), "essential-present helper resolves native filename");

  cachearte_limpar_referencias_grupo(NV_CACHE_ARTE_GRUPO_HOME);
  CHECK(!cachearte_nativo_protegido(path), "clearing HOME keeps no stale pin");
  cachearte_estatisticas(&stats);
  CHECK(stats.essenciais_esperados == 1, "PERFIL reference survives HOME refresh");
  cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_HOME, home,
                         NV_CACHE_ARTE_SMALL, 0, 1);
  CHECK(cachearte_nativo_protegido(path), "transient use pin protects the file");
  cachearte_limpar_uso();
  CHECK(!cachearte_nativo_protegido(path), "clearing use removes only transient protection");
  cachearte_estatisticas(&stats);
  CHECK(stats.essenciais_esperados == 1, "transient clear preserves PROFILE essential");

  cachearte_estatisticas_pedir();
  for (int i = 0; i < 100; i++) {
    cachearte_estatisticas(&stats);
    if (stats.itens == 2 && stats.bytes == 20) break;
    usleep(10000);
  }
  CHECK(stats.itens == 2 && stats.bytes == 20,
        "async inventory counts only hash image files and excludes partials");
  CHECK(stats.essenciais == 1, "inventory reports stored essential coverage separately");
  cachearte_limpar_referencias();
  CHECK(!cachearte_nativo_protegido(path), "global account switch clears every group");
  unlink(ignored); unlink(path); unlink(profilePath); rmdir(dir);
  puts("PASS cachearte native scoped retention and image-only inventory");
  return 0;
}
