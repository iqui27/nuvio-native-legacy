// A TROCA DE AUDIO DO DV (dvPronto + iniciarDts) com a lista da TV DIFERENTE da
// do MKV. audioAtual e o indice na lista da TV; dvAudioCodec/dvAudioIdioma sao
// as faixas do MKV na ordem do arquivo. A TV pode filtrar a lista (C9: uma faixa
// so), entao o indice de uma nao serve de indice na outra: a faixa atual da TV
// tem de ser localizada no MKV por idioma/codec/canais (como dts_playback.c
// prepare() faz). Sem localizacao segura, nao troca.
// Compila o ramo LS2 real de video.c; barramento e motor do DV dublados.
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
// Antes do #undef __APPLE__: gl_compat.h (via ajustes.h) escolhe o GL do Mac.
#include "ajustes.h"
#include "legenda.h"
#include "dados.h"
#include "dts/dts_engine.h"
#include "dts/dts_overlay.h"
// O motor do DV de verdade abriria rede e fios: aqui so registra a partida.
#define dts_playback_start dublePartida
#define dts_playback_close dubleFechar
#define dts_playback_next_resume_pct dubleResumePct
#define dts_playback_seek dubleSeek
#define dts_playback_audio dubleAudio
#define dts_playback_audio_at dubleAudioAt
static int seekCalls,audioCalls,chosenStream;
static void dubleSeek(DtsPlayback *p,double target) { assert(p && target>=0);seekCalls++; }
static void dubleAudio(DtsPlayback *p,int stream) { assert(p && stream>=0);audioCalls++;chosenStream=stream; }
static void dubleAudioAt(DtsPlayback *p,int stream,double target) {
  assert(target==745.286);dubleAudio(p,stream);
}
static DtsTrack partidaFaixa;
static double partidaPos, partidaPct;
static void dubleResumePct(double pct) { partidaPct=pct; }
static int partidas, partidaOrdinal, partidaContagem, partidaComFaixa,partidaStream,partidaPaused;
static DtsPlayback *dublePartida(const char *url, const char *headers, int audio_stream,
                                 double position, int paused, const char *window,
                                 int webos_major, const DtsTrack *selected, int ordinal,
                                 int count) {
  (void)url; (void)headers; (void)audio_stream; (void)paused; (void)window;
  partidaPos=position;partidaStream=audio_stream;partidaPaused=paused;
  (void)webos_major;
  partidas++; partidaOrdinal = ordinal; partidaContagem = count;
  partidaComFaixa = selected != NULL;
  memset(&partidaFaixa, 0, sizeof partidaFaixa);
  if (selected) partidaFaixa = *selected;
  return (DtsPlayback *)(uintptr_t)0x1;
}
static void dubleFechar(DtsPlayback *p) { (void)p; }
// Exercise an explicitly enabled DV path independently of stored/default UI
// preferences (upstream now defaults MKV DV to off).
static int fixtureMkvEnabled=1;
static int fixtureDvEnabled(void) { return fixtureMkvEnabled; }
static int fixtureP7Enabled(void) { return 1; }
#define ajustes_dv_mkv fixtureDvEnabled
#define ajustes_dv7 fixtureP7Enabled
#ifdef NV_MP4_BD_TEST
static int fixtureNextMp4Mode;
static void fixtureNextMp4(int enabled) {fixtureNextMp4Mode=enabled;}
#define dts_playback_next_mp4_test fixtureNextMp4
#endif
static int fixtureFixedPref(void) { return 2; }
static int partidaFixedAudio;
static void fixtureNextDvAudio(int mode) { partidaFixedAudio=mode; }
#ifdef NV_DV_FIXED_AUDIO_TEST
#define ajustes_dv_audio_fixo fixtureFixedPref
#define dts_playback_next_dv_audio fixtureNextDvAudio
#endif
#undef __APPLE__
#include "../src/video.c"

