// CONFERIR VARIAS FONTES AO MESMO TEMPO (2.0.2): a fonte escolhida e sempre a
// primeira que serve NA ORDEM DA FILA, mesmo que uma pior responda antes. Sem
// rede: o verificador falso dorme o que a cena manda.
//
//   bash tests/fonteparalela.sh
#include "fonteparalela.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHOU %s:%d: ", __FILE__, __LINE__); \
  printf(__VA_ARGS__); printf("\n"); } } while (0)

#define N 8
typedef struct {
  int boa[N], ms[N];
  int tocou[N], falhou[N];
  pthread_mutex_t m;
} Cena;
static int verificar(int i, void *u) {
  Cena *c = u;
  usleep((useconds_t)c->ms[i] * 1000);
  pthread_mutex_lock(&c->m); c->tocou[i]++; pthread_mutex_unlock(&c->m);
  return c->boa[i];
}
static void falhou(int i, void *u) { Cena *c = u; pthread_mutex_lock(&c->m); c->falhou[i]++; pthread_mutex_unlock(&c->m); }
static void cena(Cena *c) { memset(c, 0, sizeof *c); pthread_mutex_init(&c->m, NULL); }
static long agora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000L + t.tv_nsec / 1000000L; }

static int pronta(int i, void *u) { return ((const int *)u)[i]; }

// 8. `u` no heap, solto pelo ULTIMO fio (fonteparalela_soltando).
static int soltos;
static void soltarCena(void *u) {
  Cena *c = u;
  pthread_mutex_lock(&c->m); soltos++; pthread_mutex_unlock(&c->m);
  pthread_mutex_destroy(&c->m);
  free(c);
}

