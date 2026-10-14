#include "filmodados.h"
#include "pessoa.h"
#include "catalogo.h"
#include "progresso.h"
#include "vistoep.h"
#include "dados.h"
#include "perfis.h"
#include "sessao.h"
#include "rede.h"
#include "recomenda.h"
#include "trakt.h"
#include "simkl.h"
#include "simklauth.h"
#include "nuvem.h"
#include "js.h"
#include "jsw.h"
#include "conquistasdados.h"
#include "conquistas.h"
#include "ajustes.h"
#include "contapend.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
#define TMDB "https://api.themoviedb.org/3"
#include "descoberta.h"
static FdFicha ficha,remota;
static unsigned rev;
static pthread_mutex_t trava=PTHREAD_MUTEX_INITIALIZER;
static int vivo,resultado,alvo=-1,preparado=-1,paresN,operacao;
static VistoPar pares[FD_EPS];
static char aviso[160];
// Persistent per-account/profile provenance; imported manual history cannot bypass quiz.
typedef struct {int como,release;char em[11];long falha[2];} Marca;
static Marca marcas[FM_MAX];
static char arquivo[220];
static void hoje(char *s){time_t t=time(NULL);struct tm tm;localtime_r(&t,&tm);strftime(s,11,"%Y-%m-%d",&tm);}
static void nomeArquivo(void){char hex[160];size_t n=0;const unsigned char *p=(const unsigned char*)sessao_usuario();while(*p&&n+2<sizeof hex){snprintf(hex+n,3,"%02x",*p++);n+=2;}hex[n]=0;snprintf(arquivo,sizeof arquivo,"filmo-%s-p%d-%ld.txt",n?hex:"anon",perfis_ativo(),ficha.pessoa);}
static int gravar(void){char *b=calloc(FM_MAX,120);size_t n=0;if(!b)return 0;for(int i=0;i<ficha.n;i++){Marca *m=marcas+i;if(m->como||m->falha[0]||m->falha[1])n+=(size_t)snprintf(b+n,FM_MAX*120-n,"%ld %d %d %d %s %ld %ld\n",ficha.c[i].tmdb,ficha.c[i].tv,m->como,m->release,m->em[0]?m->em:"-",m->falha[0],m->falha[1]);}int ok=dados_gravar(arquivo,b);free(b);return ok;}
static char *tmdb(const char *path){char url[600];const char *k=desc_chave_tmdb();if(!k||!*k)return NULL;snprintf(url,sizeof url,TMDB "%s%sapi_key=%s&language=%s",path,strchr(path,'?')?"&":"?",k,desc_tmdb_idioma());return rede_baixar(url,20);}
static char *servico(const char *path,const char *body,int *st){const char *cab[4];char aut[3200],via[40],pf[40],url[700];*st=0;if(!NV_REC_URL[0]||!recomenda_cabecalhos(cab,aut,sizeof aut,via,sizeof via,pf,sizeof pf))return NULL;snprintf(url,sizeof url,"%s%s",NV_REC_URL,path);return body?rede_postar_st(url,20,cab,body,st):rede_baixar_st(url,20,cab,st);}
static void linhas(const char *b,const char *key,FdLinha *out,int *n,int max){const char *p=js_array(b,NULL,key);*n=0;while(p&&*n<max){const char *f=js_fim(p);FdLinha *l=out+(*n)++;memset(l,0,sizeof *l);js_texto(p,f,"nome",l->nome,sizeof l->nome);js_texto(p,f,"rel",l->rel,sizeof l->rel);l->id=(long)js_num(p,f,"tmdb",0);l->ano=(int)js_num(p,f,"ano",0);p=js_prox(f);}}
static int temPessoa(const char *p,const char *f,const char *key,long id){const char *e=js_array(p,f,key);while(e){const char *ef=js_fim(e);if((long)js_num(e,ef,"id",0)==id)return 1;e=js_prox(ef);}return 0;}
static int carregaEps(FdFicha *s,int idx,VistoPar *out){char path[200];FmCredito *c=s->c+idx;snprintf(path,sizeof path,"/tv/%ld?append_to_response=external_ids",c->tmdb);char *b=tmdb(path);if(!b)return -1;int seasons=(int)js_num(b,NULL,"number_of_seasons",0),n=0;js_texto(b,NULL,"imdb_id",c->imdb,sizeof c->imdb);free(b);if(seasons<1||seasons>64)return -1;
  for(int season=1;season<=seasons;season++){
    snprintf(path,sizeof path,"/tv/%ld/season/%d?append_to_response=credits",c->tmdb,season);b=tmdb(path);if(!b)return -1;
    int regular=temPessoa(b,NULL,"cast",s->pessoa);
    const char *e=js_array(b,NULL,"episodes");while(e){const char *f=js_fim(e);int episode=(int)js_num(e,f,"episode_number",0);if(episode>0&&(regular||temPessoa(e,f,"guest_stars",s->pessoa)||temPessoa(e,f,"crew",s->pessoa))){if(n>=FD_EPS){free(b);return -1;}out[n++]=(VistoPar){(short)season,(short)episode};}e=js_prox(f);}free(b);
  }c->eps=n;return n;
}
static int bloqueado(const char *b,const char *term){const char *p=js_array(b,NULL,"blocked");while(p){char t[80]="";js_cadeia(p,t,sizeof t);if(!strcmp(t,term))return 1;const char *e=p+1;while(*e){if(*e=='\\'&&e[1])e+=2;else if(*e++=='"')break;}p=js_prox(e);}return 0;}
static void prepara(FdFicha *s,int idx,VistoPar *eps,int *ne){FmCredito *c=s->c+idx;char path[200];snprintf(path,sizeof path,"/%s/%ld?append_to_response=credits,keywords,external_ids",c->tv?"tv":"movie",c->tmdb);char *b=tmdb(path);*ne=-1;if(!b)return;
  js_texto(b,NULL,"imdb_id",c->imdb,sizeof c->imdb);c->nCom=c->nKw=0;
  const char *p=js_array(b,NULL,"cast");while(p&&c->nCom<FM_COM_MAX){const char *f=js_fim(p);char t[40]="";js_texto_raiz_em(p,f,"name",t,sizeof t);if(t[0]&&strcmp(t,s->pessoa==pessoa_id()?pessoa_nome():""))snprintf(c->com[c->nCom++],40,"%s",t);p=js_prox(f);}
  snprintf(path,sizeof path,"/v1/filmografia/quiz?tmdb=%ld&tipo=%s",c->tmdb,c->tv?"tv":"movie");int st;char *block=servico(path,NULL,&st);
  // Fail closed: until the title blocklist has been checked, keywords are not quiz material.
  if(block&&st==200){p=js_array(b,NULL,c->tv?"results":"keywords");while(p&&c->nKw<FM_KW_MAX){const char *f=js_fim(p);char t[40]="";js_texto_raiz_em(p,f,"name",t,sizeof t);if(t[0]&&!fm_spoiler(t)&&!bloqueado(block,t))snprintf(c->kw[c->nKw++],40,"%s",t);p=js_prox(f);}}
  free(block);free(b);*ne=c->tv?carregaEps(s,idx,eps):0;
}
// Build the actual history payload once for both providers, including each episode's date.
static char *historico(const FmCredito *c,const VistoPar *eps,int n,const char *date,int released,int trakt){Jsw w;jsw_iniciar(&w);jsw_obj_ini(&w);jsw_chave(&w,c->tv?"shows":"movies");jsw_arr_ini(&w);jsw_obj_ini(&w);jsw_chave(&w,"ids");jsw_obj_ini(&w);jsw_cs(&w,"imdb",c->imdb);jsw_obj_fim(&w);if(c->tv){jsw_chave(&w,"seasons");jsw_arr_ini(&w);for(int i=0;i<n;i++){int first=1;for(int j=0;j<i;j++)if(eps[j].temporada==eps[i].temporada)first=0;if(!first)continue;jsw_obj_ini(&w);jsw_ci(&w,"number",eps[i].temporada);jsw_chave(&w,"episodes");jsw_arr_ini(&w);for(int j=i;j<n;j++)if(eps[j].temporada==eps[i].temporada){jsw_obj_ini(&w);jsw_ci(&w,"number",eps[j].episodio);char iso[32];snprintf(iso,sizeof iso,"%sT12:00:00Z",date);jsw_cs(&w,"watched_at",released&&trakt?"released":iso);jsw_obj_fim(&w);}jsw_arr_fim(&w);jsw_obj_fim(&w);}jsw_arr_fim(&w);}else{char iso[32];snprintf(iso,sizeof iso,"%sT12:00:00Z",date);jsw_cs(&w,"watched_at",released&&trakt?"released":iso);}jsw_obj_fim(&w);jsw_arr_fim(&w);jsw_obj_fim(&w);char *b=strdup(jsw_texto_final(&w));jsw_livre(&w);return b;}
typedef struct {FdFicha s;int idx,op,como,release,remover;char data[11];VistoPar eps[FD_EPS];int ne;char aut[3200],key[200],token[300];int tk,sm;unsigned long long geracao;char arquivo[220];} Pedido;
static void *worker(void *v){Pedido *p=v;int ok=1;
  if(p->op==1)prepara(&p->s,p->idx,p->eps,&p->ne);
  if(p->op==2){FmCredito *c=p->s.c+p->idx;for(int provider=0;provider<2;provider++){if(!(provider?p->sm:p->tk))continue;char url[140],auth[3300],key[220];const char *cab[4];char *b=historico(c,p->eps,p->ne,p->data,p->release,!provider);if(!b){ok=0;continue;}snprintf(url,sizeof url,"https://api.%s/%s",provider?"simkl.com":"trakt.tv",p->remover?"sync/history/remove":"sync/history");snprintf(auth,sizeof auth,"Authorization: Bearer %s",provider?p->token:p->aut);snprintf(key,sizeof key,"%s: %s",provider?"simkl-api-key":"trakt-api-key",provider?nuvem_simkl_cliente():p->key);cab[0]=auth;cab[1]=key;cab[2]=provider?NULL:"trakt-api-version: 2";cab[3]=NULL;int st=0;char *r=rede_postar_st(url,20,cab,b,&st);if(st<200||st>=300||!r)ok=0;else{const char *nf=strstr(r,"\"not_found\"");if(nf&&strstr(nf,"\"ids\""))ok=0;}free(r);free(b);}
  }
  if(p->op==3||p->op==4){char path[160],body[80];snprintf(path,sizeof path,"/v1/filmografia/pessoa?id=%ld",p->s.pessoa);if(p->op==3){snprintf(body,sizeof body,"{\"pessoa\":%ld,\"seguir\":%d}",p->s.pessoa,!p->s.seguindo);snprintf(path,sizeof path,"/v1/filmografia/seguir");}int st;char *b=servico(path,p->op==3?body:NULL,&st);ok=b&&st==200;if(ok){if(p->op==3)p->s.seguindo=!p->s.seguindo;else{p->s.seguindo=(int)js_num(b,NULL,"seguindo",0);linhas(b,"familia",p->s.familia,&p->s.nf,FD_LINHAS);linhas(b,"premios",p->s.premios,&p->s.np,FD_LINHAS);}}free(b);}
  pthread_mutex_lock(&trava);if(p->geracao==cat_historico_geracao()&&p->s.pessoa==ficha.pessoa){remota=p->s;resultado=ok?1:-1;alvo=p->idx;operacao=p->op;if(p->op==1){paresN=p->ne;if(p->ne>0)memcpy(pares,p->eps,p->ne*sizeof *pares);}}vivo=0;pthread_mutex_unlock(&trava);memset(p,0,sizeof *p);free(p);return NULL;
}
static Pedido *novo(int op,int idx){pthread_mutex_lock(&trava);if(vivo){pthread_mutex_unlock(&trava);return NULL;}Pedido *p=calloc(1,sizeof *p);if(!p){pthread_mutex_unlock(&trava);return NULL;}p->s=ficha;p->idx=idx;p->op=op;p->geracao=cat_historico_geracao();p->ne=paresN;if(paresN>0)memcpy(p->eps,pares,paresN*sizeof *pares);vivo=1;ficha.ocupado=1;resultado=0;pthread_mutex_unlock(&trava);return p;}
static int lancar(Pedido *p){pthread_t t;if(!p)return 0;if(pthread_create(&t,NULL,worker,p)){free(p);pthread_mutex_lock(&trava);vivo=0;ficha.ocupado=0;pthread_mutex_unlock(&trava);return 0;}pthread_detach(t);return 1;}
void fd_abrir(void){memset(&ficha,0,sizeof ficha);memset(marcas,0,sizeof marcas);rev=0;preparado=-1;aviso[0]=0;fd_atualizar();}
static void contar(int avisar){FmConquista cq[FM_CQ_MAX];int nc=fm_conquistas(ficha.pessoa,pessoa_nome(),ficha.c,ficha.st,ficha.n,cq);if(!ajustes_conquistas())return;CqEstado s,before;cqd_carregar(&s);before=s;for(int i=0;i<nc;i++){FmConquista *c=cq+i;int up=cqregra_registrar(&s,c->chave,!strncmp(c->chave,"dir:",4)?CQ_FAM_DIRETOR:CQ_FAM_EXPLORADOR,pessoa_nome(),c->nivel,c->visto,c->total,(long)time(NULL));if(up&&!avisar)for(int j=0;j<s.n;j++)if(!strcmp(s.e[j].chave,c->chave))s.e[j].avisado=s.e[j].nivel;}if(memcmp(&before,&s,sizeof s)&&cqd_gravar(&s)&&avisar)conquistas_aviso_armar();}
void fd_atualizar(void){int imported=0;
  if(pessoa_pronta()&&rev!=pessoa_revisao()) {rev=pessoa_revisao();ficha.pessoa=pessoa_id();ficha.n=pessoa_n_creditos();memcpy(ficha.c,pessoa_creditos(),ficha.n*sizeof *ficha.c);ficha.pronto=1;nomeArquivo();memset(marcas,0,sizeof marcas);char *b=dados_ler(arquivo),*ctx;for(char *l=b?strtok_r(b,"\n",&ctx):NULL;l;l=strtok_r(NULL,"\n",&ctx)){long id;int tv;Marca m={0};if(sscanf(l,"%ld %d %d %d %10s %ld %ld",&id,&tv,&m.como,&m.release,m.em,m.falha,m.falha+1)==7)for(int i=0;i<ficha.n;i++)if(ficha.c[i].tmdb==id&&ficha.c[i].tv==tv)marcas[i]=m;}free(b);imported=1;}
  pthread_mutex_lock(&trava);int res=resultado,op=operacao,i=alvo;resultado=0;
  if(res){if(op==1){ficha.c[i]=remota.c[i];preparado=paresN>=0?i:-1;}else if(op==3||op==4){ficha.seguindo=remota.seguindo;ficha.nf=remota.nf;ficha.np=remota.np;memcpy(ficha.familia,remota.familia,sizeof ficha.familia);memcpy(ficha.premios,remota.premios,sizeof ficha.premios);}ficha.erro=res<0;snprintf(aviso,sizeof aviso,"%s",res<0?"Não foi possível sincronizar. Tente novamente.":op==2?"Histórico sincronizado.":"");}ficha.ocupado=vivo;pthread_mutex_unlock(&trava);
  for(int k=0;k<ficha.n;k++){FmCredito *c=ficha.c+k;int idx=c->imdb[0]?cat_indice_por_imdb(c->imdb):-1;if(idx<0)for(int j=0;j<cat_n();j++){const CatItem *ci=cat_item(j);if(ci&&ci->tmdb==c->tmdb&&(!strcmp(ci->tipo,"series"))==c->tv){idx=j;snprintf(c->imdb,sizeof c->imdb,"%.15s",ci->imdb);break;}}
    long long ms=0;int hist=c->imdb[0]?cat_historico_estado_tempo(c->imdb,c->tv?"series":"movie",&ms):-1;int pct=hist==1?100:idx>=0?cat_item(idx)->progresso:0;int eps=0;if(c->tv&&preparado==k&&paresN>0)for(int e=0;e<paresN;e++)eps+=vistoep_estado(c->imdb,pares[e].temporada,pares[e].episodio)==1;char date[11]="";if(ms>0){time_t t=(time_t)(ms/1000);struct tm tm;gmtime_r(&t,&tm);strftime(date,sizeof date,"%Y-%m-%d",&tm);}Marca *m=marcas+k;ficha.st[k]=fm_estado(c,pct,c->tv?eps:-1,date,m->como,m->em);
  }if(imported||res==1)contar(!imported&&op==2);
}
const FdFicha *fd_ficha(void){return &ficha;}
void fd_enriquecer(int i){if(i<0||i>=ficha.n||preparado==i)return;lancar(novo(1,i));}
int fd_preparado(int i){return preparado==i;}
int fd_tentativas(int i){return i>=0&&i<ficha.n?fm_tentativas(marcas[i].falha,2,(long)time(NULL)):0;}
void fd_falhou(int i){Marca *m=marcas+i;m->falha[0]=m->falha[1];m->falha[1]=(long)time(NULL);gravar();}
int fd_quiz(int i,FmQuiz *q){if(!fd_preparado(i))return 0;const char *dp[FM_MAX],*dc[FM_MAX],*dt[FM_MAX];int np=0,nc=0,nt=0;for(int k=0;k<ficha.n;k++){if(k==i)continue;FmCredito *c=ficha.c+k;if(c->personagem[0])dp[np++]=c->personagem;for(int j=0;j<c->nCom&&nc<FM_MAX;j++)dc[nc++]=c->com[j];for(int j=0;j<c->nKw&&nt<FM_MAX;j++)dt[nt++]=c->kw[j];}fm_quiz_montar(q,ficha.c+i,pessoa_nome(),0,dp,np,dc,nc,dt,nt,(unsigned)time(NULL));return q->nq>=2;}
int fd_marcar(int i,int como,int release,int remover){if(!fd_preparado(i)||i<0||i>=ficha.n||!ficha.c[i].imdb[0]||(ficha.c[i].tv&&paresN<=0))return 0;Pedido *p=novo(2,i);if(!p)return 0;char today[11];hoje(today);fm_data_vista(ficha.c[i].lancamento,release,today,p->data,sizeof p->data);p->como=como;p->release=release;p->remover=remover;const char *cab[4];char aut[3200],key[200];p->tk=trakt_ativo()&&trakt_cabecalhos(cab,aut,sizeof aut,key,sizeof key);if(p->tk){snprintf(p->aut,sizeof p->aut,"%s",strstr(aut,"Bearer ")?strstr(aut,"Bearer ")+7:"");snprintf(p->key,sizeof p->key,"%s",strstr(key,": ")?strstr(key,": ")+2:"");}p->sm=simkl_ativo();if(p->sm)simklauth_credencial_copiar(p->token,sizeof p->token);
  Marca old=marcas[i];marcas[i].como=remover?0:como;marcas[i].release=release;snprintf(marcas[i].em,sizeof marcas[i].em,"%s",p->data);if(!gravar()){marcas[i]=old;free(p);pthread_mutex_lock(&trava);vivo=0;ficha.ocupado=0;pthread_mutex_unlock(&trava);return 0;}FmCredito *c=ficha.c+i;if(c->tv){for(int j=0;j<paresN;j++)vistoep_definir(c->imdb,pares[j].temporada,pares[j].episodio,!remover);contapend_episodios(c->imdb,"series",pares,paresN,!remover);}else{cat_historico_definir_id(c->imdb,"movie",!remover);contapend_titulo(c->imdb,"movie",!remover);}int ok=lancar(p);if(ok)contapend_chutar();return ok;
}
void fd_seguir(void){lancar(novo(3,-1));}
void fd_sobre(void){lancar(novo(4,-1));}
const char *fd_aviso(void){return aviso;}
