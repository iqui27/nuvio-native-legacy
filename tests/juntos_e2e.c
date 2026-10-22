// WATCH TOGETHER DE PONTA A PONTA: dois motores em C (src/juntosmotor.c, o do
// app) pelo websocket DO APP (src/discordws.c, ws:// local) contra o
// nuvio-juntos de verdade (workerd via `wrangler dev`). Os players sao
// simulados; o resto e o codigo que vai para a TV. Os bilhetes vem do
// argv (tests/juntos_e2e.sh assina com servidor/juntos/src/bilhete.js).
//
//   bash tests/juntos_e2e.sh
#include "discordws.h"
#include "juntosmotor.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

typedef struct { double pos, ultT; int quer, pronto, vel; double playAte; } Sim;
static Sim S[2];
static JsMotor M[2];
static DiscordWs *W[2];

static void avancar(Sim *s, double t) {
  if (s->quer && t >= s->playAte) s->pos += (t - s->ultT) / 1000.0 * s->vel / 100.0;
  s->ultT = t;
}
static double sPos(void *u, double *t) { Sim *s = u; *t = agoraMs(); return s->pos; }
static int sToc(void *u) { Sim *s = u; return s->quer && agoraMs() >= s->playAte; }
static int sPronto(void *u) { return ((Sim *)u)->pronto; }
static unsigned sBuf(void *u) { (void)u; return 0; }
static double sDur(void *u) { (void)u; return 7200; }
static void sPausar(void *u, int p) { Sim *s = u; if (!p && !s->quer) s->playAte = agoraMs() + 150; s->quer = !p; }
static void sBuscar(void *u, double x) { ((Sim *)u)->pos = x; }
static void sVel(void *u, int c) { ((Sim *)u)->vel = c; }
static int enviar(void *u, const char *j) { return dws_enviar(W[(int)(intptr_t)u], j); }

static void passo(void) {
  int i;
  double t = agoraMs();
  for (i = 0; i < 2; i++) {
    char *m;
    avancar(&S[i], t);
    if (dws_estado(W[i]) == DWS_ABERTO && !M[i].conectado) jsm_conectou(&M[i], t);
    while ((m = dws_receber(W[i]))) { jsm_receber(&M[i], m, t); free(m); }
    jsm_passo(&M[i], t);
    { JsEvento e; while (jsm_evento(&M[i], &e)) {} }
  }
}
static void rodar(double ms) { double fim = agoraMs() + ms; while (agoraMs() < fim) { passo(); SDL_Delay(5); } }
static int esperar(int (*f)(void), double ms) {
  double fim = agoraMs() + ms;
  while (agoraMs() < fim) { passo(); if (f()) return 1; SDL_Delay(5); }
  return 0;
}
static int ambosTocando(void) { return M[0].sala.rel.estado == JSE_TOCANDO && M[1].sala.rel.estado == JSE_TOCANDO; }
static int ambosPausados(void) { return M[0].sala.rel.estado == JSE_PAUSADO && M[1].sala.rel.estado == JSE_PAUSADO; }
static int doisMembros(void) { return M[0].sala.n == 2 && M[1].sala.n == 2; }

int main(int argc, char **argv) {
  JsPlayer jp = { sPos, sToc, sPronto, sBuf, sDur, sPausar, sBuscar, sVel, NULL };
  char url[2400];
  int i, falhas = 0;
  if (argc < 4) { printf("uso: juntos_e2e <porta> <bilhete anfitriao> <bilhete convidado>\n"); return 2; }
  for (i = 0; i < 2; i++) {
    S[i].vel = 100; S[i].pronto = 1; S[i].ultT = agoraMs(); S[i].quer = 1;
    jp.u = &S[i];
    jsm_iniciar(&M[i], i ? "nuvio:convidado" : "nuvio:anfitriao", "mac", "e2e", 'A', &jp, enviar, (void *)(intptr_t)i);
    snprintf(url, sizeof url, "ws://127.0.0.1:%s/ws?b=%s", argv[1], argv[2 + i]);
    W[i] = dws_abrir(url);
    if (!i) rodar(1500);   // a sala nasce do anfitriao
  }
  if (!esperar(doisMembros, 8000)) { printf("FALHA: os dois nao chegaram na sala (estado ws %d/%d)\n", dws_estado(W[0]), dws_estado(W[1])); return 1; }
  // Lobby: os players ficam parados no quadro comum.
  rodar(1500);
  if (S[0].quer || S[1].quer) { printf("FALHA: lobby deveria pausar os players\n"); falhas++; }
  // O convidado (que comecou 12 s a frente) e levado ao pos0 da sala.
  S[1].pos += 12;
  jsm_cmd(&M[1], "comecar", -1, NULL, agoraMs());
  if (!esperar(ambosTocando, 5000)) { printf("FALHA: comecar\n"); return 1; }
  rodar(6000);
  {
    double d = fabs(S[0].pos - S[1].pos) * 1000;
    printf("  depois de comecar: anfitriao %.3f s, convidado %.3f s, diferenca %.0f ms\n", S[0].pos, S[1].pos, d);
    if (d > 150 || S[0].pos < 3) { printf("FALHA: desalinhados\n"); falhas++; }
  }
  jsm_cmd(&M[0], "pausa", -1, NULL, agoraMs());
  if (!esperar(ambosPausados, 4000)) { printf("FALHA: pausa\n"); return 1; }
  rodar(800);
  if (fabs(S[0].pos - S[1].pos) > 0.3 || S[1].quer) { printf("FALHA: pausa nao parou no mesmo quadro (%.3f / %.3f)\n", S[0].pos, S[1].pos); falhas++; }
  jsm_cmd(&M[1], "seek", 600, NULL, agoraMs());
  rodar(1500);
  jsm_cmd(&M[1], "play", -1, NULL, agoraMs());
  if (!esperar(ambosTocando, 4000)) { printf("FALHA: play\n"); return 1; }
  rodar(4000);
  {
    double d = fabs(S[0].pos - S[1].pos) * 1000;
    printf("  depois de seek 600 s e play: %.3f / %.3f s, diferenca %.0f ms\n", S[0].pos, S[1].pos, d);
    if (d > 150 || S[0].pos < 602 || S[0].pos > 606) { printf("FALHA: seek/play\n"); falhas++; }
  }
  jsm_sair(&M[1]);
  rodar(500);
  dws_fechar(W[0]); dws_fechar(W[1]);
  if (falhas) return 1;
  printf("juntos_e2e: motor + websocket do app contra o nuvio-juntos local: alinhados\n");
  return 0;
}
