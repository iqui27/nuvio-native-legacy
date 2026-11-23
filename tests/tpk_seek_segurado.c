// Seek segurado pela sonda do mkvfix (PR 680, item 6): enquanto a sonda do
// cabecalho nao responde a TV nao recebe a busca — num arquivo de indice
// escondido ela falhava e virava "conexao caiu". Ate 3 s; o alvo mais novo
// vence; a reabertura pelo servidor local acontece NO alvo; e o rotulo da
// reconexao separa queda de rede de verdade (com o arquivo original na TV)
// do erro na janela da reabertura local ("preparando o arquivo"). Host falso,
// sem rede (o mesmo harness de tests/tpk-mp4-mkvfix-legendas.c).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "mkvfix.h"
#include "video.h"
#include "mkvass.h"
#include "intro.h"
#include "rede.h"

extern void mkvfix_mp4_stub_ocultar(int n);
extern void mkvfix_mp4_stub_ocultas(int n);
extern void mkvfix_mp4_stub_estado(int e);
extern void dts_tpk_stub_ligar(int on);
extern int dts_tpk_stub_aberturas(void);

const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { return c && p && !strcmp(c, p); }
const char *ling_audio(void) { return ""; }
const char *ling_audio2(void) { return ""; }
const char *ling_original(void) { return ""; }
int ling_tipo_legenda(const char *s, int f, int d) { (void)s; (void)f; (void)d; return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t; return ""; }
const char *ling_do_nome(const char *nome) { (void)nome; return NULL; }
int ling_letreiro(const char *nome, int forcado) { (void)nome; (void)forcado; return 0; }
void mkvass_aceitar_texto(int sim) { (void)sim; }
void mkvass_aceitar_imagem(int sim) { (void)sim; }
void intro_definir_capitulos(const IntroTrecho *v, int n) { (void)v; (void)n; }
void intro_definir_capitulos_palpite(const IntroTrecho *v, int n) { (void)v; (void)n; }
void intro_definir_capitulos_marcas(const double *v, int n) { (void)v; (void)n; }
void rede_lateral(int sim) { (void)sim; }

static int posMs;
char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim, long *tam) {
  (void)url; (void)segundos; (void)ini; (void)fim; (void)tam;
  return 0;
}
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *tam, int *status, int *erro, char *final, unsigned tamFinal) {
  (void)url; (void)segundos; (void)ini; (void)fim; (void)final; (void)tamFinal;
  if (status) *status = 0; if (erro) *erro = 1; if (tam) *tam = 0;
  return 0;
}
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  (void)url; (void)buf; (void)n; return 0;
}

void nv_tpk_video_registrar(void (*)(const char *, const char *), void (*)(void), void (*)(int),
                            void (*)(int), void (*)(int), void (*)(int, int, int, int), int (*)(void));
void nv_tpk_video_registrar_faixas(void (*)(int, int));
void nv_tpk_video_faixa(int tipo, int idx, const char *lingua);
void nv_tpk_video_faixas_fim(int selAudio, int selLeg);
void nv_tpk_video_evento(int tipo, int a, int b);

static int falhas, nBuscar, ultBuscarMs = -1, nAbrirMkv;
static void hAbrir(const char *u, const char *c) { (void)c; if (u && strstr(u, "127.0.0.1")) nAbrirMkv++; }
static void hBuscarT(int ms) { nBuscar++; ultBuscarMs = ms; }
static void hSem(void) {}
static void hInt(int v) { (void)v; }
static void hJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static int hPos(void) { return posMs; }
static void hEscolher(int tipo, int idx) { (void)tipo; (void)idx; }
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static void bombear(int vezes) { while (vezes--) { video_bombear(); SDL_Delay(20); } }
static void bombear_ms(int ms) {
  Uint32 fim = SDL_GetTicks() + (Uint32)ms;
  do { video_bombear(); SDL_Delay(20); } while (SDL_GetTicks() < fim);
}
static void pronto(int dur, int nAud, int nLeg) {
  int i;
  nv_tpk_video_evento(1, dur, 0);
  for (i = 0; i < nAud; i++) nv_tpk_video_faixa(0, i, "eng");
  for (i = 0; i < nLeg; i++) nv_tpk_video_faixa(1, i, i ? "por" : "eng");
  nv_tpk_video_faixas_fim(nAud ? 0 : -1, nLeg ? 0 : -1);
}