static int nativeSelectCalls,nativeSelectedIndex;
static int fakeCall(LSHandle *h, const char *uri, const char *carga, Filtro cb,
                    void *ctx, unsigned long *tok, void *erro) {
  (void)h; (void)cb; (void)ctx; (void)tok; (void)erro;
  if(strstr(uri,"/selectTrack") && strstr(carga,"\"type\":\"audio\"")) {
    nativeSelectCalls++;nativeSelectedIndex=(int)js_num(carga,NULL,"index",-1);
  }
  return 1;
}
static const char *fakePayload(LSMessage *m) { return (const char *)m; }
static void evento(const char *p) {
  aoEvento(NULL, (LSMessage *)p, (void *)(uintptr_t)sessao);
}


typedef struct { const char *codec, *idioma, *nome; } Mkv;
typedef struct { const char *idioma, *codec; int canais; } Tv;

static void montar(const Mkv *m, int nm, const Tv *t, int nt, int atual) {
  int i;
  dtsSessao = NULL; dtsModoDv = 0; dtsTentou = 0; dvAudioMkvOrd = dvAudioMkvAtual = -1; dvAudioDe[0] = 0;
#ifdef NV_NATIVE_SEEK_TEST
  converterResumeRequested=converterResumeUsed=0;
  converterResumeSeconds=converterResumePct=0;
  fonteTocou=0;
#endif
  ligado = pronto = tocando = 1;
  snprintf(midia, sizeof midia, "fixture-hdr10");
  snprintf(urlAtual, sizeof urlAtual, "http://h/remux.dv.mkv");
  dvHabilitado = 1; dvNao = 0; dvSondado = 1;
  dvMkvPerfil = nm ? 8 : 0; dvMkvBl = dvMkvRpu = nm ? 1 : 0; dvMkvEl = 0;
  snprintf(vidHdr,sizeof vidHdr,"none");
  dvMemOrd = -1;
  for (i = 0; i < nm; i++) {
    snprintf(dvAudioCodec[i], sizeof dvAudioCodec[0], "%s", m[i].codec);
    snprintf(dvAudioIdioma[i], sizeof dvAudioIdioma[0], "%s", m[i].idioma);
    snprintf(dvAudioNome[i], sizeof dvAudioNome[0], "%s", m[i].nome);
  }
  memset(dvAudioCanais, 0, sizeof dvAudioCanais);
  dvAudios = nm;
  memset(faixaAudio, 0, sizeof faixaAudio);
  for (i = 0; i < nt; i++) {
    snprintf(faixaAudio[i].idioma, sizeof faixaAudio[i].idioma, "%s", t[i].idioma);
    snprintf(faixaAudio[i].codec, sizeof faixaAudio[i].codec, "%s", t[i].codec);
    faixaAudio[i].canais = t[i].canais;
    faixaAudio[i].numero = i; faixaAudio[i].ordinalMkv = -1;
    faixaAudio[i].stream_index = faixaAudio[i].stream_id = -1;
  }
  nAudio = nt; audioAtual = atual;
}
// dvPronto decide; iniciarDts entrega ao motor. Devolve o idioma entregue.
static const char *abrir(void) {
  assert(dvPronto());
  dtsModoDv = 1; partidas = 0;
#ifdef NV_NATIVE_SEEK_TEST
  unsigned retiredSession=sessao;
  bufferandoDesde=12345;
#endif
  assert(iniciarDts(-1) && partidas == 1);
#ifdef NV_DV_FIXED_AUDIO_TEST
  assert(partidaFixedAudio==2);
#endif
#ifdef NV_NATIVE_SEEK_TEST
  // The unloaded URI session cannot finish its old buffering event. Its stamp
  // must retire on DV handoff; the new converter still has its own preroll.
  assert(!video_pronto() && dtsSessao && dtsModoDv);
  assert(!bufferandoDesde && !video_bufferando_ms());
  eventoPayload("{\"bufferingStart\":{}}",retiredSession);
  assert(!video_bufferando_ms()); // Retired callbacks cannot revive the stamp.
  evento("{\"bufferingStart\":{}}");assert(video_bufferando_ms()>0);
  evento("{\"bufferingEnd\":{}}");assert(!video_bufferando_ms());
#endif
  dtsSessao = NULL;
  return partidaComFaixa ? partidaFaixa.language : "";
}

