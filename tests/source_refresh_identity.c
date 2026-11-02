// Router and addon transport fixture. Production functions are extracted by the runner.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef unsigned Uint32;
typedef struct {char imdb[64],tipo[12];int nElenco;} CatItem;
typedef struct {int unused;} Stream;
typedef struct {int unused;} FontecacheEscopo;
typedef struct {int valido;} Resumo;
enum { ADD_PARADO,ADD_BUSCANDO,ADD_PRONTO,ADD_VAZIO,FC_NADA=0,FC_ACERTO=1,FC_EM_CURSO=2 };
static CatItem series={.imdb="tt123",.tipo="series",.nElenco=1};
static int metaReady,season=1,episode=1,sourceN,fioVivo,nAddon=1,progLigado,estado,jfAlvo,adotado;
static int cwEspera,aguardandoFonte,requests,cacheDrops;
static Uint32 ticks=1000;
static char lastRequest[64],firstRequest[64],alvoId[64],alvoTipo[16],alvoBase[64],pendId[64],pendTipo[16],pendBase[64],listTarget[64],requestedTarget[64];
static int pendRenovar;
static FontecacheEscopo fioEscopo;
static Resumo resumo;
static Uint32 SDL_GetTicks(void){return ticks;}
static int detail_ep_foco(int *t,int *e){if(!metaReady)return 0;*t=season;*e=episode;return 1;}
static int detail_indice(void){return 0;}
static const CatItem *cat_item(int i){(void)i;return &series;}
static int cat_id_stream(int i,int t,int e,char *o,unsigned n){(void)i;snprintf(o,n,"%s:%d:%d",series.imdb,t,e);return 1;}
static void stream_filtro_tipo(const char *p){(void)p;}
static void stream_folha_canal(int p){(void)p;}
static int nv_tipo_canal(const char *p){(void)p;return 0;}
static int jfid_e(const char *p){(void)p;return 0;}
static void stream_invalidar(const char *p){(void)p;sourceN=0;listTarget[0]=0;}
static int servidores_fontes_pedir(const char *p){(void)p;return 0;}
static void resumoDaLista(Resumo *r){(void)r;}
static int idbase_e_imdb(const char *p){return !strncmp(p,"tt",2);}
static void ondever_pedir(const char *p,int a,int b){(void)p;(void)a;(void)b;}
static void debrid_nova_busca(void){}
static int addons_origem_extra_ativa(void){return 0;}
static int idbase_tem_episodio(const char *p){return strchr(p,':')!=NULL;}
static void stream_definir_alvo(const char *p){snprintf(requestedTarget,sizeof requestedTarget,"%s",p);}
static int escopoAindaAtual(const FontecacheEscopo *s){(void)s;return 1;}
static int stream_lista_do_alvo(const char *p){return sourceN>0&&!strcmp(listTarget,p);}
static void progRepublicar(void){sourceN=1;snprintf(listTarget,sizeof listTarget,"%s",alvoId);}
static int stream_n(void){return sourceN;}
static void stream_definir_lista(const Stream *s,int n){(void)s;sourceN=n;if(!n)listTarget[0]=0;}
static void idbase_episodio(const char *p,int *t,int *e){sscanf(p,"tt%*d:%d:%d",t,e);}
static void debrid_definir_episodio(int t,int e){(void)t;(void)e;}
static int alvoVod(void){return 1;}
static void addons_capturar_escopo(FontecacheEscopo *e){(void)e;}
static void fontecache_vod_apagar(const char *a,const char *b,const char *c,FontecacheEscopo *e){(void)a;(void)b;(void)c;(void)e;cacheDrops++;}
static int fontecache_vod_pegar(const char *a,const char *b,const char *c,FontecacheEscopo *e,Stream **s,int *n,Uint32 *i){(void)a;(void)b;(void)c;(void)e;(void)s;(void)n;(void)i;return FC_NADA;}
static void stream_definir_lista_idade(const Stream *s,int n,Uint32 i){(void)i;stream_definir_lista(s,n);}
static int fontecache_pegar(const char *a,const char *b,Stream **s,int *n){(void)a;(void)b;(void)s;(void)n;return FC_NADA;}
static void dispararBusca(void){requests++;if(requests==1)snprintf(firstRequest,sizeof firstRequest,"%s",alvoId);snprintf(lastRequest,sizeof lastRequest,"%s",alvoId);fioVivo=progLigado=1;estado=ADD_BUSCANDO;}
#include "source_refresh_addons.inc"

static void addons_buscar(const char *i,const char *t){buscarPedido(i,t,0);}
static int player_aberto(void){return 0;}
static unsigned addonVersion=1;
static unsigned addons_versao(void){return addonVersion;}
static int stream_folha_aberta(void){return 0;}
static void fontepref_id_base(const char *p,char *o,unsigned n){snprintf(o,n,"%.*s",(int)strcspn(p,":"),p);}
static int desc_elenco_precisa_pessoas(const CatItem *i){(void)i;return 0;}
static void desc_episodios(int i,int t){(void)i;(void)t;}
static void origemFontes(const CatItem *p){(void)p;}
static void aquecerAoAbrirTitulo(void){}
static void addons_buscar_legendas(const char *a,const char *b){(void)a;(void)b;}
static int detail_aberto(void){return 1;}
#include "source_refresh_target.inc"

#include "source_refresh_watch.inc"
static void finish(void){fioVivo=0;progLigado=0;sourceN=1;estado=ADD_PRONTO;snprintf(listTarget,sizeof listTarget,"%s",alvoId);}
int main(int argc,char **argv){
 assert(argc==2);
 int mode=atoi(argv[1]);
 metaReady=mode>=2;
 if(mode==3)episode=13;
 watch();assert(requests==1);
 if(mode!=1)finish();
 metaReady=1;
 if(mode==3){season=2;episode=1;}
 ticks=10000;watch();ticks=11201;watch();
 if(mode==0){assert(requests==1&&sourceN==1&&cacheDrops==0);puts("PASS: late S1E1 metadata preserves the completed list and makes no second request");}
 if(mode==1){assert(requests==1);puts("Confirmed: same episode while source search is still active -> network request reused");}
 if(mode==2){assert(requests==1);puts("Confirmed: metadata/season update with unchanged source ID -> no second source request");}
 if(mode==3){assert(requests==2&&strcmp(firstRequest,lastRequest));puts("Confirmed: canonical S1E13 -> S2E1 changes target -> second source request");}
 if(mode==4){finish();int before=requests;buscarPedido(alvoId,alvoTipo,1);assert(requests==before+1&&cacheDrops==1);puts("PASS: explicit refresh still refetches");}
 if(mode==5){finish();int before=requests;addonVersion++;ticks+=2000;watch();assert(requests==before+1);puts("PASS: addon configuration change still refetches");}
 {char id[64];metaReady=0;strcpy(series.imdb,"kitsu:41370");idDoAlvo(&series,id,sizeof id);assert(!strcmp(id,"kitsu:41370"));strcpy(series.tipo,"movie");strcpy(series.imdb,"tt123");idDoAlvo(&series,id,sizeof id);assert(!strcmp(id,"tt123"));}
 printf("mode=%d requests=%d first=%s last=%s cacheInvalidations=%d\n",mode,requests,firstRequest,lastRequest,cacheDrops);
 return 0;
}
