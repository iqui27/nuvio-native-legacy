/* #497: actual discovery/catalogue/profile modules; only remote services wait.
 * The existing six montagem_cedo scenarios keep their default neighbours. */
#define MONTAGEM_PERFIS_REAIS
#define main montagem_cedo_original_main
#include "montagem_cedo.c"
#undef main
#include "perfis.h"
#include <errno.h>

static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int entered[2],released[2],finished[2];
static int mountingDone;
static void *mountHome(void *u) {
  void *result=montar(u);
  __atomic_store_n(&mountingDone,1,__ATOMIC_RELEASE);
  return result;
}
static void waitMount(void) {
  unsigned long long end=agoraMs()+3000;
  while(!__atomic_load_n(&mountingDone,__ATOMIC_ACQUIRE)&&agoraMs()<end)usleep(1000);
  assert(__atomic_load_n(&mountingDone,__ATOMIC_ACQUIRE));
}
static void remoteWait(int social) {
  pthread_mutex_lock(&gate);
  entered[social]=1;pthread_cond_broadcast(&changed);
  while(!released[social])pthread_cond_wait(&changed,&gate);
  __atomic_store_n(&finished[social],1,__ATOMIC_RELEASE);pthread_cond_broadcast(&changed);
  pthread_mutex_unlock(&gate);
}
static void releaseRemote(int social) {
  pthread_mutex_lock(&gate);released[social]=1;
  pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
}
static void waitEntered(void) {
  struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_sec+=3;
  pthread_mutex_lock(&gate);
  while(!entered[0]||!entered[1])assert(pthread_cond_timedwait(&changed,&gate,&until)!=ETIMEDOUT);
  pthread_mutex_unlock(&gate);
}
static int countCW(void) {
  CatItem copy[4];return cat_copiar_fileira("continue_watching",copy,4,NULL);
}
static int cwRunning(void) {
  pthread_mutex_lock(&cwFilaTrava);int active=cwVivo;
  pthread_mutex_unlock(&cwFilaTrava);return active;
}
static int catalogCount(void) {
  int rows=0;
  for(int i=0;i<cat_n_fileiras();i++)if(cat_fileira(i)->catId[0])rows++;
  return rows;
}
static void waitCW(int count) {
  unsigned long long end=agoraMs()+3000;
  CatItem copy[4];int n;
  do {
    n=cat_copiar_fileira("continue_watching",copy,4,NULL);
    if(n==count&&(!count||!strcmp(copy[0].imdb,"ttCW")))return;
    usleep(1000);
  }while(agoraMs()<end);
  assert(!"expected updated Continue Watching did not publish");
}
static void verifyRows(int cw,int social) {
  assert(countCW()==cw&&catalogCount()==3);
  int found=0;
  for(int i=0;i<cat_n_fileiras();i++) {
    const CatFileira *row=cat_fileira(i);
    assert(row->ini>=0&&row->ini+row->n<=cat_n());
    if(!strcmp(row->chave,"continue_watching"))assert(i==0);
    if(!strcmp(row->chave,"social_activity")){found++;assert(i==(cw?1:0));}
    if(row->catId[0])for(int j=0;j<row->n;j++)
      assert(!strncmp(cat_item(row->ini+j)->titulo,row->catId,strlen(row->catId)));
  }
  assert(found==social);
}
int main(int argc,char **argv) {
  assert(argc==2);
  int cached=!strcmp(argv[1],"cached"),clear=!strcmp(argv[1],"clear");
  int stale=!strcmp(argv[1],"stale");
  assert(cached||clear||stale||!strcmp(argv[1],"fresh"));
  const char *root=getenv("NV_T_DIR");assert(root&&*root);
  snprintf(dirDados,sizeof dirDados,"%s",root);
  desc_tmdb(dirDados);homeestado_iniciar();fixtureCW=1;fixtureCWEmpty=clear;fixtureSocial=1;
  if(cached||clear){
    CatItem old={0};CatFileira row={0};snprintf(old.imdb,sizeof old.imdb,"ttCached");
    snprintf(old.tipo,sizeof old.tipo,"movie");old.progresso=20;
    snprintf(row.chave,sizeof row.chave,"continue_watching");row.n=1;
    cat_definir_tudo(&old,1,&row,1);
  }
  fixtureTrakt=remoteWait;montagemSessao=nv_perfil_sessao_ler();
  pthread_t worker;buscando=1;assert(!pthread_create(&worker,NULL,mountHome,NULL));
  waitEntered();
  unsigned long long end=agoraMs()+3000;
  while(catalogCount()!=3&&agoraMs()<end)usleep(1000);
  assert(catalogCount()==3&&!released[0]&&!released[1]);
  assert(countCW()==(cached||clear));
  puts("PASS catalog rows publish while Continue Watching and social requests are blocked");
  if(stale){
    perfis_invalidar_sessao();montagemGeracao++;descobertaInicializada=1;
    aberturaFontesProntas=0;aberturaPendente=1;aberturaSessao=nv_perfil_sessao_ler();
    waitMount();assert(!pthread_join(worker,NULL));
    CatItem marker={0};CatFileira row={0};snprintf(marker.imdb,sizeof marker.imdb,"ttNewProfile");
    snprintf(row.chave,sizeof row.chave,"new_profile");row.n=1;cat_definir_tudo(&marker,1,&row,1);
    releaseRemote(0);releaseRemote(1);
    end=agoraMs()+3000;
    while((cwRunning()||!__atomic_load_n(&finished[1],__ATOMIC_ACQUIRE))&&agoraMs()<end)usleep(1000);
    assert(!cwRunning()&&cat_n()==1&&!strcmp(cat_item(0)->imdb,"ttNewProfile"));
    puts("PASS superseded profile cannot publish late Continue Watching or social rows");
  }else{
    releaseRemote(0);waitCW(clear?0:1);verifyRows(clear?0:1,0);
    releaseRemote(1);waitMount();assert(!pthread_join(worker,NULL));
    waitCW(clear?0:1);verifyRows(clear?0:1,1);
    desc_remontar_fileiras();verifyRows(clear?0:1,1);
    end=agoraMs()+3000;while(cwRunning()&&agoraMs()<end)usleep(1000);assert(!cwRunning());
    puts("PASS cached/updated/empty Continue Watching survives catalog publication and remount");
  }
  assert(__atomic_load_n(&fixtureAddonListSnapshots, __ATOMIC_RELAXED) > 0);
  assert(!fixtureAddonListDepth);
  puts("cw_prioridade: ok");return 0;
}
