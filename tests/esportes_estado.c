#define _POSIX_C_SOURCE 200809L
#include "esportes.h"
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t sessao = 1;
static int perfil = 1, falharGravacao;
/* Freeze UTC around a provider/date-boundary regression. */
static time_t agoraTeste=1792411200;
time_t time(time_t *out) { time_t t=agoraTeste; if(out)*out=t; return t; }
static char salvoNome[160], salvo[27000];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int entrou, liberado, pedidos,pedidosFav,modoFav,segurarFav,entrouFav,modoCatalogo,pedidosCatalogo,pedidosLinha;
static int segurarLinha, entrouLinha, erroLinha, segurarDatas, entrouData, pedidosPadrao;
int ajustes_idioma(void) { return 1; }
static const char *eventosNfl="{\"events\":["
 "{\"id\":\"game\",\"date\":\"2026-10-21T12:00Z\",\"name\":\"Team at Other\",\"competitions\":[{\"competitors\":[{\"team\":{\"id\":\"team\",\"displayName\":\"Team\"}},{\"team\":{\"id\":\"other\",\"displayName\":\"Other\"}}]}]},"
 "{\"id\":\"unrelated\",\"date\":\"2026-10-21T12:00Z\",\"name\":\"Wrong at Nope\",\"competitions\":[{\"competitors\":[{\"team\":{\"id\":\"wrong\",\"displayName\":\"Wrong\"}},{\"team\":{\"id\":\"nope\",\"displayName\":\"Nope\"}}]}]}]}";
static const char *eventosSoccer="{\"events\":["
 "{\"id\":\"cup\",\"date\":\"2026-10-22T12:00Z\",\"name\":\"Team at Rival\",\"league\":{\"slug\":\"uefa.champions\",\"name\":\"UEFA Champions League\"},\"competitions\":[{\"competitors\":[{\"team\":{\"id\":\"team\",\"displayName\":\"Team\"}},{\"team\":{\"id\":\"rival\",\"displayName\":\"Rival\"}}]}]},"
 "{\"id\":\"league\",\"date\":\"2026-10-23T12:00Z\",\"name\":\"Team at Rival\",\"league\":{\"slug\":\"eng.1\",\"name\":\"Premier League\"},\"competitions\":[{\"competitors\":[{\"team\":{\"id\":\"team\",\"displayName\":\"Team\"}},{\"team\":{\"id\":\"rival\",\"displayName\":\"Rival\"}}]}]}]}";
