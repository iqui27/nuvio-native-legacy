#include "marcacena.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static MarcaCena filme(int seg) {
  MarcaCena m; memset(&m, 0, sizeof m);
  strcpy(m.id, "tt0111161"); m.tipo = 'm'; m.seg = seg; m.durSeg = 8520; m.quando = 1700000000;
  return m;
}

// The published server lowercases and keeps only [a-z0-9 ], cut at 60.
static void servidor(const char *s, char *dst) {
  size_t n = 0;
  for (; *s && n < 60; s++) dst[n++] = (isalnum((unsigned char)*s) || *s == ' ') ? (char)tolower((unsigned char)*s) : ' ';
  dst[n] = 0;
}

int main(void) {
  char b[256], c[128];
  MarcaCena m = filme(3723), r, v[4], w[4];
  int n = 0, inc, i;

  assert(!strcmp(marcacena_tempo(3723, b, sizeof b), "1:02:03"));
  assert(!strcmp(marcacena_tempo(123, b, sizeof b), "2:03"));
  assert(!strcmp(marcacena_tempo(-5, b, sizeof b), "0:00"));

  assert(marcacena_texto(&m, b, sizeof b) && !strcmp(b, "cena 1h02m03s d8520"));
  servidor(b, c); assert(!strcmp(b, c));   // survives the server untouched
  memset(&r, 0, sizeof r);
  assert(marcacena_ler_texto(c, &r) && r.seg == 3723 && r.durSeg == 8520 && !r.episodio);
  m.tipo = 's'; m.temporada = 2; m.episodio = 5; m.seg = 59;
  assert(marcacena_texto(&m, b, sizeof b) && !strcmp(b, "cena 0h00m59s t2 e5 d8520"));
  assert(marcacena_ler_texto(b, &r) && r.seg == 59 && r.temporada == 2 && r.episodio == 5);
  m.temporada = 999; m.episodio = 9999; m.seg = 99 * 3600; m.durSeg = 99 * 3600;
  assert(marcacena_texto(&m, b, sizeof b) && strlen(b) <= MARCACENA_TEXTO_MAX);
  assert(marcacena_ler_texto(b, &r) && r.seg == 99 * 3600);
  assert(!marcacena_texto(&m, b, 8));
  assert(!marcacena_ler_texto("muito bom", &r));
  assert(!marcacena_ler_texto("cena", &r));
  assert(!marcacena_ler_texto("cena 1h02m03", &r));
  assert(!marcacena_ler_texto("cena 1h61m03s", &r));
  assert(!marcacena_ler_texto("cena 1h02m03s do ano", &r));
  assert(!marcacena_ler_texto("cena 1h02m03s t2", &r));
  assert(!marcacena_ler_texto("cena 99999999h02m03s", &r));
  assert(!marcacena_ler_texto(NULL, &r));

  m = filme(3723);
  assert(marcacena_link(&m, b, sizeof b) && !strcmp(b, "nuvio-legacy://play/movie/tt0111161?t=3723"));
  m.tipo = 's'; m.temporada = 2; m.episodio = 5;
  assert(marcacena_link(&m, b, sizeof b) && !strcmp(b, "nuvio-legacy://play/series/tt0111161/2/5?t=3723"));
  assert(!marcacena_link(&m, b, 10));
  strcpy(m.id, "tt1/../x"); assert(!marcacena_link(&m, b, sizeof b) && !marcacena_valida(&m));

  assert(marcacena_ancorar(100, 8520, 8520, &inc) == 100 && !inc);
  assert(marcacena_ancorar(100, 8520, 8600, &inc) == 100 && !inc);
  assert(marcacena_ancorar(100, 8520, 9900, &inc) == 100 && inc);
  assert(marcacena_ancorar(100, 0, 9900, &inc) == 100 && !inc);
  assert(marcacena_ancorar(9000, 9900, 8520, &inc) == -1);
  assert(marcacena_ancorar(-1, 0, 0, NULL) == -1);
  assert(marcacena_ancorar(100, 0, 0, NULL) == 100);

  m = filme(100); assert(marcacena_inserir(v, &n, 4, &m) == 0 && n == 1);
  m = filme(103); assert(marcacena_inserir(v, &n, 4, &m) == 0 && n == 1 && v[0].seg == 103);
  m = filme(200); assert(marcacena_inserir(v, &n, 4, &m) == 1);
  m = filme(300); marcacena_inserir(v, &n, 4, &m);
  m = filme(400); marcacena_inserir(v, &n, 4, &m);
  m = filme(500); assert(marcacena_inserir(v, &n, 4, &m) == 3 && n == 4 && v[0].seg == 200);
  m = filme(201); m.episodio = 1; m.tipo = 's';
  assert(marcacena_inserir(v, &n, 4, &m) == 3 && v[0].seg == 300);   // other episode: not merged
  m.seg = -1; assert(marcacena_inserir(v, &n, 4, &m) == -1 && n == 4);

  assert(marcacena_serializar(v, n, b, sizeof b) > 0);
  assert(marcacena_carregar(b, w, 4) == 4);
  for (i = 0; i < 4; i++)
    assert(!strcmp(v[i].id, w[i].id) && v[i].tipo == w[i].tipo && v[i].seg == w[i].seg &&
           v[i].episodio == w[i].episodio && v[i].durSeg == w[i].durSeg && v[i].quando == w[i].quando);
  assert(marcacena_serializar(v, n, c, 40) < strlen(b));            // truncates on a line edge
  assert(marcacena_carregar(c, w, 4) == 1);
  assert(marcacena_carregar("lixo\n\ntt1 m 0 0 10 0 5\ntt2 x 0 0 10 0 5\ntt3 m 0 0 -4 0 5\n", w, 4) == 1 &&
         !strcmp(w[0].id, "tt1"));
  assert(marcacena_carregar(NULL, w, 4) == 0);
  puts("marcacena: ok");
  return 0;
}
