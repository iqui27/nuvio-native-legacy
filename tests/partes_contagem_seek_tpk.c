// Contagem de fim de parte durante a reabertura do seek no .tpk (F80): a
// troca que a contagem dispara ao vencer sozinha tem de esperar a busca
// assentar — a posicao que o host devolve e a de antes do pedido, e trocar de
// arquivo no meio do seek perdia as duas coisas. O "Assistir agora" da pessoa
// continua imediato, e sem busca no ar o vencimento troca como sempre.
// Backend de producao (src/video_tpk.c + src/partescartao.c) com host e
// relogio simulados; a decisao e o trecho de avanco do app.c (parteNoFim e o
// bloco dos creditos), extraidos pelo .sh para fim.c e avanco.c.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "partescartao.h"
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

// Relogio simulado: backend, contagem e decisao leem o mesmo tempo.
static Uint32 agora = 1000u;
Uint32 SDL_GetTicks(void) { return agora; }
void SDL_Delay(Uint32 ms) { agora += ms; }
float anim_mola(float a, float b, float dt, float mola) {
  (void)a; (void)dt; (void)mola; return b;
}

// O que o trecho do app.c consulta fora do backend real.
static int player_buscando(void) { return 0; }
static int player_carregando(void) { return !video_pronto(); }
static int player_pausa_pessoa(void) { return 0; }
static double parteCreditos(double d) { (void)d; return 1490; }
static int ajustes_proximo_contagem(void) { return 10; }
int partes_numero(const PartesLista *l, int k) { (void)l; return k + 1; }
int partes_principais(const PartesLista *l) { (void)l; return 2; }
void partes_link_pedir(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
static void parteFalhou(const char *s) { (void)s; }

// O estado que o trecho extraido espera do app.c.
static int trocou, parteForcar, parteContagemFeita;
static double parteFimPos = -1;
static Uint32 parteFimDesde;
static const char *parteFimComo;
static int parteProxFase = 2;             // link da seguinte ja na mao
static Uint32 parteProxDesde;
static char parteProxUrl[] = "fixture";
static PartesLista lista;

static void parteTrocarPara(const PartesLista *l, const char *a, int k, const char *u, double pos) {
  (void)l; (void)a; (void)k; (void)u; (void)pos;
  trocou++;
  // Como a de verdade: a parte nova zera o forcado e fecha o cartao.
  parteForcar = 0; parteContagemFeita = 0;
  partescartao_fechar();
}

#include "fim.c"
#include "avanco.c"

static void reabrirEmSeek(double alvo) {
  // Posicao nos creditos, seek para tras: a sonda do mkvfix manda reabrir pelo
  // servidor local e o alvo sai quando a sessao nova tocar.
  posMs = 1498000;
  pronto(1500000, 1, 0);
  nv_tpk_video_evento(2, 0, 0);
  video_bombear();
  video_buscar(alvo);
  mkvfix_mp4_stub_ocultas(1);
  mkvfix_mp4_stub_estado(MKVFIX_PRECISA);
  video_bombear();
  posMs = 0;
  pronto(1500000, 1, 0);
  video_bombear();
}

static void assentarSeek(double alvo) {
  // Tocando, o seek do reabrindo sai ao host; o ack e o quadro no alvo soltam
  // o "reabrindo".
  posMs = 0;
  nv_tpk_video_evento(2, 0, 0);
  video_bombear();
  posMs = (int)(alvo * 1000.0);
  if (nBuscar) nv_tpk_video_evento(11, ultBuscarMs, 0);
  video_bombear();
}

static void cenarioReset(void) {
  trocou = nBuscar = nAbrirMkv = 0; ultBuscarMs = -1;
  parteForcar = parteContagemFeita = 0;
  parteFimPos = -1; parteFimComo = "";
  parteProxFase = 2;
  partescartao_zerar();
  mkvfix_mp4_stub_ocultas(0);
  mkvfix_mp4_stub_estado(MKVFIX_NADA);
  // A permissao de reconexao e consumida pelo video_tocar: repete a cada sessao.
  video_definir_reconexao(1);
  video_definir_mp4(0);
}

int main(void) {
  nv_tpk_video_registrar(hAbrir, hSem, hInt, hBuscarT, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_iniciar_auto();

  /* A: a contagem vencida com a reabertura no ar espera a busca assentar. */
  cenarioReset();
  video_tocar("http://fixture.invalid/part.mkv");
  reabrirEmSeek(1496.0);
  ok(nAbrirMkv == 1 && video_busca_em_voo() && video_pos() == 1496.0,
     "A o seek nos creditos reabre pelo servidor local e segura a posicao");
  frame(video_duracao(), video_pos());
  ok(partescartao_modo_contagem(), "A a contagem abre nos creditos");
  agora += 10001;
  video_bombear();
  partescartao_atualizar(0.016f, agora);    /* vence sozinha, com a busca no ar */
  frame(video_duracao(), video_pos());
  ok(trocou == 0, "A a contagem vencida nao troca de arquivo durante a busca");
  assentarSeek(1496.0);
  ok(!video_busca_em_voo(), "A a busca assentou no alvo");
  frame(video_duracao(), video_pos());
  ok(trocou == 1, "A a troca adiada sai quando a busca assenta");
  video_parar();

  /* B: "Assistir agora" da pessoa troca na hora, mesmo com a busca no ar. */
  cenarioReset();
  video_tocar("http://fixture.invalid/part.mkv");
  reabrirEmSeek(1496.0);
  frame(video_duracao(), video_pos());
  ok(partescartao_modo_contagem(), "B a contagem abre nos creditos");
  ok(video_busca_em_voo(), "B a reabertura segue no ar");
  { SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_RETURN;        /* ENTER em "Assistir agora" */
    partescartao_evento(&ev); }
  frame(video_duracao(), video_pos());
  ok(trocou == 1, "B o pedido da pessoa troca na hora durante a busca");
  video_parar();

  /* C: sem busca no ar a contagem vencida troca como sempre. */
  cenarioReset();
  video_tocar("http://fixture.invalid/part.mkv");
  posMs = 1496000;
  pronto(1500000, 1, 0);
  nv_tpk_video_evento(2, 0, 0);
  video_bombear();
  mkvfix_mp4_stub_estado(MKVFIX_NORMAL);
  frame(video_duracao(), video_pos());
  ok(partescartao_modo_contagem(), "C a contagem abre nos creditos");
  agora += 10001;
  video_bombear();
  partescartao_atualizar(0.016f, agora);
  frame(video_duracao(), video_pos());
  ok(trocou == 1, "C sem busca a contagem vencida troca");
  video_parar();

  printf(falhas ? "partes-contagem-seek-tpk: %d FAIL(S)\n" : "partes-contagem-seek-tpk: ok\n", falhas);
  return falhas != 0;
}
