#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/rewatchui.c"
static char conta[96]="account-a";
static int perfil=1, deletes, syncDeletes, trocar;
static unsigned long long cred=1;
static void *(*worker)(void *); static void *job;
const char *sessao_usuario(void){return conta;}
int perfis_ativo(void){return perfil;}
unsigned long long trakt_credencial_geracao(void){return cred;}
int pthread_create(pthread_t *t,const pthread_attr_t *a,void *(*f)(void *),void *u){worker=f;job=u;return 0;}
int pthread_detach(pthread_t t){return 0;}
int trakt_playback_remover(const char *id){deletes++; if(trocar) strcpy(conta,"account-b"); return 1;}
int trakt_progresso_ocultar(const char *id,int o){assert(0); return 0;}
int syncprog_remover(const char *id){syncDeletes++;return 1;}
int desc_tirar_continuar(const char *id,int t,int e){return 1;}
void desc_refazer_continuar(void){}
void sync_sujar_progresso(void){}
int rewatch_iniciar(const char *id,int o){return 1;}
void rewatch_encerrar(const char *id){}
void rewatch_parcial(const char *id,int t,int e,int pct){}
void prog_chave(char *d,unsigned n,const char *id,int t,int e){snprintf(d,n,"%s:%d:%d",id,t,e);}
static void launch(void){
 strcpy(conta,"account-a"); perfil=1;cred=1;deletes=syncDeletes=trocar=0;
 strcpy(rwItem.imdb,"tt1");rwItem.temporada=2;rwItem.episodio=8;
 rwN=3;rwFoco=1;rwFim=0;rwConflito=1;rwEscolher();assert(worker && job);
}
int main(void){
 launch();strcpy(conta,"account-b");worker(job);assert(!deletes && !syncDeletes);
 launch();perfil=2;worker(job);assert(!deletes && !syncDeletes);
 launch();cred=2;worker(job);assert(!deletes && !syncDeletes);
 launch();trocar=1;worker(job);assert(deletes==1 && !syncDeletes);
 launch();worker(job);assert(deletes==1 && syncDeletes==1);
}
