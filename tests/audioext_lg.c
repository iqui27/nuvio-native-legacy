// AUDIO FROM ANOTHER VERSION, real webOS plumbing (src/video.c LS2 branch):
// the marked source reaches the converter at start, a donor failure and the
// setting turned off give the file's own audio back, a profile change and a
// stop retire the donor and the track read. Converter and bus are dubbed.
// Adapted from Mateusz (Matis94) external-audio-test-2.0.8.7.
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
#include "audioext.h"

#define dts_playback_start dublePartida
#define dts_playback_close dubleFechar
#define dts_playback_status dubleStatus
#define dts_playback_event dubleEvento
#define dts_playback_next_external_audio dubleExterno
static int partidas, fechadas;
static char externoUrl[256];
static int externoOffset;
static DtsPlaybackStatus status;
static DtsPlayback *dublePartida(const char *url, const char *headers, int audio_stream,
                                 double position, int paused, const char *window,
                                 int webos_major, const DtsTrack *selected, int ordinal,
                                 int count) {
  (void)url; (void)headers; (void)audio_stream; (void)position; (void)paused; (void)window;
  (void)webos_major; (void)selected; (void)ordinal; (void)count;
  partidas++;
  return (DtsPlayback *)(uintptr_t)0x1;
}
static void dubleFechar(DtsPlayback *p) { (void)p; fechadas++; }
static void dubleStatus(DtsPlayback *p, DtsPlaybackStatus *st) { (void)p; *st = status; }
static int dubleEvento(DtsPlayback *p, char *out, size_t n) { (void)p; (void)out; (void)n; return 0; }
static void dubleExterno(const char *url, const char *headers, const char *langs, int offset_ms) {
  (void)headers; (void)langs;
  snprintf(externoUrl, sizeof externoUrl, "%s", url ? url : "");
  externoOffset = offset_ms;
}
#undef __APPLE__
#include "../src/video.c"

static int fakeCall(LSHandle *h, const char *uri, const char *carga, Filtro cb,
                    void *ctx, unsigned long *tok, void *erro) {
  (void)h; (void)uri; (void)carga; (void)cb; (void)ctx; (void)tok; (void)erro;
  return 1;
}
static const char *fakePayload(LSMessage *m) { return (const char *)m; }

#define VIDEO "http://h/remux.mkv"
#define DOADOR "http://h/webdl.dub.mkv"

// The picture source plays on the TV player; the person marked a dub version.
static void montar(void) {
  dtsSessao = NULL; dtsModoDv = 0; dtsTentou = 0; extAtivo = 0; extSoAudio = 0;
  ligado = pronto = tocando = 1; falhou = terminou = 0; modoLoad = 0;
  snprintf(midia, sizeof midia, "fixture");
  snprintf(urlAtual, sizeof urlAtual, VIDEO);
  posSeg = 1234.5;
  memset(&status, 0, sizeof status);
  partidas = fechadas = 0; externoUrl[0] = 0;
  audioext_alvo("tt1:1:2");
  if (!audioext_marcada(DOADOR)) assert(audioext_alternar(DOADOR, "", "WEB-DL Dub") == 1);
  audioext_offset_definir(-2500);
}

// A track read that only ends when cancelled (a slow donor header).
static int lendo;
static int sondaLenta(const char *url, const char *cab, AudioExtFaixa *out, int max, const int *cancel) {
  (void)url; (void)cab; (void)out; (void)max;
  lendo = 1;
  while (!__atomic_load_n(cancel, __ATOMIC_ACQUIRE)) usleep(1000);
  return -1;
}

int main(void) {
  lsCall = fakeCall; lsPayload = fakePayload;
  bus = (LSHandle *)(uintptr_t)1; sessao = 40;
  audioext_perfil("1:0");

  // 1. The menu choice while the TV player plays: the converter opens at the
  //    picture's position with the donor's audio and the kept offset.
  montar();
  extSoAudio = 1;
  assert(iniciarDts(-1) && partidas == 1);
  assert(extAtivo && !strcmp(externoUrl, DOADOR) && externoOffset == -2500);
  assert(audioext_carregando());
  puts("audioext_lg: marked version reaches the converter with its offset");

  // 2. The donor fails (ends, slow, unreadable): own audio, mark gone, said once.
  status.failed = 1;
  snprintf(status.error, sizeof status.error, "%s", DTS_PLAYBACK_STARVED);
  bombearDts();
  assert(!extAtivo && !audioext_tem_marca() && !audioext_tocando() && !audioext_carregando());
  assert(!falhou && dtsEstado == 0 && !dtsModoDv);
  { char m[160]; assert(audioext_falha_consumir(m, sizeof m) && !audioext_falha_consumir(m, sizeof m)); }
  puts("audioext_lg: donor failure restores the file's own audio");

  // 3. The next converter start for the same title carries no donor.
  memset(&status, 0, sizeof status);
  dtsSessao = NULL; partidas = 0; snprintf(externoUrl, sizeof externoUrl, "stale");
  assert(iniciarDts(-1) && partidas == 1 && !extAtivo && !externoUrl[0]);
  puts("audioext_lg: next start plays own audio only");

  // 4. Settings > Audio from another version turned off while the donor plays
  //    on a Dolby Vision session: the converter reopens with its own audio.
  montar();
  dtsModoDv = 1;
  assert(iniciarDts(-1) && extAtivo);
  audioext_tocando_definir(1);
  audioext_limpar();                 // player_atualizar: setting off
  extReabrirPend = 1; partidas = 0; snprintf(externoUrl, sizeof externoUrl, "stale");
  extReabrir();
  assert(partidas == 1 && !extAtivo && !externoUrl[0] && fechadas >= 1);
  puts("audioext_lg: disable reopens the converter on the file's own audio");

  // 5. Another profile while the donor plays: the mark and the session's
  //    donor are retired; the converter reopens without it.
  montar();
  dtsModoDv = 1;
  assert(iniciarDts(-1) && extAtivo);
  { unsigned g = audioext_pedido_geracao();
    audioext_perfil("2:0");
    assert(audioext_pedido_geracao() != g && !audioext_tem_marca()); }
  extReabrirPend = 1; partidas = 0; snprintf(externoUrl, sizeof externoUrl, "stale");
  extReabrir();
  assert(partidas == 1 && !extAtivo && !externoUrl[0]);
  puts("audioext_lg: profile change retires the donor");

  // 6. Stop while a donor's header is being read: the worker is cancelled,
  //    joined, and its result never published.
  montar();
  audioext_definir_sonda_segura(sondaLenta);
  { unsigned ger = audioext_sondar(DOADOR, "");
    AudioExtFaixa f[AUDIOEXT_FAIXAS_MAX];
    for (int i = 0; i < 2000 && !lendo; i++) usleep(1000);
    assert(lendo && audioext_sonda_ocupada());
    dtsSessao = (DtsPlayback *)(uintptr_t)0x1; extAtivo = 1;
    pararSessao();
    assert(!extAtivo && !audioext_tocando());
    for (int i = 0; i < 2000 && audioext_sonda_ocupada(); i++) usleep(1000);
    assert(!audioext_sonda_ocupada());
    assert(audioext_sonda_resultado(ger, f, AUDIOEXT_FAIXAS_MAX) < 0); }
  puts("audioext_lg: stop cancels the track read");
  return 0;
}
