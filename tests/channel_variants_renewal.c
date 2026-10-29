#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { char url[256],canalVariante[80]; } Stream;
static Stream sources[2];
static int canalFonteIdx,canalFonteDesde,canalViaProxy,canalFontePrazo;
static int stalkerRenovando,stalkerTentativas,fontePedidoGeracao,fonteEscolhida;
static int requests,errors;
static unsigned listGeneration=3;
static char requested[80];
static const char *playing="stalker:hd";
#define CANAL_STALKER_TENTATIVAS 2
#define CANAL_FONTE_PRAZO_MS 25000
#define FJOB_STALKER 1
static const char *player_id_canal(void){return playing;}
static int stalker_e_id(const char *id){return !strncmp(id,"stalker:",8);}
static int m3u_e_id(const char *id){return !strncmp(id,"m3u:",4);}
static int guia_tem_variantes(const char *id){return 1;}
static const Stream *stream_item(int i){return i>=0&&i<2?&sources[i]:NULL;}
static int stream_atual(void){return 1;}
static unsigned stream_lista_geracao(void){return listGeneration;}
static unsigned novaGeracaoFonte(void){return 1;}
static int pedirFonteJob(int type,unsigned generation,const char *id,int renewing){requests++;snprintf(requested,sizeof requested,"%s",id);return 0;}
static int SDL_GetTicks(void){return 100;}
static int player_mini_ativo(void){return 0;}
static int player_aberto(void){return 1;}
static int player_quer_sair(void){return 0;}
static void player_fechar_mini(void){errors++;}
static void player_erro_fonte(void){errors++;}
static void player_definir_fonte(const char *url){assert(strstr(url,"fresh"));}
static void stream_definir_atual(int i){assert(i==1);}
static void marco(const char *s){}
static int montarCanalStalker(const char *id,const char *url){assert(!"must preserve grouped sources");return -1;}
static int stream_vod_renovada_aplicar(int i,unsigned generation,const Stream *s,unsigned long session){assert(i==1&&generation==listGeneration);sources[i]=*s;return 1;}
static void watchdog(void){
#include "stalker_watchdog.inc"
}
static void consume(const char *id,int success){
  int mesmoId=0,renovando=1,aplicar=1,resultado=success?0:-1;
  const char *url=success?"https://example.invalid/fresh":"";
  struct { unsigned listaGeracao; unsigned long sessaoPerfil; } data={listGeneration,1},*job=&data;
#include "stalker_consumer.inc"
}
int main(void){
  strcpy(sources[0].canalVariante,"stalker:hd");strcpy(sources[0].url,"old-hd");
  strcpy(sources[1].canalVariante,"stalker:fhd");strcpy(sources[1].url,"old-fhd");
  canalFonteIdx=1;watchdog();
  assert(requests==1&&stalkerRenovando&&!strcmp(requested,"stalker:fhd"));
  consume(requested,1);
  assert(!stalkerRenovando&&stalkerTentativas==1&&canalFonteIdx==1);
  assert(!strcmp(sources[0].url,"old-hd")&&strstr(sources[1].url,"fresh"));
  canalFonteIdx=1;watchdog();assert(requests==2);
  consume("stalker:hd",1);assert(stalkerRenovando); /* Stale variant response. */
  consume(requested,0);assert(!stalkerRenovando&&errors==0&&canalFonteIdx==1);
  watchdog();assert(requests==2); /* Exhausted renewal continues to variant fallback. */
  puts("PASS: grouped Stalker watchdog renews current variant and preserves fallback sources");
}
