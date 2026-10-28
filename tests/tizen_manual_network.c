// Execute the actual manual-open helpers and network-health implementation.
#include "streams.h"
#include "redesaude.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned Uint32;
static int fonteVODRetentou, vodRenovou, aguardandoFonte, preparoEspera;
static Uint32 fonteManualDesde;
static int delivered;
static void novaGeracaoFonte(void) {}
static void limparFontePendente(void) {}
static void limparFonteVOD(void) {}
static void provaZerar(void) {}
static void video_definir_dv(int value) { (void)value; }
static void video_definir_cabecalhos(const char *value) { (void)value; }
static void video_definir_mp4(int value) { (void)value; }
int stream_e_mp4(const Stream *s) { return s->mp4; }
Uint32 SDL_GetTicks(void) { return 1; }
static void player_definir_fonte(const char *url) {
  assert(!strcmp(url, "https://stream.example.invalid/video.mkv"));
  delivered++;
}
#include "manual.inc"
int main(void) {
  Stream s = {0};
  strcpy(s.url, "https://stream.example.invalid/video.mkv");
  abrirFonteManualDireta(&s);
  assert(delivered == 1 && fonteManualDesde);
  rede_saude_nota(0, "https://metadata.example.invalid/meta");
  for (int i = 0; i < 8; i++)
    rede_saude_nota_desde(6, s.url, 1);
  assert(!rede_saude_offline());
  rede_saude_nota(0, "https://subtitle.example.invalid/sub");
  assert(!rede_saude_offline());
  // Also advance the pure clock past the latch window without real sleeps.
  RedeSaude health = {0};
  for (unsigned t = 1; t < 20000; t += 1000) {
    rede_saude_passo(&health, 0, 11, t);
    assert(!health.offline);
  }
  rede_saude_passo(&health, 0, 22, 20001);
  assert(health.offline); // a real multi-host outage still counts
  puts("PASS: production manual handoff and distinct-host health guard");
}
