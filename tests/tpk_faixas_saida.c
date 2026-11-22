// TRACK STATE AFTER BACK ON THE .tpk (PR 734, parte A): the host keeps
// sending track lists and MKV events after the player closes. Before the fix
// video_parar left the episode's lists behind and the callbacks still ran:
// the next video_bombear re-did the MKV matching and the audio re-pick over a
// closed player's list ("[mkv] contagem" after Voltar). Now video_parar clears
// the whole track state (lists, MKV header, generation++) and nv_tpk_video_faixa
// / nv_tpk_video_faixas_fim / sondaMkv do nothing while the player is closed.
//
// Sem a correcao este teste falha: a lista tardia repovoa nAudio/nLeg.
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include "nvtmp.h"

// Mesmas fixtures de tpk-escolha.c: sem rede, sem TorBox.
int debrid_tb_link(int usenet, int item, int file, char *dst, unsigned n) {
  (void)usenet; (void)item; (void)file;
  if (n) dst[0] = 0;
  return 0;
}
const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { (void)c; (void)p; return 0; }
const char *ling_audio(void) { return ""; }
const char *ling_audio2(void) { return ""; }
const char *ling_original(void) { return ""; }
int ling_tipo_legenda(const char *s, int f, int d) { (void)s; (void)f; (void)d; return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t; return ""; }
const char *ling_do_nome(const char *nome) { (void)nome; return NULL; }
int ling_letreiro(const char *nome, int forcado) { (void)nome; (void)forcado; return 0; }
void mkvass_aceitar_texto(int sim) { (void)sim; }
void mkvass_aceitar_imagem(int sim) { (void)sim; }

void nv_tpk_video_registrar(void (*)(const char *, const char *), void (*)(void), void (*)(int),
                            void (*)(int), void (*)(int), void (*)(int, int, int, int), int (*)(void));
void nv_tpk_video_registrar_faixas(void (*)(int, int));

static void fxAbrir(const char *u, const char *c) { (void)u; (void)c; }
static void fxSem(void) {}
static void fxInt(int v) { (void)v; }
static void fxJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static int  fxPos(void) { return 0; }
static void fxEscolher(int tipo, int idx) { (void)tipo; (void)idx; }

// video_tpk.c inteiro no TU de teste: os estaticos (ativo, nAudio, mkvN,
// mkvGeracao...) sao o objeto da prova — a saida zera tudo isso.
#include "../src/video_tpk.c"

int main(void) {
  unsigned g0;
  nv_tpk_video_registrar(fxAbrir, fxSem, fxInt, fxInt, fxInt, fxJanela, fxPos);
  nv_tpk_video_registrar_faixas(fxEscolher);

  video_tocar("http://x/episodio.mkv");
  nv_tpk_video_evento(6, 1920, 1080);
  nv_tpk_video_evento(1, 100000, 0);
  assert(ativo);
  nv_tpk_video_faixa(0, 0, "ko");
  nv_tpk_video_faixa(1, 0, "ru");
  nv_tpk_video_faixa(1, 1, "en");
  nv_tpk_video_faixas_fim(0, 0);
  assert(nAudio == 1 && nLeg == 2 && faixasLidas);
  video_bombear();
  g0 = mkvGeracao;

  // VOLTAR: a sessao fecha e o estado das faixas vai com ela.
  video_parar();
  assert(!ativo && !pronto);
  assert(nAudio == 0 && nLeg == 0 && audioAtual == 0 && legAtual == -1);
  assert(nNovasA == 0 && nNovasL == 0);
  assert(mkvN == 0 && mkvEstado == 0 && !faixasNovas && !faixasLidas);
  assert(mkvGeracao == g0 + 1);   // fioMkv ainda na rede descarta o que trouxer

  // O host manda a lista do player que JA FECHOU (callback tardio): nao pode
  // repovoar o menu nem marcar faixas novas — e o tipo 2 (codec) continua so
  // informativo, nao abre lista.
  nv_tpk_video_faixa(0, 0, "en");
  nv_tpk_video_faixa(0, 1, "pt");
  nv_tpk_video_faixa(1, 0, "en");
  nv_tpk_video_faixa(3, 0, "6|eac3");
  nv_tpk_video_faixa(2, 0, "hevc");
  nv_tpk_video_faixas_fim(0, 0);
  assert(nNovasA == 0 && nNovasL == 0);          // nada pendente da lista tardia
  assert(nAudio == 0 && nLeg == 0);
  assert(audioAtual == 0 && legAtual == -1);
  assert(!faixasNovas && !faixasLidas);

  // E o quadro que roda depois do Voltar nao refaz casamento nem ordem de
  // audio: sondaMkv sai na porta (!ativo) e nada volta.
  video_bombear(); video_bombear();
  assert(nAudio == 0 && nLeg == 0 && mkvN == 0 && !faixasLidas);
  assert(mkvGeracao == g0 + 1);

  // O titulo seguinte abre limpo, numa geracao nova.
  video_tocar("http://x/proximo.mkv");
  nv_tpk_video_evento(1, 100000, 0);
  assert(ativo && mkvGeracao == g0 + 2);
  nv_tpk_video_faixa(0, 0, "ja");
  nv_tpk_video_faixas_fim(0, -1);
  assert(nAudio == 1 && nLeg == 0 && faixasLidas);

  puts("tpk faixas saida: video_parar leva as faixas e a geracao; "
       "lista tardia e sonda com player fechado nao repovoam nada");
  return 0;
}
