#include "player_dts_notice.h"
#include "plrilha.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PG_SEG_TOTAL 6.0f
static NvDtsNotice truehdConvertedNotice;
static Uint32 truehdDesde,pgDesde;
static int truehdTrack=-1;
typedef struct { int stream_index; } VideoFaixa;
static VideoFaixa selected={18};
static int trackReady=1;
int video_audio_atual(void) { return 0; }
const VideoFaixa *video_audio(int n) { (void)n;return trackReady?&selected:NULL; }
static int truehdVisto,loading,channel,converted,playing=1,starting,calls;
static char message[256];
int player_carregando(void) { return loading; }
static int ehCanal(void) { return channel; }
int video_truehd_convertido(void) { return converted; }
int video_tocando(void) { return playing; }
int video_iniciando(void) { return starting; }
const char *i18n(const char *s) { (void)s;return "Dźwięk DTS jest konwertowany na tym TV"; }
void plrilha_pedir(const PlrIlhaPedido *p) {
  assert(!strcmp(p->icone,"aj_audio-lines") && p->semFim && p->aberta);
  snprintf(message,sizeof message,"%s",p->texto);calls++;
}
#include "truehd-render.c"
int main(void) {
  converted=1;loading=1;avisarTruehd(100);avisarTruehd(3000);assert(!calls);
  loading=0;starting=1;avisarTruehd(4000);assert(!calls);
  starting=0;avisarTruehd(5000);avisarTruehd(6499);assert(!calls);
  avisarTruehd(6500);assert(calls==1);
  assert(!strcmp(message,"Dźwięk TrueHD jest konwertowany na tym TV"));
  avisarTruehd(11499);assert(calls==2);avisarTruehd(11500);assert(calls==2);
  for(int i=0;i<5;i++) {
    loading=1;playing=0;avisarTruehd(12000+i*10000);
    loading=0;playing=1;avisarTruehd(14000+i*10000);avisarTruehd(16000+i*10000);
    converted=0;avisarTruehd(17000+i*10000);converted=1;
  }
  assert(calls==2); // Same track: seeks/loading/reloads do not repeat it.
  selected.stream_index=12;converted=0;avisarTruehd(65000);
  selected.stream_index=18;converted=1;avisarTruehd(66000);avisarTruehd(67500);
  assert(calls==3); // Returning to TrueHD announces the new conversion.
  selected.stream_index=19;avisarTruehd(73000);avisarTruehd(74500);
  assert(calls==4); // A different TrueHD track also announces once ready.
  trackReady=0;loading=1;avisarTruehd(80000);trackReady=1;loading=0;
  avisarTruehd(82000);avisarTruehd(84000);assert(calls==4);
  calls=2;
  truehdConvertedNotice=(NvDtsNotice){0};truehdVisto=0;truehdDesde=0;truehdTrack=-1;
  channel=1;avisarTruehd(70000);channel=0;pgDesde=71000;
  avisarTruehd(72000);avisarTruehd(76000);assert(calls==2);
  pgDesde=0;avisarTruehd(77499);assert(calls==2);
  avisarTruehd(77500);assert(calls==3); // A new source may announce after the parental card.
  puts("TrueHD notice: localized actual renderer, selected conversion only, startup/bind/parental gates and once per selected track, repeat on return, no repeat for seeks/reloads passed");
}
