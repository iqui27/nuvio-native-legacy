// Fim real durante a reabertura do seek no .tpk (F80): EV_BUSCOU confirmado e
// EV_FIM na sequencia tem de encerrar o "reabrindo" na hora — sem isso a
// posicao segurada prendia a parte em "busca em voo" para sempre e nem o EOS
// confirmado avancava (o retorno cedo do video_bombear barrava ate o limite
// de 60 s do reabrindoVigiar). Backend de producao (src/video_tpk.c) com host
// e relogio simulados, sem rede — o mesmo harness de tests/tpk_seek_segurado.c.
// A decisao de fim e a parteNoFim do app.c, extraida pelo .sh para fim.c.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
static void pronto(int dur, int nAud, int nLeg) {
  int i;
  nv_tpk_video_evento(1, dur, 0);
  for (i = 0; i < nAud; i++) nv_tpk_video_faixa(0, i, "eng");
  for (i = 0; i < nLeg; i++) nv_tpk_video_faixa(1, i, i ? "por" : "eng");
  nv_tpk_video_faixas_fim(nAud ? 0 : -1, nLeg ? 0 : -1);
}

// Relogio simulado: tudo (reabrindo, vigias, parteNoFim) le o mesmo tempo.
static Uint32 relogio = 1000u;
Uint32 SDL_GetTicks(void) { return relogio; }
void SDL_Delay(Uint32 ms) { relogio += ms; }

// O que a parteNoFim extraida consulta alem do backend real.
static int player_buscando(void) { return 0; }
static int player_pausa_pessoa(void) { return 0; }
static double parteFimPos = -1;
static Uint32 parteFimDesde;
static const char *parteFimComo;
#include "fim.c"

int main(void) {
  nv_tpk_video_registrar(hAbrir, hSem, hInt, hBuscarT, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_iniciar_auto();
  video_definir_reconexao(1);
  video_definir_mp4(0);
  video_tocar("http://fixture.invalid/part.mkv");
  posMs = 1498000;
  pronto(1500000, 1, 0);
  nv_tpk_video_evento(2, 0, 0);            /* EV_TOCANDO */
  video_bombear();
  video_buscar(1499.8);                    /* seek para os ultimos 0,2 s */
  mkvfix_mp4_stub_ocultas(1);
  mkvfix_mp4_stub_estado(MKVFIX_PRECISA);
  video_bombear();
  ok(nAbrirMkv == 1 && video_busca_em_voo(),
     "o seek no fim reabre pelo servidor local e segura a posicao");
  posMs = 0;
  pronto(1500000, 1, 0);
  nv_tpk_video_evento(2, 0, 0);
  video_bombear();
  ok(nBuscar == 1 && ultBuscarMs == 1499800,
     "a reabertura leva o seek do alvo para a sessao nova");
  posMs = ultBuscarMs;
  nv_tpk_video_evento(11, ultBuscarMs, 0);  /* EV_BUSCOU: seek confirmado */
  nv_tpk_video_evento(4, 0, 0);             /* EV_FIM: fim real antes do proximo quadro */
  video_bombear();
  ok(!video_busca_em_voo(), "o fim real encerra a reabertura e solta o avanco");
  ok(video_terminou() && video_pos() >= video_duracao(),
     "depois do EOS a posicao e o fim do arquivo");
  relogio += 61000;
  video_bombear();                          /* o limite de 60 s corre tambem depois do EOS */
  ok(!video_busca_em_voo(), "segue solto depois dos 60 s de guarda");
  ok(parteNoFim(video_duracao(), video_pos()),
     "a parte avanca no fim real depois do seek confirmado");
  video_parar();

  printf(falhas ? "partes-fim-eof-tpk: %d FAIL(S)\n" : "partes-fim-eof-tpk: ok\n", falhas);
  return falhas != 0;
}
