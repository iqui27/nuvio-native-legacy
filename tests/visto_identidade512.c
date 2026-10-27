// #512: real watched enqueue/worker with deterministic offline service edges.
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int criarFila(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
static int destacarFila(pthread_t);
#define pthread_create criarFila
#define pthread_detach destacarFila
#include "../src/visto.c"
#undef pthread_create
#undef pthread_detach

static void *(*pendente)(void *);
static void *pendenteArg;
static int falharFio, travaPerfil, perfil = 1, trocarDurante;
static unsigned versaoLista = 1;
static int indiceLista = 7, tvdbLista = 11209906;
static const char *idLista = "tt21975436", *tipoLista = "series";
static int consultas, porId, porNumero, simklEnvios, diarios, drenagens;
static int confirmados;
static long long gestoMs = 100;
static int ultimoVisto, perfilTrakt, perfilSimkl, perfilDiario, tvdbDiario[VT_LOTE];
static VistoPar paresTrakt[VT_LOTE], paresSimkl[VT_LOTE], paresDiario[VT_LOTE];
static TraktEpisodioAlvo alvosRecebidos[VT_LOTE];
static int criarFila(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg) {
  (void)a; if (falharFio) return EAGAIN;
  assert(!pendente); memset(t, 0, sizeof *t); pendente=f; pendenteArg=arg; return 0;
}
static int destacarFila(pthread_t t) { (void)t; return 0; }
static void executarFila(void) {
  void *(*f)(void *)=pendente; void *a=pendenteArg;
  assert(f); pendente=NULL; pendenteArg=NULL; f(a);
}
uint64_t perfis_sessao_geracao(void) { return (uint64_t)perfil; }
int perfis_sessao_atual(uint64_t g) { return g==(uint64_t)perfil; }
int perfis_sessao_travar(uint64_t g) {
  assert(!travaPerfil); if (!perfis_sessao_atual(g)) return 0; travaPerfil=1; return 1;
}
void perfis_sessao_destravar(void) { assert(travaPerfil); travaPerfil=0; }
int cat_episodio_tvdb(int i, const char *id, int t, int e) {
  assert(travaPerfil); consultas++;
  int r=i==indiceLista && !strcmp(id,idLista) && !strcmp(tipoLista,"series") && t==2 && e==2 ? tvdbLista : 0;
  if (trocarDurante) versaoLista++;
  return r;
}
unsigned cat_versao_episodios(void) { return versaoLista; }
int trakt_ativo(void) { return 1; }
int simkl_ativo(void) { return 1; }
int sessao_logada(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int trakt_destino_capturar(TraktDestino *d) {
  assert(travaPerfil); memset(d,0,sizeof *d); d->sessao=(uint64_t)perfil;
  snprintf(d->aut,sizeof d->aut,"fixture-trakt-profile-%d",perfil); return 1;
}
int simkl_destino_capturar(SimklDestino *d) {
  assert(travaPerfil); memset(d,0,sizeof *d); d->sessao=(uint64_t)perfil;
  snprintf(d->aut,sizeof d->aut,"fixture-simkl-profile-%d",perfil); return 1;
}
int trakt_episodios_marcar_alvos_destino(const TraktDestino *d,const char *id,const TraktEpisodioAlvo *a,int n,int v) {
  assert(travaPerfil && !strcmp(id,"tt21975436") && n>0 && n<=VT_LOTE);
  assert(!strcmp(d->aut,"fixture-trakt-profile-1"));
  porId++; ultimoVisto=v; perfilTrakt=(int)d->sessao;
  memcpy(alvosRecebidos,a,(size_t)n*sizeof *a);
  for (int i=0;i<n;i++) paresTrakt[i]=a[i].par;
  return 1;
}
int trakt_episodios_marcar_destino(const TraktDestino *d,const char *id,const VistoPar *p,int n,int v) {
  (void)id; assert(travaPerfil); porNumero++; ultimoVisto=v; perfilTrakt=(int)d->sessao;
  assert(!strcmp(d->aut,"fixture-trakt-profile-1"));
  memcpy(paresTrakt,p,(size_t)n*sizeof *p); return 1;
}
int trakt_episodios_marcar(const char *id,const VistoPar *p,int n,int v) {
  (void)id;(void)p;(void)n;(void)v;assert(!"captured Trakt destination required");return 0;
}
int trakt_titulo_marcar_destino(const TraktDestino *d,const char *id,const char *tipo,int v) {
  (void)d;(void)id;(void)tipo;(void)v;assert(!"unexpected title path");return 0;
}
int simkl_episodios_marcar_destino(const SimklDestino *d,const char *id,const VistoPar *p,int n,int v) {
  (void)id;(void)v;assert(travaPerfil);simklEnvios++;perfilSimkl=(int)d->sessao;
  assert(!strcmp(d->aut,"fixture-simkl-profile-1"));
  memcpy(paresSimkl,p,(size_t)n*sizeof *p);return 1;
}
int simkl_episodios_marcar(const char *id,const VistoPar *p,int n,int v) {
  (void)id;(void)p;(void)n;(void)v;assert(!"captured Simkl destination required");return 0;
}
int simkl_titulo_marcar_destino(const SimklDestino *d,const char *i,const char *t,const int *p,int n,int v) {
  (void)d;(void)i;(void)t;(void)p;(void)n;(void)v;assert(!"unexpected title path");return 0;
}
int simkl_titulo_marcar(const char *i,const char *t,const int *p,int n,int v) {
  (void)i;(void)t;(void)p;(void)n;(void)v;assert(!"unexpected title path");return 0;
}
int contapend_episodios(const char *id,const char *tipo,const VistoPar *p,int n,int v) {
  (void)v;assert(travaPerfil && !strcmp(id,"tt21975436") && !strcmp(tipo,"series"));
  diarios++;perfilDiario=perfil;memcpy(paresDiario,p,(size_t)n*sizeof *p);return n;
}
int contapend_titulo(const char *id,const char *tipo,int v) {
  (void)id;(void)tipo;(void)v;assert(!"unexpected title journal");return 0;
}
void contapend_reenviar_com(int (*f)(const char *,const char *,const VistoPar *,int,int,int,const int *,int,const int *)) {
  (void)f;
}
long long contapend_gesto_alvos(const char *id,const char *tipo,const VistoPar *p,
                          const int *tvdb,int n,int v,int destinos,
                          const int *temps,int nt) {
  (void)v;(void)temps;(void)nt;
  if (!(travaPerfil && !strcmp(id,"tt21975436")))
    fprintf(stderr, "unexpected journal: lock=%d id=%s type=%s\n", travaPerfil,
            id ? id : "(null)", tipo ? tipo : "(null)");
  assert(travaPerfil && !strcmp(id,"tt21975436"));
  if (destinos & VISTO_CONTA) {
    diarios++; perfilDiario=perfil; memcpy(paresDiario,p,(size_t)n*sizeof *p);
  }
  for (int i=0;i<n;i++) tvdbDiario[i]=tvdb ? tvdb[i] : 0;
  return ++gestoMs;
}
long long contapend_gesto(const char *id,const char *tipo,const VistoPar *p,int n,
                          int v,int destinos,const int *temps,int nt) {
  return contapend_gesto_alvos(id,tipo,p,NULL,n,v,destinos,temps,nt);
}
int contapend_gesto_atual(const char *id,int t,int e,long long ms) {
  (void)t;(void)e; return travaPerfil && !strcmp(id,"tt21975436") && ms>0;
}
void contapend_confirmar_gesto(const char *id,const VistoPar *p,int n,long long ms,int destinos) {
  (void)id;(void)p;(void)n;(void)ms; assert(travaPerfil); confirmados |= destinos;
}
int contapend_reenviar_overrides(const char *id,long long ms) { (void)id;(void)ms; return 0; }
int contapend_enviar(void) { drenagens++;return 0; }
void vistoep_titulo_gesto(const char *id,int v) { (void)id;(void)v;assert(!"enqueue changed local marks"); }
static void zerarTeste(void) {
  assert(!pendente && !travaPerfil);perfil=1;falharFio=trocarDurante=0;
  indiceLista=7;idLista="tt21975436";tipoLista="series";tvdbLista=11209906;versaoLista=1;
  consultas=porId=porNumero=simklEnvios=diarios=drenagens=confirmados=0;
  ultimoVisto=perfilTrakt=perfilSimkl=perfilDiario=0;
  memset(alvosRecebidos,0,sizeof alvosRecebidos);memset(tvdbDiario,0,sizeof tvdbDiario);
}
int main(void) {
  VistoPar p[2]={{2,2},{2,3}},original[2]={{2,2},{2,3}};
  assert(sizeof(VistoPar)==2*sizeof(short));
  zerarTeste();assert(visto_episodios_item(7,"tt21975436","series",p,2,1,VISTO_TRAKT));
  assert(consultas==2 && !porId);tvdbLista=77;idLista="tt-other";p[0].episodio=99;
  executarFila();p[0]=original[0];
  assert(porId==1 && !porNumero && perfilTrakt==1);
  assert(alvosRecebidos[0].tvdbEp==11209906 && alvosRecebidos[1].tvdbEp==0);
  assert(tvdbDiario[0]==11209906 && tvdbDiario[1]==0);
  assert(!memcmp(paresTrakt,original,sizeof original));
  puts("ok  queued targets survive catalog and caller-buffer replacement");
  zerarTeste();assert(visto_episodios_item(7,"tt21975436","series",p,2,1,VISTO_TRAKT));
  perfil=2;executarFila();assert(!porId && !porNumero);
  puts("ok  profile replacement cancels the old profile worker");
  zerarTeste();assert(visto_episodios("tt21975436","series",p,2,1,VISTO_TRAKT));executarFila();
  assert(!consultas && porNumero==1 && !porId);puts("ok  legacy caller never guesses a catalog copy");
  zerarTeste();tvdbLista=0;assert(visto_episodios_item(7,"tt21975436","series",p,2,1,VISTO_TRAKT));executarFila();
  assert(porNumero==1 && !porId);puts("ok  missing or ambiguous identity keeps numeric targets");
  zerarTeste();assert(visto_episodios_item(7,"tt21975436","movie",p,2,1,VISTO_TRAKT));executarFila();
  assert(!consultas && porNumero==1 && !porId);puts("ok  caller movie type cannot use episode identity");
  zerarTeste();tipoLista="movie";assert(visto_episodios_item(7,"tt21975436","series",p,2,1,VISTO_TRAKT));executarFila();
  assert(porNumero==1 && !porId);puts("ok  same-ID movie replacement cannot supply episode identity");
  zerarTeste();idLista="tt-other";assert(visto_episodios_item(7,"tt21975436","series",p,2,1,VISTO_TRAKT));executarFila();
  assert(porNumero==1 && !porId);puts("ok  a reused index cannot supply another title identity");
  zerarTeste();trocarDurante=1;
  assert(visto_episodios_item(7,"tt21975436","series",p,2,1,VISTO_TRAKT|VISTO_SIMKL|VISTO_CONTA));
  assert(diarios==1 && perfilDiario==1 && !memcmp(paresDiario,original,sizeof original));
  executarFila();
  assert(!porNumero && !porId && simklEnvios==1 && drenagens==1);
  assert((confirmados & (VISTO_TRAKT|VISTO_SIMKL)) == (VISTO_TRAKT|VISTO_SIMKL));
  assert(!memcmp(paresSimkl,original,sizeof original));
  puts("ok  changed snapshot suppresses Trakt while preserving other destinations");
  zerarTeste();assert(visto_episodios_item(7,"tt21975436","series",p,2,0,VISTO_TRAKT|VISTO_SIMKL|VISTO_CONTA));
  assert(diarios==1 && perfilDiario==1 && !memcmp(paresDiario,original,sizeof original));executarFila();
  assert(porId==1 && ultimoVisto==0 && simklEnvios==1 && perfilSimkl==1 && drenagens==1);
  assert(!memcmp(paresSimkl,original,sizeof original) && !memcmp(paresTrakt,original,sizeof original));
  puts("ok  unwatch preserves Simkl, account journal and captured destinations");
  zerarTeste();assert(visto_episodios_item(7,"tt21975436","series",p,2,1,VISTO_SIMKL|VISTO_CONTA));executarFila();
  assert(!consultas && !porId && !porNumero && simklEnvios==1 && diarios==1);
  puts("ok  non-Trakt destinations perform no identity lookup");
  zerarTeste();falharFio=1;assert(!visto_episodios_item(7,"tt21975436","series",p,2,1,VISTO_TRAKT|VISTO_CONTA));
  assert(diarios==1 && !pendente && !porId && !porNumero && !drenagens && !travaPerfil);
  puts("ok  worker creation failure preserves the account journal");
  zerarTeste();int retryTvdb[2]={11209906,0};
  assert(perfis_sessao_travar(1));
  assert(visto_reenviar("tt21975436","series",p,2,1,VISTO_TRAKT,NULL,0,retryTvdb)==VISTO_TRAKT);
  perfis_sessao_destravar();
  assert(porId==1 && !porNumero && alvosRecebidos[0].tvdbEp==11209906);
  puts("ok  journal retry preserves exact TVDB targets");
  puts("visto_identidade512: 12 scenarios passed");return 0;
}