static void testarDtsOculto(void) {
  for(int profile=0;profile<=8;profile+=8)for(int pick=0;pick<2;pick++) {
    audioMkvNovaFonte();
    Mkv m[]={{"A_DTS","en","Original DTS-ES Surround Mix"},{"A_DTS","en","2026 Surround Remix"},
      {"A_FLAC","en","Commentary 1"},{"A_FLAC","en","Commentary 2"},
      {"A_FLAC","en","Commentary 3"},{"A_FLAC","en","Commentary 4"}};
    Tv tv[]={{"en","flac",2},{"en","flac",2},{"en","flac",2},{"en","flac",2}};
    montar(m,6,tv,4,0);dtsHabilitado=1;dvMkvPerfil=profile;
    dvMkvEl=0;dvMkvBl=dvMkvRpu=!!profile;semDVForcado=0;
    bindVivo=bindPendente=0;acb=0;modoLoad=0;seekEm=seekEnvEm=seekRetryEm=0;dvSilencio=0;
    posSeg=745.286;pausaPedida=pick;
    MkvFaixa fx[7]={0};fx[0].tipo=1;fx[0].numero=1;
    for(int i=0;i<6;i++) {
      fx[i+1].tipo=2;fx[i+1].numero=i+2;fx[i+1].canais=i<2?6:2;
      snprintf(fx[i+1].codec,sizeof fx[i+1].codec,"%s",m[i].codec);
      snprintf(fx[i+1].idioma,sizeof fx[i+1].idioma,"en");
      snprintf(fx[i+1].nome,sizeof fx[i+1].nome,"%s",m[i].nome);
      dvAudioCanais[i]=fx[i+1].canais;
    }
    completarAudioMkv(fx,7,audioMkvEpoch);audioMkvAplicar();
    assert(nAudio==6 && audioAtual==0 && faixaAudio[4].stream_id==2 && faixaAudio[5].stream_id==3);
    nativeSelectCalls=partidas=0;
    video_escolher_audio(4+pick);assert(nativeSelectCalls==0 && partidas==0);
    video_escolher_audio(0);assert(!audioMkvPedido && nativeSelectCalls==1);
    audioMkvPedidoBombear();assert(!partidas);
    nativeSelectCalls=0;video_escolher_audio(4+pick);
    audioMkvPedidoBombear();
    assert(partidas==1 && partidaOrdinal==pick && partidaContagem==6 && partidaStream==-1);
    assert(partidaPaused==pick && partidaPos==745.286 && !strcmp(partidaFaixa.codec,"dts"));
    assert(dtsModoDv==!!profile && dtsSessao);
    // Native IDs differ from demux indices. A metadata revision preserves
    // native return indices and the distinct identities of both hidden DTS.
    nAudio=6;
    for(int i=0;i<6;i++) {
      VideoFaixa *f=&faixaAudio[i];memset(f,0,sizeof *f);f->numero=i;f->stream_index=i+1;
      f->canais=i<2?6:2;strcpy(f->codec,i<2?"dts":"flac");strcpy(f->idioma,"en");
    }
    audioMkvConvertido();audioAtual=pick;posSeg=745.286;
    assert(faixaAudio[0].numero==-1 && faixaAudio[1].numero==-1 && faixaAudio[5].numero==3);
    seekCalls=audioCalls=0;video_escolher_audio(!pick);
    assert(audioCalls==1 && chosenStream==(!pick)+1 && !seekCalls && dtsSessao && dtsModoDv==!!profile);
    // A return to the fourth same-language FLAC must select native index 3,
    // rather than fail as ambiguous or accidentally choose another commentary.
    dtsNativeTarget=faixaAudio[5];dtsNativePending=1;dtsSessao=NULL;fonteMp4=1;
    audioAoCarregar=-1;falhou=0;nativeSelectCalls=0;
    snprintf(midia,sizeof midia,"returned-native");
    evento("{\"sourceInfo\":{\"audioTrackInfo\":[{\"language\":\"en\",\"codec\":\"flac\",\"channels\":2},{\"language\":\"en\",\"codec\":\"flac\",\"channels\":2},{\"language\":\"en\",\"codec\":\"flac\",\"channels\":2},{\"language\":\"en\",\"codec\":\"flac\",\"channels\":2}],\"subtitleTrackInfo\":[]}}");
    assert(!falhou && !dtsNativePending && nativeSelectCalls==1 && nativeSelectedIndex==3 && nAudio==6);
    unsigned retired=audioMkvEpoch;audioMkvNovaFonte();
    completarAudioMkv(fx,7,retired);assert(audioMkvCount==0 && !audioMkvPending);
  }
  puts("PASS hidden DTS controller: HDR10/DV8, both exact DTS, paused/current position, compatible switches, native FLAC return, refreshed lists and stale probes");
}


