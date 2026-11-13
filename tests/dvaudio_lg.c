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
static DtsTrack partidaFaixa;
static int partidas, partidaOrdinal, partidaContagem, partidaComFaixa;
static DtsPlayback *dublePartida(const char *url, const char *headers, int audio_stream,
                                 double position, int paused, const char *window,
                                 int webos_major, const DtsTrack *selected, int ordinal,
                                 int count) {
  (void)url; (void)headers; (void)audio_stream; (void)position; (void)paused; (void)window;
  (void)webos_major;
  partidas++; partidaOrdinal = ordinal; partidaContagem = count;
  partidaComFaixa = selected != NULL;
  memset(&partidaFaixa, 0, sizeof partidaFaixa);
  if (selected) partidaFaixa = *selected;
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


typedef struct { const char *codec, *idioma, *nome; } Mkv;
typedef struct { const char *idioma, *codec; int canais; } Tv;

static void montar(const Mkv *m, int nm, const Tv *t, int nt, int atual) {
  int i;
  dtsSessao = NULL; dtsModoDv = 0; dtsTentou = 0; dvAudioMkvOrd = -1; dvTruehd = 0; dvAudioDe[0] = 0;
  ligado = pronto = tocando = 1;
  snprintf(midia, sizeof midia, "fixture-hdr10");
  snprintf(urlAtual, sizeof urlAtual, "http://h/remux.dv.mkv");
  dvHabilitado = 1; dvNao = 0; dvSondado = 1;
  dvMkvPerfil = 8; dvMkvBl = 1; dvMkvRpu = 1; dvMkvEl = 0;
  dvMemOrd = -1;
  for (i = 0; i < nm; i++) {
    snprintf(dvAudioCodec[i], sizeof dvAudioCodec[0], "%s", m[i].codec);
    snprintf(dvAudioIdioma[i], sizeof dvAudioIdioma[0], "%s", m[i].idioma);
    snprintf(dvAudioNome[i], sizeof dvAudioNome[0], "%s", m[i].nome);
  }
  dvAudios = nm;
  memset(faixaAudio, 0, sizeof faixaAudio);
  for (i = 0; i < nt; i++) {
    snprintf(faixaAudio[i].idioma, sizeof faixaAudio[i].idioma, "%s", t[i].idioma);
    snprintf(faixaAudio[i].codec, sizeof faixaAudio[i].codec, "%s", t[i].codec);
    faixaAudio[i].canais = t[i].canais;
    faixaAudio[i].stream_index = faixaAudio[i].stream_id = -1;
  }
  nAudio = nt; audioAtual = atual;
}
// dvPronto decide; iniciarDts entrega ao motor. Devolve o idioma entregue.
static const char *abrir(void) {
  assert(dvPronto());
  dtsModoDv = 1; partidas = 0;
  assert(iniciarDts(-1) && partidas == 1);
  dtsSessao = NULL;
  return partidaComFaixa ? partidaFaixa.language : "";
}

int main(void) {
  { // DV MKV is opt-in (default Off since its setting landed); the fixture opts in.
    const char *d = getenv("NUVIO_DADOS"); char c[512]; FILE *f;
    assert(d); snprintf(c, sizeof c, "%s/ajustes.txt", d);
    f = fopen(c, "w"); assert(f); fputs("dvMkvLocal 0\n", f); fclose(f);
    ajustes_dir(d); assert(ajustes_dv_mkv()); }
  lsCall = fakeCall; lsPayload = fakePayload;
  bus = (LSHandle *)(uintptr_t)1; sessao = 40;

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
    assert(dvAudioMkvOrd == 2 && dvTruehd && !strcmp(partidaFaixa.codec, "truehd"));
    assert(ling_casa(l, "pt") && partidaOrdinal == 2 && partidaContagem == 4);
    puts("dvaudio_lg: selected Portuguese TrueHD retained for conversion"); }

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
    assert(dvAudioMkvOrd == 0 && dvTruehd && ling_casa(l, "en") && partidaOrdinal == 0 && partidaContagem == 2);
    assert(!strcmp(partidaFaixa.codec, "truehd"));
    puts("dvaudio_lg: English TrueHD retained despite compatible substitute"); }

  // 6. Listas do mesmo tamanho: a ordem vale (o motor faz o mesmo), mesmo sem
  //    codec na lista da TV.
  { Mkv m[] = {{"A_TRUEHD", "eng", "TrueHD"}, {"A_EAC3", "eng", "DD+"}};
    Tv t[] = {{"en", "", 0}, {"en", "", 0}};
    montar(m, 2, t, 2, 0);
    (void)abrir();
    assert(dvAudioMkvOrd == 0 && dvTruehd);
    puts("dvaudio_lg: equal lists retain TrueHD ordinal for conversion"); }

  // 7. Late DV8 (Matis94 614176d73): the prepared output record wins over an
  //    inconclusive probe; HDR10, P7/EL and non-HEVC stay untouched.
  { DtsMediaInfo prepared={0};strcpy(prepared.video_codec,"hevc");
    prepared.dovi_profile=8;prepared.dovi_bl_present=prepared.dovi_rpu_present=1;
    prepared.dovi_bl_compatibility_id=1;
    dtsModoDv=dvHabilitado=0;dvNao=VIDEO_DV_NAO_FALHOU;sincronizarRotaDts(&prepared);
    assert(dtsModoDv && dvHabilitado && !dvNao);
    for(int invalid=0;invalid<5;invalid++) {
      DtsMediaInfo bad=prepared;
      if(invalid==0)bad.dovi_profile=0;
      if(invalid==1)bad.dovi_rpu_present=0;
      if(invalid==2)bad.dovi_el_present=1;
      if(invalid==3)bad.dovi_profile=7;
      if(invalid==4)strcpy(bad.video_codec,"h264");
      dtsModoDv=dvHabilitado=0;sincronizarRotaDts(&bad);assert(!dtsModoDv && !dvHabilitado);
    }
    dtsModoDv=dvHabilitado=0;
    puts("dvaudio_lg: late DV8 output promotes controller DV; invalid/layered profiles stay"); }
  { VideoFaixa f; DtsTrack t = { .kind = DTS_AUDIO, .channels = 6 };
    strcpy(t.codec, "dts"); strcpy(t.language, "fr"); strcpy(t.title, "Original DTS-ES Surround Mix");
    dtsRotuloAudio(&f, &t);
    assert(strstr(f.rotulo, " \xc2\xb7 DTS 5.1") && !strstr(f.rotulo, "Surround Mix"));
    assert(!strcmp(f.nome, "Original DTS-ES Surround Mix"));
    strcpy(t.codec, "flac"); t.channels = 2; strcpy(t.title, "Commentary by director David Fincher and actors Brad Pitt");
    dtsRotuloAudio(&f, &t);
    assert(!strncmp(f.rotulo, "Commentary by director David Fincher and actors Brad Pitt \xc2\xb7 FLAC", 65));
    puts("dvaudio_lg: converter rows are language-first; FLAC keeps the full file title"); }
  { const Mkv m[] = {{"A_AC3","eng",""},{"A_DTS","eng","Main"},{"A_DTS","eng","Commentary"}};
    const Tv t[] = {{"en","ac3",6}};
    montar(m,3,t,1,0); dtsHabilitado=1;
    __atomic_store_n(&audioMkvPronto,1,__ATOMIC_RELEASE);
    audioMkvOmitidas(); assert(nAudio==3 && faixaAudio[2].ordinalMkv==2);
    assert(video_audio_numero_memoria(&faixaAudio[1]) != video_audio_numero_memoria(&faixaAudio[2]));
    audioMkvOmitidas(); assert(nAudio==3 && faixaAudio[2].rotulo[0]);
    partidas=0; video_escolher_audio(2);
    assert(partidas==1 && partidaOrdinal==2 && partidaContagem==3);
    assert(!strcmp(partidaFaixa.codec,"dts") && !strcmp(partidaFaixa.language,"eng"));
    dtsSessao=NULL;
    puts("dvaudio_lg: recovered same-language DTS selection passes full-file identity"); }
  return 0;
}