uint64_t perfis_sessao_geracao(void) { return sessao; }
int perfis_sessao_atual(uint64_t s) { return s == sessao; }
int perfis_ativo(void) { return perfil; }
const char *sessao_usuario(void) { return "test-account"; }
char *dados_ler(const char *nome) { return !strcmp(nome,salvoNome) ? strdup(salvo) : NULL; }
int dados_gravar(const char *nome, const char *t) {
  if (falharGravacao) return 0;
  snprintf(salvoNome,sizeof salvoNome,"%s",nome); snprintf(salvo,sizeof salvo,"%s",t); return 1;
}
int dados_gravar_leve(const char *nome, const char *t) { (void)nome; (void)t; return 1; }
char *rede_baixar_medido_controle(const char *url,int s,const char *const *h,const RedeControle *c,RedeMedida *m) {
  (void)h;assert(s==8 && c->max_bytes==2*1024*1024 && strstr(url,"lang=en"));
  if(modoCatalogo) {
    m->status=200;
    if(strstr(url,"/leagues/dropdown")) {
      pedidosCatalogo++;assert(strstr(url,"sport=football"));
      return strdup("{\"leagues\":[{\"slug\":\"nfl\",\"name\":\"NFL\"},{\"slug\":\"college-football\",\"name\":\"College football\"},{\"slug\":\"third\",\"name\":\"Third\"},{\"slug\":\"fourth\",\"name\":\"Fourth\"},{\"slug\":\"fifth\",\"name\":\"Fifth\"},{\"slug\":\"sixth\",\"name\":\"Sixth\"},{\"slug\":\"seventh\",\"name\":\"Seventh\"},{\"slug\":\"eighth\",\"name\":\"Eighth\"},{\"slug\":\"ninth\",\"name\":\"Ninth\"}]}");
    }
    pthread_mutex_lock(&mutex);pedidosLinha++;entrouLinha=1;
    int data=strstr(url,"dates=")!=NULL;if(data)entrouData=1;else pedidosPadrao++;
    pthread_cond_broadcast(&cond);
    while(segurarLinha || (segurarDatas && data))pthread_cond_wait(&cond,&mutex);
    int falha=erroLinha;pthread_mutex_unlock(&mutex);
    assert(strstr(url,"/scoreboard?") && !strstr(url,"/schedule?"));
    if(falha){m->status=503;return NULL;}return strdup(eventosNfl);
  }
  int fav=strstr(url,"/schedule?")!=NULL || (modoFav==4 && strstr(url,"/scoreboard?"));
  pthread_mutex_lock(&mutex);
  int numero=fav?++pedidosFav:++pedidos;entrou=1;if(fav)entrouFav=1;pthread_cond_broadcast(&cond);
  while(!liberado || (fav && segurarFav))pthread_cond_wait(&cond,&mutex);
  pthread_mutex_unlock(&mutex);m->status=200;
  if(fav) {
    if(modoFav==4 && strstr(url,"/schedule?")){m->status=404;return NULL;}
    if(strstr(url,"/soccer/")){assert(strstr(url,"/soccer/all/teams/") && strstr(url,"fixture=true"));return strdup(eventosSoccer);}
    return strdup(modoFav && modoFav!=5?eventosNfl:"{\"events\":[]}");
  }
  if(strstr(url,"/leagues/dropdown")) {
    assert(strstr(url,"sport=football"));return strdup("{\"leagues\":[{\"slug\":\"nfl\",\"name\":\"NFL\"}]}");
  }
  assert(strstr(url,"/sports/football/nfl/scoreboard?") && strstr(url,"limit=400"));
  const char *d=strstr(url,"dates=");
  if(d) {time_t t=time(NULL)+(numero-4)*86400;struct tm tm;char esperado[16];gmtime_r(&t,&tm);strftime(esperado,sizeof esperado,"%Y%m%d",&tm);assert(!strncmp(d+6,esperado,8));}
  return strdup("{\"events\":[]}");
}
static void pausa(void) { struct timespec t={0,1000000}; nanosleep(&t,NULL); }
static void drenar(void) {
  int i;
  for (i=0;i<10000 && (esportes_carregando() || esportes_favoritos_carregando());i++) { esportes_passo(); pausa(); }
  assert(i<10000); esportes_passo();
}
int main(void) {
  esportes_abrir_sessao();
  assert(esportes_fundo_qualidade()==0 && esportes_fundo_definir_qualidade(1));
  falharGravacao=1;assert(!esportes_fundo_definir_qualidade(2) && esportes_fundo_qualidade()==1);falharGravacao=0;
  assert(!esportes_fundo_definir_qualidade(-1) && !esportes_fundo_definir_qualidade(3));
  sessao++;esportes_abrir_sessao();assert(esportes_fundo_qualidade()==1);
  assert(esportes_fundo_definir_qualidade(0));
  assert(esportes_habilitado(1)); assert(esportes_habilitar(1,0));
  assert(!esportes_habilitado(1));
  esportes_pedir_ligas(1,1); assert(!esportes_carregando()); assert(!pedidos);
  falharGravacao=1; assert(!esportes_habilitar(1,1)); assert(!esportes_habilitado(1)); falharGravacao=0;
  sessao++; esportes_abrir_sessao(); assert(!esportes_habilitado(1));
  assert(esportes_habilitar(1,1));
  EspEvento e={0}; strcpy(e.modalidade,"football"); strcpy(e.ligaId,"nfl");
  e.nParticipantes=1; strcpy(e.participantes[0].id,"team");
  assert(esportes_alternar_favorito(&e,0)); assert(esportes_favorito(&e,0));
  falharGravacao=1; assert(!esportes_alternar_favorito(&e,0)); assert(esportes_favorito(&e,0)); falharGravacao=0;
  sessao++; esportes_abrir_sessao(); assert(esportes_favorito(&e,0));
  /* Old public catalog results cannot populate the next profile. */
  esportes_pedir_ligas(1,1); esportes_passo();
  pthread_mutex_lock(&mutex); while(!entrou) pthread_cond_wait(&cond,&mutex); pthread_mutex_unlock(&mutex);
  sessao++; perfil=2; esportes_abrir_sessao(); assert(!esportes_favorito(&e,0));
  pthread_mutex_lock(&mutex); liberado=1; pthread_cond_broadcast(&cond); pthread_mutex_unlock(&mutex);
  drenar(); assert(!esportes_ligas_n());
  esportes_pedir_ligas(1,1); drenar(); assert(esportes_ligas_n()==1);
  EspLiga l=*esportes_liga(0); assert(!strcmp(l.id,"nfl"));
  esportes_pedir_eventos(1,&l,0); drenar(); assert(!esportes_erro() && !esportes_eventos_n());
  esportes_pedir_ligas(1,1); esportes_cancelar(); drenar();
  assert(pedidos==10);
  /* Combined feed: different sports, soccer cups and duplicate opponents. */
  modoFav=1;sessao++;perfil=3;esportes_abrir_sessao();
  assert(esportes_alternar_favorito(&e,0));
  EspEvento outro=e;strcpy(outro.participantes[0].id,"other");assert(esportes_alternar_favorito(&outro,0));
  EspEvento soccer=e;strcpy(soccer.modalidade,"soccer");strcpy(soccer.ligaId,"eng.1");
  assert(esportes_alternar_favorito(&soccer,0));drenar();
  assert(esportes_favoritos_times()==3 && esportes_favoritos_n()==3 && !esportes_favoritos_erro());
  int cup=0,game=0;
  for(int k=0;k<esportes_favoritos_n();k++) {
    const EspEvento *v=esportes_favoritos_evento(k);
    if(!strcmp(v->id,"cup")){cup++;assert(!strcmp(v->ligaId,"uefa.champions") && !strcmp(v->liga,"UEFA Champions League"));}
    if(!strcmp(v->id,"game"))game++;
  }
  assert(cup==1 && game==1);
  EspEvento college=e;strcpy(college.ligaId,"college-football");assert(!esportes_favorito(&college,0));
  strcpy(soccer.ligaId,"uefa.champions");assert(esportes_favorito(&soccer,0));
  falharGravacao=1;assert(!esportes_alternar_favorito(&soccer,0) && esportes_favorito(&soccer,0));falharGravacao=0;
  assert(esportes_alternar_favorito(&soccer,0));drenar();assert(esportes_favoritos_n()==1);
  assert(esportes_habilitar(1,0));drenar();assert(!esportes_favoritos_n() && !esportes_favoritos_times());
  assert(esportes_habilitar(1,1));drenar();assert(esportes_favoritos_n()==1);
  /* A slow favorites fetch cannot block the foreground league or leak profiles. */
  pthread_mutex_lock(&mutex);segurarFav=1;entrouFav=0;pthread_mutex_unlock(&mutex);
  esportes_pedir_favoritos(1);esportes_passo();
  pthread_mutex_lock(&mutex);while(!entrouFav)pthread_cond_wait(&cond,&mutex);pthread_mutex_unlock(&mutex);
  esportes_pedir_ligas(1,1);
  for(int k=0;k<3000 && esportes_carregando();k++){esportes_passo();pausa();}
  assert(esportes_ligas_n()==1 && esportes_favoritos_carregando());
  sessao++;perfil=4;esportes_abrir_sessao();
  pthread_mutex_lock(&mutex);segurarFav=0;pthread_cond_broadcast(&cond);pthread_mutex_unlock(&mutex);
  drenar();assert(!esportes_favoritos_n() && !esportes_favoritos_times());
  /* Unsupported team APIs fall back to the existing eight bounded scoreboards. */
  modoFav=4;int antesFav=pedidosFav;assert(esportes_alternar_favorito(&e,0));drenar();
  assert(esportes_favoritos_n()==1 && pedidosFav-antesFav==9 && !esportes_favoritos_erro());
  modoFav=5;esportes_pedir_favoritos(1);drenar();assert(!esportes_favoritos_n());
  /* League rows: persist visibility separately from whole-sport visibility. */
  modoCatalogo=1;sessao++;perfil=5;esportes_abrir_sessao();
  for(int i=0;i<esportes_modalidades_n();i++)if(i!=1)assert(esportes_habilitar(i,0));
  assert(esportes_liga_habilitada(1,"nfl"));
  falharGravacao=1;assert(!esportes_liga_habilitar(1,"nfl",0) && esportes_liga_habilitada(1,"nfl"));falharGravacao=0;
  assert(esportes_liga_habilitar(1,"nfl",0));assert(!esportes_liga_habilitada(1,"nfl") && esportes_liga_habilitada(2,"nfl"));
  sessao++;esportes_abrir_sessao();assert(!esportes_liga_habilitada(1,"nfl"));
  falharGravacao=1;assert(!esportes_liga_habilitar(1,"nfl",1) && !esportes_liga_habilitada(1,"nfl"));falharGravacao=0;
  esportes_catalogo_iniciar(-1,0);esportes_passo();drenar();
  assert(pedidosCatalogo==1 && esportes_catalogo_n()==9 && !pedidosLinha);
  assert(esportes_catalogo_modalidade(0)==1 && !strcmp(esportes_catalogo_liga(0)->id,"nfl"));
  esportes_catalogo_visiveis();esportes_catalogo_pedir(0);esportes_passo();drenar();assert(!pedidosLinha); /* Hidden rows never fetch. */
  assert(esportes_liga_habilitar(1,"nfl",1));
  esportes_catalogo_pedir(0);esportes_passo();drenar();
  assert(pedidosLinha==8 && esportes_catalogo_eventos_n(0)==2 && esportes_catalogo_estado(0)==1);
  assert(!strcmp(esportes_catalogo_evento(0,0)->ligaId,"nfl"));
  esportes_catalogo_pedir(0);esportes_passo();drenar();assert(pedidosLinha==8); /* Cached, not polled per frame. */
  /* At most eight row payloads survive; switching rows evicts the oldest. */
  for(int i=1;i<9;i++) {
    esportes_catalogo_visiveis();esportes_catalogo_pedir(i);esportes_passo();drenar();
    assert(esportes_catalogo_eventos_n(i)==2);
  }
  assert(!esportes_catalogo_eventos_n(0) && !esportes_catalogo_evento(0,0));
  assert(!strcmp(esportes_catalogo_evento(8,0)->ligaId,"ninth"));
  /* League cards do not prefetch scoreboards. Opening a recent league reuses
   * the bounded cache; old/failed refreshes keep its visible cards intact. */
  esportes_catalogo_suspender();EspLiga nona=*esportes_catalogo_liga(8),nfl=*esportes_catalogo_liga(0);
  int pedidosAntes=pedidosLinha;
  esportes_pedir_eventos(1,&nona,0);assert(esportes_eventos_n()==2 && pedidosLinha==pedidosAntes);
  assert(!strcmp(esportes_evento(0)->ligaId,"ninth"));drenar();assert(pedidosLinha==pedidosAntes);
  esportes_pedir_eventos(1,&nfl,0);assert(!esportes_eventos_n());drenar();assert(pedidosLinha==pedidosAntes+8);
  assert(esportes_eventos_n()==2 && !strcmp(esportes_evento(0)->ligaId,"nfl"));
  pedidosAntes=pedidosLinha;esportes_pedir_eventos(1,&nona,0);assert(esportes_eventos_n()==2);drenar();
  assert(pedidosLinha==pedidosAntes && !strcmp(esportes_evento(0)->ligaId,"ninth"));
  agoraTeste+=61;
  pthread_mutex_lock(&mutex);segurarLinha=1;entrouLinha=0;erroLinha=1;pthread_mutex_unlock(&mutex);
  esportes_pedir_eventos(1,&nona,0);assert(esportes_eventos_n()==2);esportes_passo();
  pthread_mutex_lock(&mutex);while(!entrouLinha)pthread_cond_wait(&cond,&mutex);
  pthread_mutex_unlock(&mutex);assert(esportes_eventos_n()==2);
  pthread_mutex_lock(&mutex);segurarLinha=0;pthread_cond_broadcast(&cond);pthread_mutex_unlock(&mutex);
  drenar();assert(esportes_erro() && esportes_eventos_n()==2);
  erroLinha=0;esportes_pedir_eventos(1,&nona,0);assert(esportes_eventos_n()==2);drenar();assert(!esportes_erro());
  pedidosAntes=pedidosLinha;esportes_pedir_eventos(1,&nona,0);drenar();assert(pedidosLinha==pedidosAntes);
  esportes_pedir_eventos(1,&nona,1);assert(esportes_eventos_n()==2);drenar();assert(pedidosLinha==pedidosAntes+1);
  /* Back/reopen during date expansion resumes the remaining date pages;
   * it does not fetch a default scoreboard that would blank cached games. */
  pthread_mutex_lock(&mutex);segurarDatas=1;entrouData=0;pthread_mutex_unlock(&mutex);
  esportes_pedir_eventos(1,&nfl,0);
  int aguardando=0;
  for(int k=0;k<3000;k++) {
    esportes_passo();pausa();pthread_mutex_lock(&mutex);aguardando=entrouData;pthread_mutex_unlock(&mutex);if(aguardando)break;
  }
  assert(aguardando && esportes_eventos_n()==2 && !strcmp(esportes_evento(0)->ligaId,"nfl"));
  esportes_cancelar();pthread_mutex_lock(&mutex);segurarDatas=0;pthread_cond_broadcast(&cond);pthread_mutex_unlock(&mutex);drenar();
  pedidosAntes=pedidosPadrao;esportes_pedir_eventos(1,&nfl,0);assert(esportes_eventos_n()==2);drenar();assert(pedidosPadrao==pedidosAntes);
  /* An in-flight old-profile league response cannot revive the cached cards. */
  pthread_mutex_lock(&mutex);segurarLinha=1;entrouLinha=0;pthread_mutex_unlock(&mutex);
  esportes_pedir_eventos(1,&nona,1);esportes_passo();
  pthread_mutex_lock(&mutex);while(!entrouLinha)pthread_cond_wait(&cond,&mutex);pthread_mutex_unlock(&mutex);
  esportes_cancelar();sessao++;perfil=6;esportes_abrir_sessao();
  pthread_mutex_lock(&mutex);segurarLinha=0;pthread_cond_broadcast(&cond);pthread_mutex_unlock(&mutex);
  drenar();assert(!esportes_eventos_n());
  assert(!esportes_catalogo_n() && !esportes_catalogo_eventos_n(8) && esportes_liga_habilitada(1,"nfl"));
  puts("sports profile settings, league visibility, bounded league caching, nonblank refresh, cancellation and URL contracts: passed");return 0;
}