static void testarRoteamentoRecuperado(void) {
  // LG P7: empty native list, English TrueHD/AC3, recovered French DTS.
  // Discovery alone must not turn the first recovered DTS into the selection.
  audioMkvNovaFonte();
  Mkv m[]={{"A_TRUEHD","en","Original"},{"A_AC3","en","Surround"},{"A_DTS","fr","French"}};
  montar(m,3,NULL,0,0);fonteMp4=0;dvMkvPerfil=7;dvMkvEl=dvMkvBl=dvMkvRpu=1;
  MkvFaixa fx[4]={0};fx[0].tipo=1;fx[0].numero=1;
  for(int i=0;i<3;i++) {
    fx[i+1].tipo=2;fx[i+1].numero=i+2;fx[i+1].canais=6;
    snprintf(fx[i+1].codec,sizeof fx[i+1].codec,"%s",m[i].codec);
    snprintf(fx[i+1].idioma,sizeof fx[i+1].idioma,"%s",m[i].idioma);
    dvAudioCanais[i]=6;
  }
  completarAudioMkv(fx,4,audioMkvEpoch);audioMkvAplicar();
  assert(nAudio==1 && audioAtual==-1 && !video_faixa_dts(video_audio(audioAtual)));
  bindVivo=bindPendente=0;acb=modoLoad=0;semDVForcado=0;dvMemOrd=-1;
  assert(dvPronto());dtsModoDv=1;dvSilencio=1;dvEntradaSeg=0;posSeg=0;
  converterResumeRequested=1;converterResumeUsed=0;fonteTocou=0;
  converterResumeSeconds=7534.103;converterResumePct=0;
  dvSegurar=1;assert(dvPodeIniciar());
  seekEm=seekEnvEm=seekRetryEm=0;partidas=0;
  assert(iniciarDts(-1));
  assert(dtsModoDv && partidas==1 && !partidaComFaixa && partidaContagem==0);
  assert(partidaPos==7534.103 && converterResumeUsed);
  assert(!dvPodeIniciar());dvSegurar=0;
  pronto=tocando=1;strcpy(vidHdr,"DolbyVision");
  VideoDvFase fase;video_dv_fase(&fase);assert(fase.caminho && fase.carregado && fase.dvConfirmado && fase.tocando);
  dtsSessao=NULL;
  // Native DV8 already confirmed by LG is stronger than an inconclusive
  // small MKV probe. A DTS handoff must retain DV control/seek policy.
  audioMkvNovaFonte();
  Mkv d[]={{"A_DTS","en","Original"}};Tv tv[]={{"en","dts",6}};
  montar(d,1,tv,1,0);dvMkvRpu=0;strcpy(vidHdr,"DolbyVision");
  dtsModoDv=0;fixtureMkvEnabled=0;semDVForcado=0;dvSondado=1;
  converterResumeRequested=0;dvSilencio=0;posSeg=745.286;pausaPedida=1;
  bindVivo=bindPendente=0;acb=modoLoad=0;
  assert(iniciarDts(-1) && dtsModoDv && partidaPos==745.286 && partidaPaused);
  dtsSessao=NULL;fixtureMkvEnabled=1;dvSilencio=0;
  // A reopen can prepare P8 after native confirmation/probe were reset.
  DtsMediaInfo prepared={0};strcpy(prepared.video_codec,"hevc");
  prepared.dovi_profile=8;prepared.dovi_bl_present=prepared.dovi_rpu_present=1;
  prepared.dovi_bl_compatibility_id=1;
  dtsModoDv=dvHabilitado=0;dvNao=VIDEO_DV_NAO_FALHOU;
  dvMkvRpu=0;strcpy(vidHdr,"none");sincronizarRotaDts(&prepared);
  assert(dtsModoDv && dvHabilitado && !dvNao);
  // Output truth wins even when native return temporarily forced HDR10.
  dtsModoDv=dvHabilitado=0;semDVForcado=1;sincronizarRotaDts(&prepared);
  assert(dtsModoDv && dvHabilitado);semDVForcado=0;
  for(int invalid=0;invalid<5;invalid++) {
    DtsMediaInfo bad=prepared;
    if(invalid==0)bad.dovi_profile=0;
    if(invalid==1)bad.dovi_rpu_present=0;
    if(invalid==2)bad.dovi_el_present=1;
    if(invalid==3)bad.dovi_profile=7;
    if(invalid==4)strcpy(bad.video_codec,"h264");
    dtsModoDv=dvHabilitado=0;sincronizarRotaDts(&bad);assert(!dtsModoDv && !dvHabilitado);
  }
  puts("PASS prepared DV routing: reopen after inconclusive probe uses actual output; HDR10 and invalid/layered profiles stay unchanged");
  puts("PASS recovered audio: empty P7 native list retains file default, initial saved position and DV overlay state; native-confirmed DV8 retains DV routing");
}