int main(void) {
  int fila[8] = { 0, 1, 2, 3, 4, 5, 6, 7 }, tocadas, r;
  long t0;
  Cena c;

  // 1. A pior responde primeiro, mas a melhor tambem serve: ganha a melhor.
  cena(&c); c.boa[0] = 1; c.ms[0] = 80; c.boa[1] = 1; c.ms[1] = 5; c.boa[2] = 1; c.ms[2] = 5;
  r = fonteparalela(fila, 8, 3, verificar, falhou, &c, &tocadas, 0);
  CONFERE(r == 0, "a primeira da fila que serve ganha, mesmo lenta (r=%d)", r);
  CONFERE(tocadas == 3, "so as 3 primeiras comecam (%d)", tocadas);
  usleep(150000);
  CONFERE(c.tocou[3] == 0 && c.tocou[4] == 0, "nenhuma alem das 3 e conferida");

  // 2. A primeira morre rapido, a segunda serve: devolve sem esperar a terceira lenta.
  cena(&c); c.boa[0] = 0; c.ms[0] = 5; c.boa[1] = 1; c.ms[1] = 20; c.boa[2] = 1; c.ms[2] = 600;
  t0 = agora();
  r = fonteparalela(fila, 8, 3, verificar, falhou, &c, &tocadas, 0);
  CONFERE(r == 1, "segue para a segunda (r=%d)", r);
  CONFERE(agora() - t0 < 400, "nao espera a terceira (%ld ms)", agora() - t0);
  usleep(50000);
  CONFERE(c.falhou[0] == 1 && c.falhou[1] == 0, "so a reprovada vai para falhou");
  usleep(700000);   // a terceira termina sozinha, sem tocar em memoria liberada

  // 3. Nenhuma das 3 serve: -1, e todas foram para falhou.
  cena(&c); c.ms[0] = 5; c.ms[1] = 10; c.ms[2] = 15;
  r = fonteparalela(fila, 8, 3, verificar, falhou, &c, &tocadas, 0);
  CONFERE(r == -1, "nenhuma serve (r=%d)", r);
  usleep(50000);
  CONFERE(c.falhou[0] == 1 && c.falhou[1] == 1 && c.falhou[2] == 1, "as tres reprovadas");

  // 4. Fila de 2: so 2 fios. Fila de 1: uma.
  cena(&c); c.boa[0] = 0; c.boa[1] = 1;
  r = fonteparalela(fila, 2, 3, verificar, falhou, &c, &tocadas, 0);
  CONFERE(r == 1 && tocadas == 2, "k limitado a n (r=%d tocadas=%d)", r, tocadas);
  cena(&c); c.boa[0] = 1;
  r = fonteparalela(fila, 1, 3, verificar, falhou, &c, &tocadas, 0);
  CONFERE(r == 0 && tocadas == 1, "uma so");
  CONFERE(fonteparalela(fila, 0, 3, verificar, falhou, &c, &tocadas, 0) == -1 && tocadas == 0, "fila vazia");

  // 5. Fila que nao comeca em 0 (a ordem de exibicao): devolve o indice da fila, nao a posicao.
  { int f2[3] = { 5, 2, 7 };
    cena(&c); c.boa[5] = 0; c.boa[2] = 1; c.boa[7] = 1;
    r = fonteparalela(f2, 3, 3, verificar, falhou, &c, &tocadas, 0);
    CONFERE(r == 2, "devolve o indice da fila (r=%d)", r); }

  // 6. Prazo: o que nao respondeu a tempo conta como nao serviu.
  cena(&c); c.boa[0] = 1; c.ms[0] = 800;
  t0 = agora();
  r = fonteparalela(fila, 1, 1, verificar, falhou, &c, &tocadas, 100);
  CONFERE(r == -1 && agora() - t0 < 500, "prazo vencido (r=%d, %ld ms)", r, agora() - t0);
  usleep(900000);

  // 7. So fonte ja em cache entra na conferencia conjunta: o prefixo pronto da
  // fila, no maximo 3. A primeira nao pronta corta; uncached segue em serie.
  { int cache[8] = { 1, 1, 1, 1, 0, 0, 0, 0 }, f3[4] = { 4, 0, 1, 2 }, f4[3] = { 0, 4, 1 }, tudo[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    CONFERE(fonteparalela_prefixo(fila, 8, 3, pronta, cache) == 3, "4 em cache: so 3 juntas");
    CONFERE(fonteparalela_prefixo(fila, 2, 3, pronta, cache) == 2, "fila curta");
    CONFERE(fonteparalela_prefixo(f3, 4, 3, pronta, cache) == 0, "primeira fora do cache: nada em paralelo");
    CONFERE(fonteparalela_prefixo(f4, 3, 3, pronta, cache) == 1, "uncached no meio corta o prefixo (a seguinte nao pula a fila)");
    CONFERE(fonteparalela_prefixo(fila, 8, 9, pronta, tudo) == FONTEPARALELA_MAX, "teto FONTEPARALELA_MAX"); }

  // 8. A funcao volta com a primeira que serve, mas a segunda ainda confere e
  // ainda usa `u`. Com u na pilha de quem chamou (o que streams.c fazia), o fio
  // lia e escrevia num quadro desfeito. soltarU tem de vir so depois do ultimo
  // fio, e uma vez. Com SANITIZE=1, um free antes da hora vira
  // heap-use-after-free aqui mesmo.
  { Cena *h = malloc(sizeof *h);
    cena(h); h->boa[0] = 1; h->boa[1] = 1; h->ms[1] = 300;
    soltos = 0;
    r = fonteparalela_soltando(fila, 2, 2, verificar, falhou, h, &tocadas, 0, soltarCena);
    CONFERE(r == 0 && soltos == 0, "volta com a primeira e nao solta u com fio no ar (r=%d soltos=%d)", r, soltos);
    usleep(600000);
    CONFERE(soltos == 1, "u solto uma vez, depois do ultimo fio (soltos=%d)", soltos);
    soltos = 0;
    r = fonteparalela_soltando(fila, 0, 2, verificar, falhou, NULL, &tocadas, 0, NULL);
    CONFERE(r == -1, "fila vazia sem soltarU"); }

  if (falhas) { printf("fonteparalela: %d falha(s)\n", falhas); return 1; }
  printf("fonteparalela: ok\n");
  return 0;
}
