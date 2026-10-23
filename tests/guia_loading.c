#include "stub_perfisessao.h"
#include "../src/guia.c"
#include <assert.h>
static int chamadasXt,chamadasSt,chamadasM,interromper;
const char *i18n(const char *s){return s;}
int perfis_ativo(void){return 1;}
int addons_n(void){return 0;}
const char *addons_base(int i){(void)i;return "";}
const char *addons_nome(int i){(void)i;return "";}
char *dados_caminho(char *out,unsigned n,const char *nome){(void)out;(void)n;(void)nome;return NULL;}
int xtream_fonte_configurado(unsigned id){(void)id;return 1;}
int stalker_fonte_configurado(unsigned id){(void)id;return 1;}
int xtream_fonte_estado(unsigned id,int *http){(void)id;*http=200;return XT_OK;}
int xtream_fonte_canais_cb(unsigned id,void (*cat)(const char*,int,int,void*),XtreamCb cb,void *u,int *total,uint64_t sessao) {
 (void)sessao;chamadasXt++;cat("Sports",0,1,u);char key[80];
 if(id)snprintf(key,sizeof key,"xtream:@%08x:1",id);else strcpy(key,"xtream:1");
 XtreamItem c={.id=key,.nome="Sports channel",.categoria="Sports",.logo="",.epgId="sport",.catOrdem=0,.num=1};
 assert(cb(&c,u));*total=1;if(interromper)cargaCancelada=1;return 1;
}
int stalker_fonte_canais_cb(unsigned id,StalkerCb cb,void *u,uint64_t sessao) {
 (void)id;(void)sessao;chamadasSt++;StalkerCanal c={0};strcpy(c.id,"stalker:1");strcpy(c.nome,"News");strcpy(c.categoria,"News");return cb(&c,u);
}
int ajustes_livetv_sugerir(void){return 0;}
const char *livecat_secao(const char *n,int *r){(void)n;*r=0;return NULL;}
int xtream_e_id(const char *id){return id && !strncmp(id,"xtream:",7);}
int m3u_fonte_urls(unsigned id,char *url,unsigned n,char *epg,unsigned ne) {
 (void)id;snprintf(url,n,"https://playlist.invalid/shared.m3u8");snprintf(epg,ne,"https://epg.invalid/schedule.xml.gz");return 1;
}
char *rede_baixar_medido_controle(const char *url,int segundos,const char *const *cab,const RedeControle *ctl,RedeMedida *med) {
 (void)url;(void)segundos;(void)cab;(void)ctl;(void)med;assert(0);return NULL;
}
int rede_baixar_fluxo(const char *url,int segundos,const char *const *cab,int (*cb)(const char*,size_t,void*),void *u,long long max,int *st,long long *bytes) {
 (void)segundos;(void)cab;(void)max;assert(!strcmp(url,"https://playlist.invalid/shared.m3u8"));
 const char *pl="#EXTM3U\n#EXTINF:-1 tvg-id=\"sports\" group-title=\"Sports\",Sports\nhttps://stream.invalid/live.m3u8\n";
 chamadasM++;assert(cb(pl,strlen(pl),u));*st=200;*bytes=(long long)strlen(pl);return 1;
}
static void setup(int cancel) {
 chamadasXt=chamadasSt=chamadasM=0;interromper=cancel;cargaSessao=0;cargaCancelada=0;tetoCarga=30000;
 sNFontes=nAddonsCarga=nCargaUrls=0;addonsCarga=NULL;nCargaContas=4;
 cargaContas[0]=(IptvConta){.tipo=IPTV_XTREAM,.ativa=1,.nome="XT original"};
 cargaContas[1]=(IptvConta){.tipo=IPTV_XTREAM,.id=19,.ativa=1,.nome="XT second"};
 cargaContas[2]=(IptvConta){.tipo=IPTV_M3U,.id=20,.ativa=1,.nome="Playlist"};
 cargaContas[3]=(IptvConta){.tipo=IPTV_STALKER,.ativa=1,.nome="Portal"};
 // Same source added via the new upstream URL manager is loaded once.
 nCargaUrls=1;cargaUrls[0]=(LtvUrl){.tipo=LTV_URL_M3U};strcpy(cargaUrls[0].nome,"sameURL");strcpy(cargaUrls[0].url,"https://playlist.invalid/shared.m3u8");
 atomic_store(&pendPronto,0);
}
int main(void) {
 setup(0);fioGuia(NULL);assert(sOk && sNCanais==4 && sNLtvF==4 && chamadasXt==2 && chamadasSt==1 && chamadasM==1);
 assert(!strcmp(sLtvF[0].chave,"xt") && !strcmp(sLtvF[1].chave,"xta00000013") && !strcmp(sLtvF[2].chave,"ma00000014"));
 assert(!strcmp(sCanais[0].id,"xtream:1") && !strcmp(sCanais[1].id,"xtream:@00000013:1"));
 assert(!strcmp(sCanais[2].id,"nvm3u:@00000014:6dd1f284ff947429"));assert(!strcmp(sLtvF[2].urlTvg,"https://epg.invalid/schedule.xml.gz"));
 assert(sCanais[2].url[0] && !strcmp(sCanais[2].url,"https://stream.invalid/live.m3u8"));
 setup(1);fioGuia(NULL);assert(sNCanais==1 && chamadasXt==1 && !chamadasSt && !chamadasM);
 setup(0);cargaContas[1].ativa=0;fioGuia(NULL);assert(sNCanais==3 && chamadasXt==1 && chamadasM==1 && chamadasSt==1);
 puts("PASS: source-keyed streaming accounts, same-type identities, configured EPG URL, registry/URL playlist dedupe, disabled accounts and cancellation");
}
