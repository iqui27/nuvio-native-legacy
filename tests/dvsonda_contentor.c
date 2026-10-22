// A SONDA DO MKV QUE LEU UM ARQUIVO QUE NAO E MATROSKA (C9, 2.0.3, build
// e5b54bc4, "Dolby Vision em MKV" ligado). Deadpool & Wolverine pelo
// AIOStreams/ElfHosted: URL sem extensao, o cartao dizia MP4, a tela do DV
// entrou, a sonda leu o comeco do arquivo — que nao tinha a assinatura EBML —
// e tratou isso como falha de rede: 3 novas tentativas (3/8/20 s) e a tela de
// pe ~50 s num arquivo que nunca seria MKV.
//
// O que protege:
//   * bytes que chegaram e sao de outro contentor (MP4 "ftyp" no byte 4,
//     MPEG-TS 0x47 a cada 188 bytes) sao resposta definitiva: sem nova
//     tentativa, o DV desta fonte acaba ali, o HDR10 volta ao ponto de partida
//     e a tela sai no PROXIMO quadro (< 1 s depois da leitura), sem a nota de
//     "nao deu para ler";
//   * a falha de rede de verdade (nada chegou) continua com as novas tentativas.
// Compila o ramo LS2 real de src/video.c (como dvretry_lg); a rede da sonda e
// dublada: o que entra e o que mkv_faixas_e_caps teria visto.
#include "video.h"
#include "video_escala.h"
#include "video_reconexao.h"
#include "idioma.h"
#include "linguas.h"
#include "marco.h"
#include "mkv.h"
#include "mkvass.h"
#include "js.h"
#include "lsregistro.h"
#include "rede.h"
#include "dvtela.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include "dts/dts_playback.h"
#include "ajustes.h"
#include "legenda.h"
#include "dados.h"
#include "dts/dts_engine.h"
#include "dts/dts_overlay.h"

// Valores de mkv.h (MKV_CONT_*). Repetidos aqui so para o teste compilar contra
// a arvore anterior ao conserto e FALHAR pela asserção, nao pelo compilador.
#ifndef MKV_CONT_NADA
#define MKV_CONT_NADA 0
#define MKV_CONT_MP4  2
#define MKV_CONT_TS   3
#endif

// A rede da sonda: o que o servidor devolveu no Range 0-327679.
static int contentorVisto = MKV_CONT_NADA, sondas;
static int dubleCab(const char *url, unsigned char **buf, long *n) {
  (void)url; if (buf) *buf = NULL; if (n) *n = 0; return 0;   // sem pre-busca
}
static int dubleFaixas(const char *url, MkvFaixa *saida, int max, MkvCap *caps, int maxCaps,
                       int *nCaps) {
  (void)url; (void)saida; (void)max; (void)caps; (void)maxCaps;
  if (nCaps) *nCaps = 0;
  sondas++;
  return 0;                       // nenhuma faixa: MP4, TS ou rede, tanto faz aqui
}
static int dubleContentor(void) { return contentorVisto; }
#define mkvass_cabecalho dubleCab
#define mkv_faixas_e_caps dubleFaixas
#define mkv_ultimo_contentor dubleContentor
#define dts_playback_start dublePartida
#define dts_playback_close dubleFechar
static DtsPlayback *dublePartida(const char *url, const char *headers, int audio_stream,
                                 double position, int paused, const char *window,
                                 int webos_major, const DtsTrack *selected, int ordinal,
                                 int count) {
  (void)url; (void)headers; (void)audio_stream; (void)position; (void)paused; (void)window;
  (void)webos_major; (void)selected; (void)ordinal; (void)count;
  return (DtsPlayback *)(uintptr_t)0x1;
}
static void dubleFechar(DtsPlayback *p) { (void)p; }
#undef __APPLE__
#include "../src/video.c"

static int fakeCall(LSHandle *h, const char *uri, const char *carga, Filtro cb,
                    void *ctx, unsigned long *tok, void *erro) {
  (void)h; (void)uri; (void)carga; (void)cb; (void)ctx; (void)tok; (void)erro;
  return 1;
}
static const char *fakePayload(LSMessage *m) { return (const char *)m; }
static void evento(const char *p) {
  aoEvento(NULL, (LSMessage *)p, (void *)(uintptr_t)sessao);
}

