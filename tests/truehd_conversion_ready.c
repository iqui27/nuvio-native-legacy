#include "dts/dts_playback.h"
#include "dts/dts_audio_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { char codec[24];int stream_index; } VideoFaixa;
static VideoFaixa selected={"truehd",18};
static DtsPlayback *dtsSessao=(DtsPlayback *)1;
static int dtsModoDv=1;
static char dtsSaida[]="TrueHD → AAC";
static DtsPlaybackStatus status;
int video_audio_atual(void) { return 0; }
const VideoFaixa *video_audio(int i) { (void)i;return &selected; }
int video_falhou(void) { return 0; }
int video_terminou(void) { return 0; }
void dts_playback_status(DtsPlayback *p,DtsPlaybackStatus *out) { (void)p;*out=status; }
#include "truehd-ready.c"
int main(void) {
  status.prepared=1;status.info.audio_stream=18;assert(video_truehd_convertido());
  selected.stream_index=19;assert(!video_truehd_convertido());
  status.info.audio_stream=19;status.prepared=0;assert(!video_truehd_convertido());
  status.prepared=1;assert(video_truehd_convertido());
  status.failed=1;assert(!video_truehd_convertido());
  puts("PASS TrueHD readiness: previous track conversion cannot announce a pending new track");
}