int main(void) {
  SDL_Init(0);
  nv_tpk_video_registrar(hAbrir, hSem, hInt, hBuscarT, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_iniciar_auto();

  /* A: held while the probe runs; successive seeks keep only the newest. */
  posMs = 0; nBuscar = nAbrirMkv = 0; ultBuscarMs = -1;
  video_definir_reconexao(1); video_definir_mp4(0);
  video_tocar("http://h/hold.mkv");
  pronto(7200000, 1, 0);
  bombear(1);   /* sonda sai: .mkv -> SONDANDO no stub */
  ok(mkvfix_estado() == MKVFIX_SONDANDO, "A the header probe is running");
  video_buscar(300.0);
  ok(nBuscar == 0 && video_tpk_seek_segurado_ms() > 0, "A seek held during the probe");
  video_buscar(500.0);
  ok(nBuscar == 0, "A a second seek stays held too");
  mkvfix_mp4_stub_estado(MKVFIX_NORMAL);
  bombear(2);
  ok(nBuscar == 1 && ultBuscarMs == 500000, "A the newest target goes to the TV");
  video_parar();

  /* B: probe without an answer for 3 s: the seek goes raw, like before. */
  posMs = 0; nBuscar = 0; ultBuscarMs = -1;
  video_definir_reconexao(1); video_definir_mp4(0);
  video_tocar("http://h/slow.mkv");
  pronto(7200000, 1, 0);
  bombear(1);
  video_buscar(120.0);
  ok(nBuscar == 0, "B seek held while the probe runs");
  bombear_ms(3200);
  ok(nBuscar == 1 && ultBuscarMs == 120000, "B no answer in 3 s: seek goes to the TV as is");
  video_parar();

  /* C: hidden-index answer reopens through the local fix AT the held target. */
  posMs = 0; nBuscar = nAbrirMkv = 0; ultBuscarMs = -1;
  video_definir_reconexao(1); video_definir_mp4(0);
  video_tocar("http://h/index.mkv");
  pronto(7200000, 1, 0);
  bombear(1);
  video_buscar(459.0);
  ok(nBuscar == 0, "C resume seek held");
  mkvfix_mp4_stub_ocultas(2); mkvfix_mp4_stub_estado(MKVFIX_PRECISA);
  bombear(4);
  ok(nAbrirMkv > 0, "C reopen goes through the local server");
  pronto(7200000, 1, 0);
  nv_tpk_video_evento(2, 0, 0);   /* tocando: o alvo sai */
  bombear(4);
  ok(ultBuscarMs == 459000, "C the reopen seeks to the held target, not to 0");
  video_parar();

  /* D: a network drop during the probe keeps the network label — the
     original file is still on the TV; it retries all the same. */
  posMs = 6000; nBuscar = 0; ultBuscarMs = -1;
  video_definir_reconexao(1); video_definir_mp4(0);
  video_tocar("http://h/prep.mkv");
  pronto(7200000, 1, 0);
  bombear(1);
  video_buscar(60.0);
  nv_tpk_video_evento(5, (int)0xFE6C0026u, 0);   /* EV_ERRO CONNECTION_FAILED com a sonda no ar */
  bombear(2);
  ok(video_reconectando() > 0, "D the player error starts a reconnect");
  ok(video_tpk_reconexao_preparo() == 0, "D a network drop during the probe is not preparation");
  video_parar();

  /* E: the same error right after the reopen through the local fix IS the
     file being prepared — the player talks to the app's 127.0.0.1. */
  posMs = 6000; nBuscar = nAbrirMkv = 0; ultBuscarMs = -1;
  video_definir_reconexao(1); video_definir_mp4(0);
  video_tocar("http://h/reopen.mkv");
  pronto(7200000, 1, 0);
  bombear(1);
  video_buscar(60.0);
  mkvfix_mp4_stub_ocultas(1); mkvfix_mp4_stub_estado(MKVFIX_PRECISA);
  bombear(4);
  ok(nAbrirMkv > 0, "E the held seek reopens through the local server");
  nv_tpk_video_evento(5, (int)0xFE6C0026u, 0);   /* erro na sessao nova, ainda na janela */
  bombear(2);
  ok(video_reconectando() > 0, "E the error on the fresh session reconnects");
  ok(video_tpk_reconexao_preparo() == 1, "E an error just after the local reopen is preparation");
  video_parar();

  printf(falhas ? "tpk-seek-segurado: %d FAIL(S)\n" : "tpk-seek-segurado: ok\n", falhas);
  return falhas != 0;
}