// A fonte do relato: URL sem extensao, anunciada DV, nao anunciada MP4; a tela
// de pe, a retomada pediu 600 s e o HDR10 andou ate 612 s por baixo dela.
static void fonteDoRelato(void) {
  dtsSessao = NULL; dtsModoDv = 0; dtsTentou = 0; dvAudioMkvOrd = -1;
  ligado = pronto = tocando = 1;
  snprintf(midia, sizeof midia, "fixture-hdr10");
  snprintf(urlAtual, sizeof urlAtual, "https://addon.invalid/playback/abc");
  fonteMp4 = 0;
  dvHabilitado = 1; dvNao = 0; dvSondado = 0; dvMkvPerfil = 0; dvAudios = 0;
  nv_dvsonda_zerar(&mkvRetry); mkvRetryEm = 0;
  mkvFioSessao = sessao; fioMkvVivo = 1; sondas = 0;
  posSeg = 0.0;
  video_dv_tela(1);
  video_buscar(600.0);
  seekEm = seekEnvEm = seekRetryEm = 0;
  evento("{\"currentTime\":612000}");
  assert(posSeg > 611.9 && posSeg < 612.1);
}

// A tela de verdade (dvtela.c) recebendo os sinais que o player monta de
// video_dv_fase: devolve em quantos ms depois da leitura ela saiu (-1 = nao saiu
// em 1 s).
static int telaSaiEm(Uint32 leitura, int *saida) {
  DvtelaEstado e; DvtelaSinais s; VideoDvFase f; Uint32 t;
  memset(&e, 0, sizeof e);
  dvt_entrar(&e, leitura - 2000);
  for (t = leitura; t <= leitura + 1000; t += 16) {
    video_dv_fase(&f);
    memset(&s, 0, sizeof s);
    s.sessao = f.sessao; s.sondado = f.sondado; s.perfil = f.perfil; s.recusa = f.recusa;
    s.caminho = f.caminho; s.carregado = f.carregado; s.tocando = f.tocando;
    s.dvConfirmado = f.dvConfirmado; s.falhou = f.falhou;
    if ((*saida = dvt_passo(&e, &s, t)) != 0) return (int)(t - leitura);
    // o quadro do video: e aqui que o dvPronto decide (video_atualizar)
    if (dvHabilitado && !dvSondado) continue;
    if (dvHabilitado) (void)dvPronto();
  }
  return -1;
}

static void definitivo(int contentor, const char *nome) {
  int saida = 0, ms;
  fonteDoRelato();
  contentorVisto = contentor;
  lerMkv(NULL);
  assert(sondas == 1);
  // Sem nova tentativa: nada agendado, nenhuma falha contada.
  if (mkvRetryEm != 0 || mkvRetry.falhas != 0) {
    fprintf(stderr, "FALHOU: %s tratado como falha de rede (tentativa %d agendada)\n", nome,
            mkvRetry.falhas);
    exit(1);
  }
  assert(dvSondado == 1 && !fioMkvVivo);
  ms = telaSaiEm(1000, &saida);
  assert(ms >= 0 && ms < 1000);
  assert(saida == DVT_SAIDA_RECUSA);
  // O motivo nao e "a sonda desistiu" (a nota "Nao deu para ler o arquivo").
  assert(dvNao != VIDEO_DV_NAO_NADA && dvNao != VIDEO_DV_NAO_SONDA);
  // O HDR10 que andou por baixo da tela volta ao ponto de partida.
  assert(seekEm && seekAlvo > 599.9 && seekAlvo < 600.1);
  printf("dvsonda_contentor: %s -> sem nova tentativa, tela saiu em %d ms, volta a 600 s\n",
         nome, ms);
}

int main(void) {
  lsCall = fakeCall; lsPayload = fakePayload;
  bus = (LSHandle *)(uintptr_t)1; sessao = 41;

  // 1. MP4 ("ftyp" no byte 4): o arquivo do relato.
  definitivo(MKV_CONT_MP4, "MP4 (ftyp)");
  // 2. MPEG-TS (0x47 a cada 188 bytes).
  definitivo(MKV_CONT_TS, "MPEG-TS (0x47)");

  // 3. Falha de rede de verdade (nada chegou): continua com as novas tentativas.
  fonteDoRelato();
  contentorVisto = MKV_CONT_NADA;
  lerMkv(NULL);
  assert(sondas == 1);
  assert(mkvRetryEm != 0 && mkvRetry.falhas == 1 && !mkvRetry.desistiu);
  assert(dvSondado == 0 && dvNao == VIDEO_DV_NAO_NADA);
  puts("dvsonda_contentor: rede sem resposta -> nova tentativa 1/3 agendada, tela fica");
  return 0;
}