int main(void) {
#ifdef NV_NATIVE_SEEK_TEST
  (void)SDL_GetTicks();SDL_Delay(2); // Keep test events away from the zero timestamp sentinel.
  dtsImagemExibida=1;ligado=0;
  (void)video_tocar("");
  assert(!dtsImagemExibida); // A new source must never inherit the old cover state.
#endif
  setvbuf(stdout, NULL, _IOLBF, 0);
  lsCall = fakeCall; lsPayload = fakePayload;
  bus = (LSHandle *)(uintptr_t)1; sessao = 40;
  if(getenv("NV_TEST_MISSING_DTS")){testarDtsOculto();testarRoteamentoRecuperado();return 0;}
#ifdef NV_DV_TRUEHD_TEST
  // Keep the exact selected TrueHD track at startup instead of substituting
  // another language/codec or handing the picture back to the URI player.
  { Mkv m[]={{"A_TRUEHD","eng","Original Atmos"},{"A_EAC3","eng","DD+"}};
    Tv t[]={{"en","truehd",6},{"en","eac3",6}};
    montar(m,2,t,2,0);dvMkvPerfil=7;
    assert(ling_casa(abrir(),"en") && dvAudioMkvOrd==-1 && dvAudioMkvAtual==0);
    assert(partidaOrdinal==0 && !strcmp(partidaFaixa.codec,"A_TRUEHD"));
    montar(m,2,t,2,0);dtsSessao=(DtsPlayback *)(uintptr_t)1;dtsModoDv=1;
    strcpy(faixaAudio[0].codec,"truehd");faixaAudio[0].stream_index=19;
    strcpy(faixaAudio[1].codec,"eac3");faixaAudio[1].stream_index=20;
    strcpy(faixaAudio[2].codec,"dts");faixaAudio[2].stream_index=21;nAudio=3;
    posSeg=745.286;seekCalls=audioCalls=0;
    video_escolher_audio(audioAtual);assert(!audioCalls && !seekCalls);
    int order[]={1,2,0,1};
    for(int i=0;i<4;i++) {
      video_escolher_audio(order[i]);
      assert(audioCalls==i+1 && seekCalls==0 && chosenStream==19+order[i]);
      assert(dtsSessao && dtsModoDv && dvHabilitado && !dtsNativePending);
      assert(audioAtual==order[i]);
    }
    dtsSessao=NULL;
    assert(!nv_dts_audio_feeds("truehd",0)); // Regular native audio routing is unchanged.
    puts("DV TrueHD: selected startup track retained; TrueHD/EAC3/DTS/TrueHD switches stay on DV without native HDR10 handoff");
  }
  return 0;
#endif

  // 1. O cenario da revisao (#7): MKV [TrueHD eng, E-AC-3 eng, E-AC-3 por], a
  //    TV expoe so a portuguesa, no indice 0. O portugues tem de chegar ao motor.
  { Mkv m[] = {{"A_TRUEHD", "eng", "TrueHD Atmos 7.1"}, {"A_EAC3", "eng", "DD+ 7.1"},
               {"A_EAC3", "por", "DD+ 5.1"}};
    Tv t[] = {{"pt", "eac3", 6}};
    montar(m, 3, t, 1, 0);
    const char *l = abrir();
    printf("dvaudio_lg: TV [E-AC-3 por] x MKV [TrueHD eng, E-AC-3 eng, E-AC-3 por] -> motor recebe \"%s\" (ord %d de %d)\n",
           l, partidaOrdinal, partidaContagem);
    assert(ling_casa(l, "pt"));
    assert(dvAudioMkvOrd == -1 || dvAudioMkvOrd == 2); }

  // 2. A TV expoe so o TrueHD portugues: troca pelo E-AC-3 PORTUGUES do MKV.
  { Mkv m[] = {{"A_TRUEHD", "eng", "TrueHD"}, {"A_EAC3", "eng", "DD+ eng"},
               {"A_TRUEHD", "por", "TrueHD por"}, {"A_EAC3", "por", "DD+ por"}};
    Tv t[] = {{"pt", "truehd", 8}};
    montar(m, 4, t, 1, 0);
    const char *l = abrir();
    assert(dvAudioMkvOrd == 3 && !strcmp(dvAudioDe, "A_TRUEHD"));
    assert(ling_casa(l, "pt") && partidaOrdinal == 3 && partidaContagem == 4);
    puts("dvaudio_lg: TV [TrueHD por] -> E-AC-3 por (ordinal 3 do MKV)"); }

  // 3. Sem correspondencia: a faixa da TV (espanhol) nao existe no MKV lido.
  //    Nao troca (a escolha segura e do motor, que confere idioma e codec).
  { Mkv m[] = {{"A_TRUEHD", "eng", "TrueHD"}, {"A_EAC3", "eng", "DD+"}};
    Tv t[] = {{"es", "truehd", 8}};
    montar(m, 2, t, 1, 0);
    (void)dvPronto();
    assert(dvAudioMkvOrd == -1 && !dvAudioDe[0]);
    puts("dvaudio_lg: TV [es] sem par no MKV -> nao troca"); }

  // 4. Ambigua: a TV nao diz o codec e duas faixas do MKV tem o idioma dela.
  { Mkv m[] = {{"A_TRUEHD", "eng", "TrueHD"}, {"A_EAC3", "eng", "DD+"}, {"A_EAC3", "por", "DD+ por"}};
    Tv t[] = {{"en", "", 0}, {"pt", "", 0}};
    montar(m, 3, t, 2, 0);
    (void)dvPronto();
    assert(dvAudioMkvOrd == -1 && !dvAudioDe[0]);
    puts("dvaudio_lg: TV [en sem codec] ambigua no MKV -> nao troca"); }

  // 5. C9 (o caso que criou a troca): TV [TrueHD eng], MKV [TrueHD eng, E-AC-3 eng].
  { Mkv m[] = {{"A_TRUEHD", "eng", "Dolby TrueHD Atmos 7.1"}, {"A_EAC3", "eng", "Dolby Digital Plus 7.1"}};
    Tv t[] = {{"en", "truehd", 8}};
    montar(m, 2, t, 1, 0);
    const char *l = abrir();
    assert(dvAudioMkvOrd == 1 && ling_casa(l, "en") && partidaOrdinal == 1 && partidaContagem == 2);
    puts("dvaudio_lg: C9 TV [TrueHD eng] -> E-AC-3 eng"); }

  // 6. Listas do mesmo tamanho: a ordem vale (o motor faz o mesmo), mesmo sem
  //    codec na lista da TV.
  { Mkv m[] = {{"A_TRUEHD", "eng", "TrueHD"}, {"A_EAC3", "eng", "DD+"}};
    Tv t[] = {{"en", "", 0}, {"en", "", 0}};
    montar(m, 2, t, 2, 0);
    (void)abrir();
    assert(dvAudioMkvOrd == 1);
    puts("dvaudio_lg: listas iguais, indice 0 = TrueHD -> E-AC-3 eng"); }
  // Real C5 log: all 15 AC3 tracks from a 21-audio remux, including duplicate
  // Russian tracks and English tracks beyond the former 16-track metadata cap.
  { Mkv m[21]; Tv t[15]; int expected[15], nt = 0;
    for (int i = 0; i < 21; i++) {
      const char *codec = (i == 1 || (i >= 8 && i <= 11)) ? "A_DTS/LOSSLESS" : i == 17 ? "A_TRUEHD" : "A_AC3";
      const char *lang = i < 15 ? "ru" : i < 17 ? "uk" : "en";
      m[i] = (Mkv){codec, lang, "fixture"};
      if (!strcmp(codec, "A_AC3")) { expected[nt] = i; t[nt++] = (Tv){lang, "ac3", 6}; }
    }
    assert(nt == 15);
    for (int i = 0; i < nt; i++) {
      montar(m, 21, t, nt, i);
      for (int k = 0; k < 21; k++) dvAudioCanais[k] = 6;
      (void)abrir();
      assert(dvAudioMkvOrd == -1 && dvAudioMkvAtual == expected[i]);
      assert(partidaOrdinal == expected[i] && partidaContagem == 21);
      assert(!strcmp(partidaFaixa.codec, "A_AC3") && partidaFaixa.channels == 6);
    }
    puts("dvaudio_lg: all 15 filtered LG selections hand off exact MKV ordinal/count out of 21 tracks");
  }
#ifdef NV_NATIVE_SEEK_TEST
  // The current TV clock is still zero while DTS startup is being prepared.
  // The saved source position/percentage must reach the first worker instead
  // of waiting for playing and then seeking through a second native Load.
  { Tv t[]={{"pt","dts",6}};
    for(int pct=0;pct<2;pct++) {
      montar(NULL,0,t,1,0);posSeg=posNativa=0;seekEm=seekEnvEm=seekRetryEm=0;
      converterResumeRequested=1;converterResumeSeconds=pct?0:1673.992;
      converterResumePct=pct?67:0;dtsRevisao=0;partidas=0;
      bufferandoDesde=12345; // URI bufferingStart, with no bufferingEnd before unload.
      assert(iniciarDts(-1) && partidas==1);
      assert(bufferandoDesde==0 && video_bufferando_ms()==0);
      // Genuine buffering on the new converter still reports normally.
      evento("{\"bufferingStart\":{}}");assert(video_bufferando_ms()>0);
      evento("{\"bufferingEnd\":{}}");assert(video_bufferando_ms()==0);
      assert(partidaPos==(pct?0:1673.992) && partidaPct==(pct?67:0));
      assert(video_retomada_inicial_estado()==0);
      dtsRevisao=1;assert(video_retomada_inicial_estado()==1);
      dtsSessao=NULL;assert(video_retomada_inicial_estado()==-1);
    }
    // An explicit pending seek takes priority over remembered startup data.
    montar(NULL,0,t,1,0);converterResumeRequested=1;converterResumeSeconds=100;
    seekEm=1;seekAlvo=250;assert(iniciarDts(-1));
    assert(partidaPos==250 && partidaPct==0 && !converterResumeUsed);dtsSessao=NULL;
    // Switching an already playing native source to DTS uses its current
    // clock, never the old account resume target.
    montar(NULL,0,t,1,0);converterResumeRequested=1;converterResumeSeconds=100;
    fonteTocou=1;posSeg=660;seekEm=seekEnvEm=seekRetryEm=0;
    assert(iniciarDts(-1));
    assert(partidaPos==660 && partidaPct==0 && !converterResumeUsed);dtsSessao=NULL;
    converterResumeRequested=converterResumeUsed=0;seekEm=seekEnvEm=seekRetryEm=0;
    puts("dvaudio_lg: initial DTS seconds/percent handoff, ack, native fallback and user seek priority passed");
  }
  { Mkv m[]={{"A_EAC3","eng","DD+"}};Tv t[]={{"en","eac3",6}};
    fixtureMkvEnabled=0;
    for(int profile=5;profile<=8;profile+=3) {
      montar(m,1,t,1,0);dvMkvPerfil=profile;
      assert(!dvPronto() && !dvHabilitado && dvNao==VIDEO_DV_NAO_NATIVE);
      assert(!dtsModoDv && !dtsTentou && !dtsSessao);
    }
    // Rolling back P5/P8 forcing must not disable the separate P7 converter.
    montar(m,1,t,1,0);dvMkvPerfil=7;dvMkvEl=1;
    assert(dvPronto());
    fixtureMkvEnabled=1;
    puts("dvaudio_lg: native P5/P8 MKV respects its preference; P7 remains independently enabled");
  }
  // C5 regression: MP4 already playing DV must never be reopened as an
  // experiment, regardless of selected E-AC-3 audio or initial HDR timing.
  { Tv t[]={{"en","eac3",6},{"pl","eac3",6}};
    for(int detected=0;detected<2;detected++) for(int hdr=0;hdr<2;hdr++) {
      montar(NULL,0,t,2,1);fonteMp4=detected;partidas=0;
      snprintf(urlAtual,sizeof urlAtual,"http://h/native.mp4");
      snprintf(vidHdr,sizeof vidHdr,"%s",hdr?"DolbyVision":"none");
      assert(!dvPronto() && !dvHabilitado && !partidas);
      assert(!dtsModoDv && !dtsTentou && !dtsSessao);
      assert(audioAtual==1 && !strcmp(faixaAudio[1].idioma,"pl"));
    }
    fonteMp4=0;
    puts("dvaudio_lg: native DV MP4 stays on the URI player without conversion/HDR10 fallback");
  }
#ifdef NV_MP4_BD_TEST
  {
    const char *failure="{\"errorCode\":204,\"errorText\":\"Fail To Demultiplex\"}";
    Tv t[]={{"en","ac3",6}};
    // Native startup refuses a MP4: OFF keeps the error path; ON intercepts
    // exactly that startup event. No source-name or DV badge is required.
    for(int enabled=0;enabled<2;enabled++) {
      montar(NULL,0,t,1,0);mp4TestEnabled=enabled;mp4TestPending=mp4TestStarted=0;
      falhou=terminou=0;dtsImagemExibida=0;
      snprintf(urlAtual,sizeof urlAtual,"http://h/dual.mp4");fonteMp4=1;
      reconErroPend=recuperando=modoLoad=0;fonteTocou=0;
      eventoPayload(failure,sessao);
      assert(mp4TestPending==enabled && reconErroPend==!enabled);
      VideoDvFase f;video_dv_fase(&f);assert(f.mp4Inicial==enabled);
    }
    assert(mp4TestPending);dtsModoDv=1;fixtureNextMp4Mode=0;
    converterResumeRequested=1;converterResumeSeconds=246;converterResumePct=0;
    converterResumeUsed=0;seekEm=seekEnvEm=seekRetryEm=0;
    assert(iniciarDts(-1) && fixtureNextMp4Mode==1 && partidaPos==246 && converterResumeUsed);
    { VideoDvFase f;
      video_dv_fase(&f);assert(f.mp4Inicial && f.caminho);
      dtsImagemExibida=1;video_dv_fase(&f);assert(!f.mp4Inicial);
      pronto=tocando=0;video_dv_fase(&f);assert(!f.mp4Inicial); // Seek/audio reload cannot restore startup text.
      dtsImagemExibida=0;falhou=1;video_dv_fase(&f);assert(!f.mp4Inicial);falhou=0;
    }
    dtsSessao=NULL;
    for(int variant=0;variant<3;variant++) {
      montar(NULL,0,t,1,0);mp4TestEnabled=1;mp4TestPending=mp4TestStarted=0;
      snprintf(urlAtual,sizeof urlAtual,"http://h/%s",variant==0?"healthy.mkv":"native.mp4");
      fonteMp4=variant!=0;fonteTocou=variant==1;dtsTentou=variant==2;
      reconErroPend=recuperando=modoLoad=0;eventoPayload(failure,sessao);
      assert(!mp4TestPending && reconErroPend);
      VideoDvFase f;video_dv_fase(&f);assert(!f.mp4Inicial);
    }
    mp4TestEnabled=mp4TestPending=0;fonteMp4=0;dtsTentou=0;reconErroPend=0;
    puts("PASS real webOS MP4 retry: OFF/native MKV/native playing MP4/repeated failures unchanged; ON startup 204 handoff and resume");
  }
#endif
#endif
  return 0;
}
