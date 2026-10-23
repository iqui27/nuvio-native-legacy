// Exercise the production overlay formatter with deterministic memory samples.
#include "../src/memlog.h"
#include "../src/video.h"
#include "../src/desempenho.c"
#include <assert.h>

static long available = 280;
static int playing = 1, reads;
static char last[256];
long memlog_livre_mb(void) { reads++; return available; }
int video_ativo(void) { return playing; }
const char *i18n(const char *s) {
  return !strcmp(s, "%.0f MB · %ld livre") ? "%.0f MB · %ld free" : s;
}
int ajustes_idioma(void) { return 1; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  (void)e; (void)r; (void)g; (void)b; (void)a;
  snprintf(last, sizeof last, "%s", s);
  return (TxtLinha){.w = (int)strlen(s), .h = 16};
}
static void sample(void) { desempenho_amostra(60, 17, 0, 2, 1, 0, 0, 0, 153); }
int main(void) {
  Linha line;
  sample();
  montar(DS_MINIMO, &line);
  assert(!strcmp(last, "153 MB · 280 free"));
  linhaMenor();
  assert(strstr(last, "153 MB · 280 free"));
  assert(reads == 1); // No /proc reads during drawing or width calculation.
  available = 99; sample(); montar(DS_MINIMO, &line);
  assert(!strcmp(last, "153 MB · 99 free"));
  available = -1; sample(); montar(DS_MINIMO, &line);
  assert(!strcmp(last, "153 MB"));
  available = 280; playing = 0; sample(); montar(DS_MINIMO, &line);
  assert(!strcmp(last, "153 MB"));
  puts("buffer_overlay: ok");
}
