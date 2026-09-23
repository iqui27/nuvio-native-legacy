// Os tempos do sidecar do mkvass LIDOS PELO LIBASS (#92, 23/09/2026). O
// tests/mkvass.c confere pelo parser de legenda.c, que le "%lf" e aceitava os
// milesimos que o mkvass escrevia; o libass le a fracao como centesimos e
// jogava cada fala 0-9,9 s para a frente. Este confere o que a TV desenha.
//   ass_tempos <referencia.ass> <sidecar.ass>
#include <ass/ass.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void nada(int n, const char *f, va_list a, void *d) { (void)n; (void)f; (void)a; (void)d; }
static int cmp(const void *a, const void *b) {
  long long x = *(const long long *)a, y = *(const long long *)b; return x < y ? -1 : x > y;
}
static ASS_Track *ler(ASS_Library *l, const char *c) {
  FILE *f = fopen(c, "rb"); char *b; long n; ASS_Track *t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); return NULL; }
  fclose(f); b[n] = 0;
  t = ass_read_memory(l, b, (size_t)n, "UTF-8"); free(b); return t;
}
int main(int argc, char **argv) {
  ASS_Library *l; ASS_Track *r, *s; long long *a, *b, *da, *db; int i, n, ruins = 0, negativos = 0;
  long long pior = 0;
  if (argc < 3) return 2;
  l = ass_library_init(); ass_set_message_cb(l, nada, NULL);
  r = ler(l, argv[1]); s = ler(l, argv[2]);
  if (!r || !s) { printf("FALHA leitura\n"); return 1; }
  if (r->n_events != s->n_events) { printf("FALHA eventos: ref %d, sidecar %d\n", r->n_events, s->n_events); return 1; }
  n = r->n_events;
  a = malloc(sizeof *a * n); b = malloc(sizeof *b * n); da = malloc(sizeof *da * n); db = malloc(sizeof *db * n);
  for (i = 0; i < n; i++) {
    a[i] = r->events[i].Start * 100000LL + (i % 100000);  // Start + desempate
    b[i] = s->events[i].Start * 100000LL + (i % 100000);
    if (s->events[i].Duration <= 0) negativos++;
  }
  qsort(a, (size_t)n, sizeof *a, cmp); qsort(b, (size_t)n, sizeof *b, cmp);
  for (i = 0; i < n; i++) {
    long long d = a[i] / 100000LL - b[i] / 100000LL; if (d < 0) d = -d;
    if (d > pior) pior = d;
    if (d > 10) ruins++;
  }
  (void)da; (void)db;
  printf("  libass: %d eventos, pior diferenca de Start %lld ms, %d acima de 10 ms, %d com duracao <= 0\n",
         n, pior, ruins, negativos);
  if (ruins || negativos) { printf("FALHA tempos do sidecar no libass\n"); return 1; }
  printf("ass_tempos: ok\n");
  return 0;
}
