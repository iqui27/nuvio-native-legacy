// A FILA DO DECODIFICADOR DO NAVEGADOR, sob prazo curto e pedidos concorrentes.
//
// Reproduz a cascata dos logs da Samsung 1.4.1 (icone de 128x128 levando 8 a
// 17 s) e confere as duas promessas da ponte (src/webp.c + tools/decodificador.js):
//
//   1. NENHUMA RESPOSTA CAI NO PEDIDO ERRADO: cada pedido tem uma cor unica, e
//      o bloco devolvido tem de ter exatamente essa cor em todos os pixels. A
//      memoria liberada entre um pedido e outro e envenenada com a cor-isca
//      (ISCA_*), o papel do logo da NBC: se ela aparece num resultado, a ponte
//      devolveu bloco que ninguem escreveu, ou escreveu com bytes de outro.
//   2. A FILA NAO TRAVA: um pedido abandonado (o C desistiu no prazo) nao pode
//      segurar os seguintes. Mede-se a latencia de cada pedido rapido.
//
// Dois fios, como NV_TEX_FIOS. Um pedido em cada SETE e lento (atraso maior que
// o prazo do C), para forcar abandono; os demais respondem em 5 ms.
#include "../src/webp.h"
#include <emscripten.h>
#include <emscripten/threading.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_NAV_PRAZO_MS
#error "compile com -DNV_NAV_PRAZO_MS=<ms>"
#endif
#define FIOS 2
#define PEDIDOS 36
#define LADO 128
#define ISCA_R 0xAA
#define ISCA_G 0xBB
#define ISCA_B 0xCC

static atomic_int nOk, nNulo, nTrocado, nIsca, nLixo, nLentoRapido, fiosFim;
static atomic_int maxMsRapido;

// PNG de verdade ate o IHDR (webp.c tira o tamanho dali) + registro FAKE que o
// codec falso de tests/decodefila-shim.js entende.
#define TAM_IMG 40
static void imagem(unsigned char *b, int w, int h, int r, int g, int bl, int atraso) {
  static const unsigned char sig[16] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R' };
  unsigned char *f = b + 24;
  memset(b, 0, TAM_IMG);
  memcpy(b, sig, 16);
  b[18] = w >> 8; b[19] = w & 255; b[22] = h >> 8; b[23] = h & 255;
  memcpy(f, "FAKE", 4);
  f[4] = w & 255; f[5] = w >> 8; f[6] = h & 255; f[7] = h >> 8;
  f[8] = r; f[9] = g; f[10] = bl; f[11] = 255;
  f[12] = atraso & 255; f[13] = atraso >> 8;
}

// Deixa a cor-isca no que o allocator acabou de liberar: um bloco do tamanho
// dos pixels e um do tamanho dos bytes comprimidos, com uma imagem-isca valida.
static void envenenar(void) {
  unsigned char *p = malloc(LADO * (LADO + 1) * 4), *q = malloc(TAM_IMG);
  int i;
  if (p) { for (i = 0; i < LADO * (LADO + 1) * 4; i += 4) { p[i] = ISCA_R; p[i + 1] = ISCA_G; p[i + 2] = ISCA_B; p[i + 3] = 255; } }
  if (q) imagem(q, LADO, LADO, ISCA_R, ISCA_G, ISCA_B, 0);
  free(p); free(q);
}

static void *fio(void *arg) {
  int id = (int)(intptr_t)arg, i;
  for (i = 0; i < PEDIDOS; i++) {
    int lento = (i % 7) == 3;
    int r = 10 + id * 100, g = i, bl = 50;
    int w = 0, h = 0, ow = 0, oh = 0;
    unsigned char *dados = malloc(TAM_IMG);
    double t0 = emscripten_get_now(), ms;
    uint8_t *px;
    imagem(dados, LADO, LADO, r, g, bl, lento ? NV_NAV_PRAZO_MS * 3 : 5);
    px = navegador_decodificar(dados, TAM_IMG, "image/png", LADO, &w, &h, &ow, &oh);
    ms = emscripten_get_now() - t0;
    free(dados);
    if (!px) {
      atomic_fetch_add(&nNulo, 1);
      if (!lento) atomic_fetch_add(&nLentoRapido, 1);
    } else {
      int k, errado = 0, isca = 0, outro = 0;
      if (w != LADO || h != LADO) errado = 1;
      for (k = 0; !errado && k < w * h * 4; k += 4) {
        if (px[k] == r && px[k + 1] == g && px[k + 2] == bl && px[k + 3] == 255) continue;
        errado = 1;
        if (px[k] == ISCA_R && px[k + 1] == ISCA_G && px[k + 2] == ISCA_B) isca = 1;
        else if ((px[k] == 10 || px[k] == 110) && px[k + 2] == 50) outro = 1;
      }
      if (!errado) atomic_fetch_add(&nOk, 1);
      else if (isca) { atomic_fetch_add(&nIsca, 1); printf("ISCA   fio=%d pedido=%d recebeu a cor da memoria liberada\n", id, i); }
      else if (outro) { atomic_fetch_add(&nTrocado, 1); printf("TROCA  fio=%d pedido=%d recebeu rgb=%d,%d,%d\n", id, i, px[k - 4], px[k - 3], px[k - 2]); }
      else { atomic_fetch_add(&nLixo, 1); printf("LIXO   fio=%d pedido=%d %dx%d rgb=%d,%d,%d\n", id, i, w, h, px[0], px[1], px[2]); }
      free(px);
    }
    if (!lento) {
      int m = (int)ms, ant = atomic_load(&maxMsRapido);
      while (m > ant && !atomic_compare_exchange_weak(&maxMsRapido, &ant, m)) {}
    }
    envenenar();
  }
  atomic_fetch_add(&fiosFim, 1);
  return NULL;
}

// Depois dos fios, espera os lentos abandonados terminarem no Worker (atraso
// de 3x o prazo) e confere que o C devolveu tudo ao allocator.
static double fimEm;
static void fim(void *u) {
  int vivos, ruim;
  (void)u;
  if (atomic_load(&fiosFim) < FIOS) { emscripten_async_call(fim, NULL, 50); return; }
  if (!fimEm) fimEm = emscripten_get_now();
  vivos = navegador_abandonados_vivos();
  if (vivos && emscripten_get_now() - fimEm < NV_NAV_PRAZO_MS * 6) { emscripten_async_call(fim, NULL, 50); return; }
  printf("RESULTADO ok=%d nulos=%d nulos_rapidos=%d trocados=%d isca=%d lixo=%d max_ms_rapido=%d abandonados_vivos=%d prazo=%d\n",
         nOk, nNulo, nLentoRapido, nTrocado, nIsca, nLixo, maxMsRapido, vivos, NV_NAV_PRAZO_MS);
  ruim = nTrocado || nIsca || nLixo || nLentoRapido || vivos || maxMsRapido >= NV_NAV_PRAZO_MS;
  printf("%s\n", ruim ? "FALHOU" : "PASS: nenhuma resposta no pedido errado, fila sem trava, abandonados liberados");
  fflush(stdout);
  emscripten_force_exit(ruim ? 2 : 0);
}

int main(void) {
  pthread_t t[FIOS];
  int i;
  for (i = 0; i < FIOS; i++) pthread_create(&t[i], NULL, fio, (void *)(intptr_t)i);
  emscripten_async_call(fim, NULL, 50);
  return 0;
}
