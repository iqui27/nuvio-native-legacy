// Production faixas controller/list + audioext state/ranking, offline stream boundary.
// SDL/GL declarations are mocked; this test makes no rendering or SDK claim.
#define NV_GL_COMPAT_H
typedef unsigned int GLuint;
#include "faixas.h"
#include "ajustes.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <assert.h>
// Load host platform/system headers before selecting the mocked webOS branch.
#undef __APPLE__
#include "../src/faixas.c"
static Stream sources[5];
static int enabled = 1, off, current, valid = 1, total = 5;
static unsigned generation = 1;
static const char *channel = "";
int ajustes_audio_externo(void) { return enabled; }
int ajustes_audio_externo_avisar_grande(void) { return 1; }
void legsync_audio_trocou(void) {}
int ajustes_audio_externo_ocultar_fora_cache(void) { return 1; }
int ajustes_dts_desligado(void) { return off; }
int ajustes_idioma(void) { return 0; }
const char *ling_audio(void) { return "en"; }
const char *ling_audio2(void) { return "pl"; }
int ling_casa(const char *a, const char *b) { return !strcmp(a,b); }
const char *player_id_canal(void) { return channel; }
const char *video_url_atual(void) { return "fixture:video"; }
int video_audio_atual(void) { return 0; }
const VideoFaixa *video_audio(int i) { (void)i;static VideoFaixa f={.idioma="rus"};return &f; }
int video_n_legenda(void) { return 0; }
int addons_n_legendas(void) { return 0; }
int video_n_audio(void) { return 2; } // real active donor tracks are supplied by video.c
int stream_n(void) { return total; }
int stream_atual(void) { return current; }
int stream_lista_do_alvo_atual(void) { return valid; }
unsigned stream_lista_geracao(void) { return generation; }
const Stream *stream_item(int i) { return i >= 0 && i < total ? &sources[i] : NULL; }
void stream_audioext_entrada(const Stream *s, AudioExtEntrada *e) {
  memset(e,0,sizeof *e);if(!s)return;
  e->temUrl=!!s->url[0];e->codec=s->altura;e->cache=1;e->nIdiomas=1;
  snprintf(e->idiomas[0],sizeof e->idiomas[0],"%.7s",s->provedor);
}
void stream_audioext_versao(const Stream *s,char *out,unsigned n) { snprintf(out,n,"%s",s->rotulo); }
const char *i18n(const char *s) { return s; }
void player_toast(const char *s,Uint32 ms) { (void)s;(void)ms; }
static void source(int i,const char *url,const char *lang,int codec) {
  snprintf(sources[i].url,sizeof sources[i].url,"%s",url);
  snprintf(sources[i].rotulo,sizeof sources[i].rotulo,"Version %d",i);
  snprintf(sources[i].provedor,sizeof sources[i].provedor,"%s",lang);
  sources[i].altura=codec;
}
int main(void) {
  source(0,"fixture:video","en",AEXT_DD);
  source(1,"fixture:english","en",AEXT_DDP);
  source(2,"fixture:polish","pl",AEXT_DD);
  source(3,"fixture:dts","pl",AEXT_DTS);
  source(4,"","en",AEXT_AAC); // no resolved URL
  audioext_alvo("tt1:1:1");audioext_preferencias(1,1);
  extMontar(1);
  assert(extN==3 && nLinhas(0)==5 && extCand[0].indice==1 && extCand[1].indice==2);
  assert(extEscolher(2)); // the first candidate, after the two current tracks
  assert(!strcmp(audioext_url(),"fixture:english") && !strcmp(audioext_idioma(),"en"));
  assert(!strcmp(audioext_idioma_principal(),"rus"));
  audioext_tocando_definir(1);audioext_offset_definir(-2300);
  assert(nLinhas(0)==8 && extControles()==3);
  unsigned g=audioext_pedido_geracao();
  assert(extEscolher(6)); // Polish, after tracks + three controls
  assert(audioext_pedido_geracao()==g+1 && !strcmp(audioext_url(),"fixture:polish"));
  assert(!strcmp(audioext_idioma(),"pl") && audioext_offset_ms()==-2300 && audioext_so_audio());
  audioext_escolher_faixa(3); // video.c uses the selected donor's demux index
  assert(audioext_faixa()==3 && audioext_offset_ms()==-2300);
  assert(!extEscolher(1)); // active donor track delegates to existing video_escolher_audio
  // An add-on refresh cannot choose a different source at a stale row index.
  g=audioext_pedido_geracao();generation++;
  assert(extEscolher(5) && audioext_pedido_geracao()==g);
  assert(extEscolher(3) && audioext_dopasuj_pedido_consumir());
  assert(extEscolher(4) && !audioext_tem_marca()); // this file's own audio
  off=1;extMontar(1);assert(extN==2 && nLinhas(0)==4);
  for(int i=0;i<extN;i++) assert(extEnt[i].codec!=AEXT_DTS);
  valid=0;extMontar(1);assert(extN==0 && nLinhas(0)==2);
  valid=1;channel="live";extMontar(1);assert(extN==0 && nLinhas(0)==2);
  channel="";enabled=0;extMontar(1);assert(extN==0 && nLinhas(0)==2);
  puts("PASS player audio menu: real candidates/actions, track delegation, stale list, DTS Off, own audio, live/disabled isolation");
}
