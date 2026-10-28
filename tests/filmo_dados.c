#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "../src/fpscena.h"
static void *scheduled;
static int schedule(pthread_t *t,const pthread_attr_t *a,void *(*fn)(void *),void *p){(void)t;(void)a;(void)fn;assert(!scheduled);scheduled=p;return 0;}
static int detach(pthread_t t){(void)t;return 0;}
#define pthread_create schedule
#define pthread_detach detach
#include "../src/filmomodelo.h"
static int stateCalls;
FmEstado measured_state(const FmCredito *,int,int,const char *,int,const char *);
#define fm_estado measured_state
#include "../src/filmodados.c"
#undef fm_estado
FmEstado measured_state(const FmCredito *c,int pct,int eps,const char *date,int como,const char *manual){stateCalls++;return fm_estado(c,pct,eps,date,como,manual);}
static int watched[3];
static unsigned catalogRevision,episodeRevision;
static CatItem catalog[2000];
static int catalogCount,indexCalls,itemReads;
unsigned cat_revisao_itens(void){return catalogRevision;}
unsigned vistoep_revisao(void){return episodeRevision;}
const CatItem *cat_item(int i){itemReads++;return i>=0&&i<catalogCount?catalog+i:NULL;}
void dados_iniciar(const char *s){(void)s;}
static char savedName[220],saved[10000];
int dados_gravar(const char *s,const char *b){snprintf(savedName,sizeof savedName,"%s",s);snprintf(saved,sizeof saved,"%s",b);return 1;}
char *dados_ler(const char *s){return !strcmp(s,savedName)?strdup(saved):NULL;}
const char *sessao_usuario(void){return "test";}
int perfis_ativo(void){return 1;}
int ajustes_conquistas(void){return 0;}
int cat_n(void){return catalogCount;}
int cat_indice_por_imdb(const char *s){indexCalls++;for(int i=0;i<cat_n();i++){const CatItem *c=cat_item(i);if(!strcmp(s,c->imdb))return i;}return -1;}
int cat_historico_estado_tempo(const char *s,const char *t,long long *ms){(void)s;(void)t;*ms=0;return watched[0]?1:-1;}
int vistoep_estado(const char *s,int t,int e){(void)t;(void)e;return watched[atoi(s+2)];}
void vistoep_definir(const char *s,int t,int e,int v){(void)t;(void)e;watched[atoi(s+2)]=v;episodeRevision++;}
const char *i18n(const char *s){return s;}
static unsigned long long historyGeneration=1;
unsigned long long cat_historico_geracao(void){return historyGeneration;}
const char *desc_chave_tmdb(void){return "test";}
const char *desc_tmdb_idioma(void){return "en";}
char *rede_baixar(const char *url,int timeout){(void)timeout;char b[500];int id=0;if(strstr(url,"/tv/"))return strdup(strstr(url,"/season/")?"{\"cast\":[{\"id\":123}],\"episodes\":[{\"episode_number\":1}]}":"{\"number_of_seasons\":1,\"imdb_id\":\"tt1\"}");sscanf(strstr(url,"/movie/")+7,"%d",&id);snprintf(b,sizeof b,"{\"imdb_id\":\"tt%d\",\"credits\":{\"cast\":[{\"name\":\"Actor %d\"},{\"name\":\"Other %d\"},{\"name\":\"Third %d\"}]}}",id,id,id,id);return strdup(b);}
static void finish(int op){assert(scheduled);Pedido *p=scheduled;scheduled=NULL;assert(p->op==op);worker(p);fd_atualizar();}
static void unchanged(void){
 FpsAmostra sample;fps_zerar(&sample);
 indexCalls=itemReads=stateCalls=0;
 for(int frame=0;frame<120;frame++){
  struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);
  fd_atualizar();
  clock_gettime(CLOCK_MONOTONIC,&b);
  fps_add(&sample,(b.tv_sec-a.tv_sec)*1000.0+(b.tv_nsec-a.tv_nsec)/1e6,0);
 }
 char line[400];fps_linha(&sample,"filmography-300-catalog-2000-state",line,sizeof line);puts(line);
 printf("unchanged: lookups/frame=%.0f catalog reads/frame=%.0f states/frame=%.0f\n",indexCalls/120.0,itemReads/120.0,stateCalls/120.0);fflush(stdout);
 assert(indexCalls==0&&itemReads==0&&stateCalls==0);
}
static void refreshed(void){
 indexCalls=stateCalls=0;fd_atualizar();
 assert(stateCalls==300&&indexCalls<=300);
 unchanged();
}
static void performance(void){
 char body[60000];size_t used=(size_t)snprintf(body,sizeof body,"{\"name\":\"A\",\"cast\":[");
 for(int i=0;i<300;i++)used+=(size_t)snprintf(body+used,sizeof body-used,"%s{\"id\":%d,\"title\":\"Movie %d\",\"release_date\":\"2020-01-01\"}",i?",":"",i+1,i+1);
 snprintf(body+used,sizeof body-used,"]}");
 catalogCount=2000;
 for(int i=0;i<catalogCount;i++){catalog[i].tmdb=i+1;snprintf(catalog[i].imdb,sizeof catalog[i].imdb,"tt%d",i+1);strcpy(catalog[i].tipo,"movie");}
 assert(pessoa_ler(123,body,"2026-01-01"));fd_abrir();assert(ficha.n==300);
 unchanged();
 catalog[0].progresso=42;catalogRevision++;refreshed();assert(ficha.st[0].pct==42);
 watched[0]=1;catalogRevision++;refreshed();assert(ficha.st[0].estado==FM_VISTO);
 watched[0]=0;historyGeneration++;refreshed();assert(ficha.st[0].pct==42);
 assert(pessoa_ler(123,body,"2026-01-01"));refreshed();assert(ficha.n==300);
 // Resolve an out-of-catalog credit only after enrichment supplies its IMDb id.
 catalogCount=1999;catalogRevision++;refreshed();
 remota=ficha;remota.c[0].tmdb=9000;strcpy(remota.c[0].imdb,"tt2000");
 catalog[1999].progresso=73;catalogCount=2000;
 operacao=1;alvo=0;paresN=0;resultadoGeracao=historyGeneration;resultado=1;
 refreshed();assert(ficha.st[0].pct==73&&fd_preparado(0));
 // Unknown IDs and failed remote results must also settle into a quiet cache.
 remota=ficha;remota.c[0].tmdb=9001;strcpy(remota.c[0].imdb,"missing");resultado=-1;
 refreshed();assert(ficha.st[0].estado==FM_NAO);
 // A late episode mark changes state without changing the catalog revision.
 remota=ficha;remota.c[0].tv=1;remota.c[0].eps=1;strcpy(remota.c[0].imdb,"tt1");
 paresN=1;pares[0]=(VistoPar){1,1};resultado=1;refreshed();
 vistoep_definir("tt1",1,1,1);refreshed();assert(ficha.st[0].estado==FM_VISTO);
 fd_abrir();assert(ficha.st[0].pct==42);unchanged();
 puts("filmography invalidation and idle lookup budget: PASS");
}
int main(int argc,char **argv){
 if(argc>1&&!strcmp(argv[1],"performance")){performance();return 0;}
 if(argc>1&&!strcmp(argv[1],"publication")){
  for(int scenario=0;scenario<5;scenario++){
   pessoa_ler(123,"{\"name\":\"A\",\"cast\":[{\"id\":1,\"title\":\"M1\"},{\"id\":2,\"title\":\"M2\"}]}","2026-01-01");fd_abrir();
   Pedido *p=novo(1,1);assert(p);
   if(scenario!=4){worker(p);assert(resultado);}
   if(scenario==2)historyGeneration++;
   if(scenario==0||scenario==1||scenario==4)
    pessoa_ler(scenario==4?123:456,"{\"name\":\"B\",\"cast\":[{\"id\":99,\"title\":\"B movie\"}]}","2026-01-01");
   if(scenario==0||scenario==3||scenario==4)fd_abrir();
   else fd_atualizar();
   if(scenario==4){worker(p);fd_atualizar();}
   assert(ficha.c[0].tmdb==(scenario==2||scenario==3?1:99));
   assert(!fd_preparado(1));assert(!episodios[1].carregado);
   assert(!resultado&&!vivo);
  }
  worker(novo(1,0));fd_atualizar();assert(fd_preparado(0));
  puts("stale result publication: PASS");return 0;
 }
 if(argc>1&&!strcmp(argv[1],"provenance")){
  const char *body="{\"name\":\"A\",\"cast\":[{\"id\":1,\"title\":\"Shared\"}]}";
  pessoa_ler(123,body,"2026-01-01");fd_abrir();
  marcas[0].como=FM_COMO_DIRETO;strcpy(marcas[0].em,"2026-01-01");assert(gravar(0));watched[0]=1;
  pessoa_ler(456,body,"2026-01-01");fd_abrir();
  assert(ficha.st[0].manual&&ficha.st[0].como==FM_COMO_DIRETO);
  puts("cross-person provenance: PASS");return 0;
 }
 if(argc>1&&!strcmp(argv[1],"scheduling")){
  pessoa_ler(123,"{\"name\":\"A\",\"cast\":[{\"id\":1,\"name\":\"S1\",\"media_type\":\"tv\"},{\"id\":2,\"name\":\"S2\",\"media_type\":\"tv\"}]}","2026-01-01");
  fd_abrir();assert(scheduled&&((Pedido*)scheduled)->op==5);
  fd_enriquecer(0);fd_seguir();fd_sobre();
  finish(5);assert(scheduled&&((Pedido*)scheduled)->op==1);
  finish(1);assert(fd_preparado(0));
  finish(3);finish(4);finish(5);assert(fd_preparado(0));
  assert(!scheduled);puts("pending user actions: PASS");return 0;
 }
 if(argc>1&&!strcmp(argv[1],"quiz")){pessoa_ler(123,"{\"name\":\"A\",\"cast\":[{\"id\":1,\"title\":\"M1\",\"character\":\"C1\"},{\"id\":2,\"title\":\"M2\",\"character\":\"C2\"},{\"id\":3,\"title\":\"M3\",\"character\":\"C3\"},{\"id\":4,\"title\":\"M4\",\"character\":\"C4\"}]}","2026-01-01");fd_abrir();worker(novo(1,0));fd_atualizar();FmQuiz q;assert(fd_quiz(0,&q));puts("fresh quiz: PASS");return 0;}
 dados_iniciar(NULL);
 vivo=1;
 pessoa_ler(123,"{\"name\":\"A\",\"cast\":[{\"id\":1,\"name\":\"S1\",\"media_type\":\"tv\",\"episode_count\":1},{\"id\":2,\"name\":\"S2\",\"media_type\":\"tv\",\"episode_count\":1}]}","2026-01-01");
 fd_abrir();
 for(int k=0;k<2;k++){snprintf(ficha.c[k].imdb,16,"tt%d",k+1);remota=ficha;remota.c[k].eps=1;paresN=1;pares[0]=(VistoPar){1,1};alvo=k;operacao=1;resultadoGeracao=cat_historico_geracao();resultado=1;fd_atualizar();vistoep_definir(ficha.c[k].imdb,1,1,1);fd_atualizar();assert(ficha.st[k].estado==FM_VISTO);}
 vivo=1;fd_atualizar();assert(ficha.st[0].estado==FM_VISTO&&ficha.st[1].estado==FM_VISTO);
 puts("per-title episodes: PASS");return 0;
}
