// BANCADA DO GIF NO NAVEGADOR (tests/gifbanco-tizen.sh, #84): o gif_textura
// de verdade, com WebGL e pthreads, tocando cada GIF ate a primeira volta
// completa. Compila contra o gif.c NOVO ou contra o da 1.4.6 (-DNV_BANCO_ANTIGO,
// com o decodificador.js dela), e mede o mesmo nos dois:
//
//   volta     ms da primeira volta completa (a linha "[gif] deu a volta")
//   fio       ms gastos DENTRO de gif_textura no fio principal, por quadro do GIF
//   pior      a chamada mais longa (ms)
//   quadros   quadros de tela (rAF) no periodo, e quantos passaram de 50 ms
//
// O ANTIGO e chamado a cada 67 ms, como home.c e perfilsel.c chamavam (e,
// com -DPASSO_MS=0, a cada desenho, para separar o passo do decode); o NOVO a
// cada desenho, como chamam agora.
#include "../src/gif.h"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdio.h>
#include <string.h>

#ifdef NV_BANCO_ANTIGO
// O gif.c da 1.4.6 pergunta ao cache de arte se ha arte na fila; aqui nao ha.
void tex_estatisticas(int *itens, int *pend, long *bytes, int *quentes, long *bq) {
  if (itens) *itens = 0;
  if (pend) *pend = 0;
  if (bytes) *bytes = 0;
  if (quentes) *quentes = 0;
  if (bq) *bq = 0;
}
#define NOME "antigo"
#ifndef PASSO_MS
#define PASSO_MS 67.0
#endif
#else
#define NOME "novo"
#endif
#ifndef PASSO_MS
#define PASSO_MS 0.0
#endif

EM_JS(int, voltas, (), { return Module.nvVoltas | 0; });
EM_JS(void, fim, (), { Module.nvFim && Module.nvFim(); });

static const struct { const char *arq; int larg; } G[] = {
  { "/a35.gif", 200 }, { "/b75.gif", 480 }, { "/c198.gif", 200 },
};
static int gi, voltasAntes, chamadas, quadrosTela, lentos;
static double inicio, ultimaChamada, ultimoQuadro, fioMs, pior;

static void laco(void) {
  double t = emscripten_get_now();
  if (gi >= (int)(sizeof G / sizeof G[0])) return;
  if (ultimoQuadro > 0.0) { quadrosTela++; if (t - ultimoQuadro > 50.0) lentos++; }
  ultimoQuadro = t;
  if (!inicio) { inicio = t; voltasAntes = voltas(); gif_parar(); }
  if (t - ultimaChamada >= PASSO_MS) {
    double a = emscripten_get_now(), d;
    gif_textura(G[gi].arq, G[gi].larg);
    d = emscripten_get_now() - a;
    fioMs += d;
    if (d > pior) pior = d;
    chamadas++;
    ultimaChamada = t;
  }
  if (voltas() > voltasAntes || t - inicio > 60000.0) {
    printf("BANCO %s(passo %.0f) %s chamadas=%d fio_total=%.1fms pior=%.2fms quadros_tela=%d acima_50ms=%d periodo=%.0fms\n",
           NOME, PASSO_MS, G[gi].arq, chamadas, fioMs, pior, quadrosTela, lentos, t - inicio);
    fflush(stdout);
    gif_parar();
    gi++;
    inicio = 0.0; chamadas = 0; fioMs = 0.0; pior = 0.0; quadrosTela = 0; lentos = 0; ultimoQuadro = 0.0;
    if (gi >= (int)(sizeof G / sizeof G[0])) { printf("FIM\n"); fflush(stdout); fim(); }
  }
}

int main(void) {
  EmscriptenWebGLContextAttributes at;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx;
  emscripten_webgl_init_context_attributes(&at);
  at.majorVersion = 1;
  ctx = emscripten_webgl_create_context("#canvas", &at);
  if (ctx <= 0) { printf("BANCO sem WebGL\nFIM\n"); fflush(stdout); fim(); return 0; }
  emscripten_webgl_make_context_current(ctx);
  emscripten_set_main_loop(laco, 0, 0);
  return 0;
}
