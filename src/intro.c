#include "intro.h"
#include "creditosjson.h"
#include "rede.h"
#include "js.h"
#include "credfonte.h"
#include "dados.h"
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t trava=PTHREAD_MUTEX_INITIALIZER;
static IntroTrecho trechos[8];static int nTrechos;static unsigned geracao;
static IntroTrecho creditosLimitados[8];static int nCreditosLimitados;
static int botaoIdx=-1;static double botaoDesde;
static int botaoVis;static double botaoFim;static int botaoTipo;
// Trechos que o PROPRIO ARQUIVO declara (capitulos do MKV, 203-capitulos): valem
// mais que TheIntroDB/AniSkip, porque descrevem ESTE corte. Ficam a parte para
// sobreviver a uma resposta da rede que chegue depois (baixar) e sao fundidos
// por tipo: um tipo que o arquivo declara substitui o da rede.
static IntroTrecho capTr[4];static int nCapTr;
static void fundirCapitulos(void);   // chamar COM a trava
static unsigned verGer;
// Memoria unica do automatico, inclusive recuo antes de uma resposta tardia.
static unsigned autoGer;static double autoFim[4],autoDe,autoAte;static int autoRecuou;
static void autoSincronizar(void){
  if(autoGer!=geracao){memset(autoFim,0,sizeof autoFim);autoRecuou=0;autoGer=geracao;}
}


// AS QUATRO CHAVES QUE A API DEVOLVE, e o tipo de cada uma (as mesmas do
// plugin oficial do TheIntroDB para o Stremio: intro, recap, credits, preview).
// "preview" e a previa do PROXIMO episodio depois dos creditos: nao ganha botao
// proprio (seria mais uma frase em 30 idiomas numa 2.0.3 so de consertos);
// emendada nos creditos, o "Pular creditos" pula as duas, e sem creditos ela
// marca o fim do episodio para o cartao do proximo (intro_creditos_seg).
static const struct { const char *chave; int tipo; } CHAVES[] = {
  { "intro",   INTRO_ABERTURA },
  { "recap",   INTRO_RESUMO   },
  { "credits", INTRO_CREDITOS },
  { "preview", INTRO_PREVIA   },
};

int intro_extrair(const char *j,IntroTrecho *out,int max){
  size_t k;int n=0;
  if(!j||!out||max<1)return 0;
  for(k=0;k<sizeof CHAVES/sizeof CHAVES[0];k++){
    // ARRAY e nao objeto: o TheIntroDB devolve uma LISTA por chave, porque um
    // episodio pode ter mais de um trecho do mesmo tipo. O servico anterior
    // mandava um objeto so, e por isso o leitor antigo usava strstr + js_fim.
    const char *p=js_array(j,NULL,CHAVES[k].chave);
    for(;p&&n<max;p=js_prox(js_fim(p))){
      const char *f=js_fim(p);
      // MILISSEGUNDOS, nao segundos — a outra diferenca de formato. Trocar as
      // unidades daria um numero mil vezes errado sem parecer errado.
      double a=js_num(p,f,"start_ms",-1),b=js_num(p,f,"end_ms",-1);
      // `start_ms: null` quer dizer ZERO (o trecho comeca junto com a midia) e
      // `end_ms: null` quer dizer ATE O FIM. js_num devolve o padrao nos dois
      // casos, entao -1 aqui e "veio nulo", nao "veio errado".
      // Inicio E fim nulos (ou zero): trecho sem informacao. O plugin oficial
      // descarta (`start_ms > 0 || end_ms > 0`); aqui viraria "a midia inteira".
      if(a<=0&&b<=0)continue;
      if(a<0)a=0;
      if(b<0)b=-1000.0;             // marcador de "sem fim", tratado abaixo
      if(b>=0&&b<=a)continue;       // trecho invertido ou vazio: descarta
      out[n++]=(IntroTrecho){a/1000.0,(b<0)?0.0:b/1000.0,CHAVES[k].tipo,0};
    }
  }
  return n;
}

static int buscarInterno(const char *url,char **corpo,int *status);
// --- ANISKIP (anime, 2.0.3) ----------------------------------------------
//
// api.aniskip.com/v2/skip-times/<malId>/<episodio>?types[]=op&types[]=ed&
// types[]=recap&episodeLength=<s>, sem chave. Indexado pelo MyAnimeList: o id
// do MAL vem do proprio item ("mal:21") ou do Kitsu ("kitsu:12" -> mappings do
// Kitsu, um GET sem chave, guardado). Numero do episodio = o do item, so na
// temporada 1 para ids kitsu:/mal:. IMDb/TMDB usa a lista Fribb por temporada
// e offset (abaixo), somente com evidencia local de anime do TMDB.
//
// CADA RESULTADO TRAZ `episodeLength`, a duracao do lancamento em que foi
// marcado. Como o AIOMetadata: fica o mais perto da nossa duracao, e se ele
// difere mais que INTRO_ANISKIP_TOLERA (10%) e outro corte e nao vale.
int intro_extrair_aniskip(const char *j,double dur,IntroTrecho *out,int max){
  static const struct{const char *nome;int tipo;}T[]={
    {"\"op\"",INTRO_ABERTURA},{"\"ed\"",INTRO_CREDITOS},{"\"recap\"",INTRO_RESUMO}};
  const char *p;int n=0;size_t k;
  double melhorDif[3]={-1,-1,-1};int melhorIdx[3]={-1,-1,-1};
  IntroTrecho cand[3];
  if(!j||!out||max<1)return 0;
  p=js_array(j,NULL,"results");
  for(;p;p=js_prox(js_fim(p))){
    const char *f=js_fim(p),*tp;
    double a=js_num(p,f,"startTime",-1),b=js_num(p,f,"endTime",-1),len=js_num(p,f,"episodeLength",0);
    double dif;
    tp=strstr(p,"\"skipType\"");
    if(!tp||tp>f||b<=a||a<0)continue;
    for(k=0;k<3;k++){
      const char *v=strstr(tp,T[k].nome);
      if(!v||v>f||v-tp>16)continue;
      // DURACAO: sem a nossa ou sem a do lancamento nao ha como comparar.
      dif=(dur>1.0&&len>1.0)?(len>dur?len-dur:dur-len)/dur:0.0;
      if(dur>1.0&&len>1.0&&dif>INTRO_ANISKIP_TOLERA)break;
      if(melhorIdx[k]<0||dif<melhorDif[k]){
        melhorIdx[k]=1;melhorDif[k]=dif;
        // fim no fim da midia (ED ate o ultimo segundo): "ate o fim".
        cand[k]=(IntroTrecho){a,(len>1.0&&b>=len-0.5)?0.0:b,T[k].tipo,1};
      }
      break;
    }
  }
  for(k=0;k<3&&n<max;k++)if(melhorIdx[k]>=0)out[n++]=cand[k];
  return n;
}

void intro_montar_url_aniskip(char *url,size_t n,long mal,int ep,double dur){
  snprintf(url,n,"https://api.aniskip.com/v2/skip-times/%ld/%d?types[]=op&types[]=ed&types[]=recap&episodeLength=%.0f",
           mal,ep,dur>1.0?dur:0.0);
}

long intro_kitsu_mal(const char *j){
  const char *p=j?js_array(j,NULL,"data"):NULL;
  for(;p;p=js_prox(js_fim(p))){
    const char *f=js_fim(p);char site[48]="",ext[24]="";
    js_texto(p,f,"externalSite",site,sizeof site);
    js_texto(p,f,"externalId",ext,sizeof ext);
    if(!strcmp(site,"myanimelist/anime")&&ext[0])return atol(ext);
  }
  return 0;
}

static struct{long kitsu,mal;}kmCache[16];static int kmProx;
static long malDe(const char *id){
  long k,m=0;int i,st=0;char url[200],*j=NULL;
  if(!strncmp(id,"mal:",4))return atol(id+4);
  if(strncmp(id,"kitsu:",6))return 0;
  k=atol(id+6);if(k<=0)return 0;
  pthread_mutex_lock(&trava);
  for(i=0;i<16;i++)if(kmCache[i].kitsu==k){m=kmCache[i].mal;break;}
  pthread_mutex_unlock(&trava);
  if(i<16)return m;
  snprintf(url,sizeof url,"https://kitsu.io/api/edge/anime/%ld/mappings?filter[externalSite]=myanimelist/anime",k);
  buscarInterno(url,&j,&st);
  if(st>=200&&st<=299&&j)m=intro_kitsu_mal(j);
  free(j);
  if(st==200||st==404){       // so guarda resposta de verdade (sem MAL = 0)
    pthread_mutex_lock(&trava);
    kmCache[kmProx].kitsu=k;kmCache[kmProx].mal=m;kmProx=(kmProx+1)%16;
    pthread_mutex_unlock(&trava);
  }
  return m;
}

// Evidencia do MESMO corpo /tv do TMDB que extras.c ja baixa: zero GET novo.
static struct{char id[24];long tmdb;int anime;}animeCache[16];static int animeProx;
static int animeDe(const char *id,long tmdb){ // COM trava
  for(int i=0;i<16;i++)if((id[0]&&!strcmp(animeCache[i].id,id))||
      (tmdb>0&&animeCache[i].tmdb==tmdb))return animeCache[i].anime;
  return 0;
}

// Fribb: indice local semanal, registros de 28 bytes; nunca guarda a lista.
#define ANIME_LIST_URL "https://raw.githubusercontent.com/Fribb/anime-lists/master/anime-list-full.json"
#define MAP_SEMANA (7*24*60*60)
#define MAP_ARQ "anime-fribb.idx"
typedef struct{uint32_t imdb,tmdb,mal;int32_t t[2],offset[2];}AnimeMapa;
typedef struct{uint32_t magic,n,hash;}MapaCab;
static pthread_mutex_t mapTrava=PTHREAD_MUTEX_INITIALIZER;
static uint32_t mapaHash(uint32_t h,const void *p,size_t n){
  const unsigned char *s=p;while(n--)h=(h^*s++)*16777619u;return h;
}
static int32_t mapaNumero(const char *j,const char *chave,int32_t padrao){
  double n=js_num(j,NULL,chave,padrao);
  return n>=INT32_MIN&&n<=INT32_MAX&&(double)(int32_t)n==n?(int32_t)n:padrao;
}
static uint32_t imdbNumero(const char *id){
  char *fim;unsigned long n;
  if(strncmp(id,"tt",2))return 0;
  n=strtoul(id+2,&fim,10);return *fim||n>UINT32_MAX?0:(uint32_t)n;
}
// ponytail: um objeto de ate 16 KiB; se crescer, falha sem substituir o velho.
typedef struct{FILE *f;MapaCab cab;char obj[16384];size_t n;int estado,prof,aspas,escape;}MapaFluxo;
static int mapaObjeto(MapaFluxo *c){
  const char *j=c->obj;char ids[2048]="",md[64]="",season[64]="",offset[64]="";
  AnimeMapa v={0};v.mal=(uint32_t)mapaNumero(j,"mal_id",0);
  if((int32_t)v.mal<=0)return 1;
  js_bruto(j,NULL,"themoviedb_id",md,sizeof md);
  v.tmdb=(uint32_t)mapaNumero(md,"tv",0);
  js_bruto(j,NULL,"season",season,sizeof season);
  js_bruto(j,NULL,"episode_offset",offset,sizeof offset);
  v.t[0]=mapaNumero(season,"tvdb",-1);v.t[1]=mapaNumero(season,"tmdb",-1);
  v.offset[0]=mapaNumero(offset,"tvdb",0);v.offset[1]=mapaNumero(offset,"tmdb",0);
  if(js_tem(j,NULL,"imdb_id")&&!js_bruto(j,NULL,"imdb_id",ids,sizeof ids))return 0;
  // Uma linha por IMDb; TMDB sem IMDb tambem vale. Duplicatas sao inofensivas.
  const char *p=ids;int n=0;
  while((p=strchr(p,'"'))){char id[24]="";if(p[1]!='"'&&!js_cadeia(p,id,sizeof id))return 0;
    p=strchr(p+1,'"');if(!p)return 0;p++;
    v.imdb=imdbNumero(id);if(!v.imdb)continue;
    if(fwrite(&v,sizeof v,1,c->f)!=1)return 0;
    c->cab.hash=mapaHash(c->cab.hash,&v,sizeof v);c->cab.n++;n++;
  }
  if(!n&&v.tmdb){v.imdb=0;if(fwrite(&v,sizeof v,1,c->f)!=1)return 0;
    c->cab.hash=mapaHash(c->cab.hash,&v,sizeof v);c->cab.n++;}
  return 1;
}
static int mapaReceber(const void *dados,size_t n,void *u){
  MapaFluxo *c=u;const unsigned char *p=dados;
  while(n--){int ch=*p++;
    if(!c->prof){
      if(isspace(ch))continue;
      if(c->estado==0&&ch=='['){c->estado=1;continue;}
      if((c->estado==1||c->estado==2)&&ch=='{'){c->prof=1;c->n=0;}
      else if((c->estado==1||c->estado==3)&&ch==']'){c->estado=4;continue;}
      else if(c->estado==3&&ch==','){c->estado=2;continue;}
      else return 0;
    }else if(c->aspas){if(c->escape)c->escape=0;else if(ch=='\\')c->escape=1;else if(ch=='"')c->aspas=0;}
    else if(ch=='"')c->aspas=1;
    else if(ch=='{')c->prof++;
    else if(ch=='}')c->prof--;
    if(!ch||c->n+1>=sizeof c->obj)return 0;
    c->obj[c->n++]=(char)ch;
    if(!c->prof){c->obj[c->n]=0;dados_fs_travar();int ok=mapaObjeto(c);dados_fs_liberar();if(!ok)return 0;c->estado=3;}
  }
  return 1;
}
// ponytail: consulta sequencial sem cache RAM; ordenar se o indice ficar lento.
static int mapaLer(const char *arq,uint32_t imdb,long tmdb,int t,int ep,long *mal,int *melhor){
  FILE *f=fopen(arq,"rb");MapaCab h;AnimeMapa v;uint32_t hash=2166136261u,n=0;
  *mal=0;*melhor=-1;if(!f)return 0;
  int ok=fread(&h,sizeof h,1,f)==1&&h.magic==0x31425246u;
  while(ok&&fread(&v,sizeof v,1,f)==1){
    hash=mapaHash(hash,&v,sizeof v);n++;
    int col=tmdb>0?1:0;
    if((tmdb>0?v.tmdb==(uint32_t)tmdb:imdb&&v.imdb==imdb)&&v.t[col]==t&&v.offset[col]>=0&&v.offset[col]<ep){
      if(v.offset[col]>*melhor){*melhor=v.offset[col];*mal=v.mal;}
      else if(v.offset[col]==*melhor&&*mal!=v.mal)*mal=0;
    }
  }
  ok=ok&&!ferror(f)&&n==h.n&&hash==h.hash&&ftell(f)==(long)(sizeof h+(size_t)n*sizeof v);
  fclose(f);if(!ok)*mal=0;return ok;
}
static int mapaBaixar(const char *arq){
  char tmp[1024];MapaFluxo c={0};RedePedido q={0};RedeResposta r={0};int ok;
  snprintf(tmp,sizeof tmp,"%s.tmp",arq);
  dados_fs_travar();c.f=fopen(tmp,"wb");
  if(!c.f){dados_fs_liberar();return 0;}
  c.cab.magic=0x31425246u;c.cab.hash=2166136261u;
  ok=fwrite(&c.cab,sizeof c.cab,1,c.f)==1;
  dados_fs_liberar();
  q.url=ANIME_LIST_URL;q.prazo_ms=30000;q.max_bytes=REDE_CORPO_MAXIMO;
  q.receber_corpo=mapaReceber;q.corpo_usuario=&c;
  if(ok)ok=rede_pedir(&q,&r)&&r.status==200&&c.estado==4&&c.cab.n>0;
  rede_resposta_limpar(&r);
  dados_fs_travar();
  if(ok)ok=!fseek(c.f,0,SEEK_SET)&&fwrite(&c.cab,sizeof c.cab,1,c.f)==1;
  if(fclose(c.f))ok=0;
  if(ok)ok=!rename(tmp,arq);
  if(!ok)remove(tmp);else dados_marcar_sujo(1);
  dados_fs_liberar();return ok;
}
// QUANTAS DESCARGAS DO INDICE FALHARAM EM SEGUIDA (o arquivo retry guarda o
// numero; vazio ou ilegivel = pelo menos uma) e onde grava a proxima contagem.
// Arquivo ausente => 0 (nunca falhou).
static int retryContar(const char *retry,struct stat *st){
  FILE *f;char b[8];size_t r;int n;
  if(stat(retry,st))return 0;
  f=fopen(retry,"rb");if(!f)return 1;
  r=fread(b,1,sizeof b-1,f);fclose(f);
  b[r<sizeof b-1?r:sizeof b-2]=0;
  n=atoi(b);
  return n<1?1:n;
}
static void retryGravar(const char *retry,int tentativasAteAgora){
  FILE *f=fopen(retry,"wb");
  if(f){fprintf(f,"%d",tentativasAteAgora+1);fclose(f);}
}
static long malMapeado(const char *id,long tmdb,int t,int *ep){
  char arq[1024],retry[1024];long mal=0;int melhor=-1;struct stat st;time_t agora=time(NULL);
  if (!dados_caminho(arq,sizeof arq,MAP_ARQ) ||
      !dados_caminho(retry,sizeof retry,"anime-fribb.retry")) return 0;
  pthread_mutex_lock(&mapTrava);
  int valido=mapaLer(arq,imdbNumero(id),tmdb,t,*ep,&mal,&melhor);
  // SEM indice util o retry e CURTO e cresce a cada falha (10 min, 1 h, 6 h):
  // uma rede fora num dia nao vira uma semana sem intro. A semana inteira so
  // existe para adiar a RENOVACAO de um indice que ja serve (utime abaixo).
  if(!valido){
    int tent=retryContar(retry,&st);
    time_t prazo=tent>=3?6*60*60:tent==2?60*60:10*60;
    if(tent>0&&st.st_mtime<=agora&&agora-st.st_mtime<prazo){
      pthread_mutex_unlock(&mapTrava);return 0;
    }
  }
  if(!valido||stat(arq,&st)||agora-st.st_mtime>=MAP_SEMANA||st.st_mtime>agora){
    if(mapaBaixar(arq)){
      dados_fs_travar();remove(retry);dados_fs_liberar();
      mapaLer(arq,imdbNumero(id),tmdb,t,*ep,&mal,&melhor);
    }
    else if(valido){ // Mantem o indice velho e adia outra tentativa por uma semana.
      dados_fs_travar();utime(arq,NULL);dados_marcar_sujo(1);dados_fs_liberar();
    }else{
      dados_fs_travar();
      // Conta a falha no arquivo: 1 => 10 min, 2 => 1 h, 3+ => 6 h. O retry
      // antigo era vazio (uma semana inteira sem tentar de novo).
      retryGravar(retry,retryContar(retry,&st));
      dados_marcar_sujo(1);dados_fs_liberar();
    }
  }
  if(mal>0)*ep-=melhor;
  pthread_mutex_unlock(&mapTrava);return mal;
}

// O PEDIDO, no molde do plugin oficial (tidb.plugin.js, fetchData):
//   - tmdb_id with TMDB-confirmed season/episode (CatEp.tmdbE), otherwise
//     imdb_id with Cinemeta numbering. v4 converts provider coordinates
//     server-side; omit mapping because we have no verified TVDB order.
//     Reject responses that echo a different episode;
//   - duration_ms = a duracao REAL do video quando o player ja sabe, e um
//     pedido de novo quando ela chega ou muda (intro_definir_duracao);
//   - 200 = trechos; 204 (sem dados) e 404 (desconhecido) = zero trechos,
//     guardados como "nao conhece"; qualquer outra resposta (5xx, rede) NAO e
//     guardada como vazia e tenta de novo mais tarde.
typedef struct{char id[24];long tmdb;int t,e,imdbT,imdbE,anime;double durAnt,durProx,dur;unsigned g;}Pedido;

void intro_montar_url(char *url,size_t n,const char *imdb,long tmdb,int t,int e,double durSeg){
  size_t k;
  if(tmdb>0)k=(size_t)snprintf(url,n,"https://api.theintrodb.org/v4/media?tmdb_id=%ld",tmdb);
  else k=(size_t)snprintf(url,n,"https://api.theintrodb.org/v4/media?imdb_id=%s",imdb?imdb:"");
  if(k<n&&t>=0&&e>0)k+=(size_t)snprintf(url+k,n-k,"&season=%d&episode=%d",t,e);
  if(k<n&&durSeg>1.0)snprintf(url+k,n-k,"&duration_ms=%.0f",durSeg*1000.0);
}

static int buscarPrazo(const char *url,char **corpo,int *status,unsigned prazo,size_t max){
  RedePedido q;RedeResposta r;int ok;
  memset(&q,0,sizeof q);memset(&r,0,sizeof r);
  q.url=url;q.seguir=1;q.prazo_ms=prazo;q.max_bytes=max;
  ok=rede_pedir(&q,&r);
  *status=ok?r.status:0;*corpo=NULL;
  if(ok&&r.status>=200&&r.status<=299&&r.corpo){*corpo=r.corpo;r.corpo=NULL;}
  rede_resposta_limpar(&r);
  return *status;
}
static int buscarPadrao(const char *url,char **corpo,int *status){return buscarPrazo(url,corpo,status,12000,0);}
static int (*buscar)(const char *,char **,int *)=buscarPadrao;
void intro_definir_buscador(int (*f)(const char *,char **,int *)){buscar=f?f:buscarPadrao;}
static int buscarInterno(const char *url,char **corpo,int *status){return buscar(url,corpo,status);}

enum{R_OK,R_VAZIO,R_FALHA};
// Episode misses are session-only (credfonte.c), cleared on upgrade/restart.
// Persistent network misses hash the full URL, so /v3 cannot block /v4.
static void chave404(char *k,size_t n,const Pedido *p){
  if(p->tmdb>0)snprintf(k,n,"tmdb:%ld",p->tmdb);else snprintf(k,n,"%s",p->id);
}
static int pedirEp(const Pedido *p,int t,int e,double dur,char **json){
  char url[320],k[32],*j=NULL;int st=0;long agora=(long)time(NULL);
  *json=NULL;chave404(k,sizeof k,p);
  if(t>=0&&e>0&&cred_404_visto(k,t,e,agora))return R_VAZIO;
  intro_montar_url(url,sizeof url,p->id,p->tmdb,t,e,dur);
  buscar(url,&j,&st);
  if(st==204||st==404){free(j);if(t>=0&&e>0)cred_404_marcar(k,t,e,agora);return R_VAZIO;}
  if(st<200||st>299||!j){
    free(j);
    printf("[intro] %s S%02dE%02d: resposta %d, tenta de novo depois\n",k,t,e,st);fflush(stdout);
    return R_FALHA;
  }
  // OUTRO EPISODIO NA RESPOSTA (intro_resposta_confere): descarta como 404.
  if(!intro_resposta_confere(j,t,e)){
    printf("[intro] %s S%02dE%02d: a API devolveu outro episodio, marcador descartado\n",k,t,e);
    fflush(stdout);
    free(j);
    cred_404_marcar(k,t,e,agora);
    return R_VAZIO;
  }
  *json=j;return R_OK;
}

static int geracaoVale(unsigned g){int v;pthread_mutex_lock(&trava);v=(g==geracao);pthread_mutex_unlock(&trava);return v;}

// IntroDB e independente do TheIntroDB: /segments, IMDb e objetos por tipo.
static int imdbValido(const char *id){
  if(!id||strncmp(id,"tt",2)||!id[2]||strlen(id)>=24)return 0;
  for(const char *p=id+2;*p;p++)if(*p<'0'||*p>'9')return 0;
  return 1;
}
int intro_montar_url_backup(char *url,size_t n,const char *id,int t,int e){
  int k;
  if(!url||!n)return 0;
  url[0]=0;
  if(!imdbValido(id)||t<0||e<0||(!e&&t))return 0;
  if(e)k=snprintf(url,n,"https://api.introdb.app/segments?imdb_id=%s&season=%d&episode=%d",id,t,e);
  else k=snprintf(url,n,"https://api.introdb.app/segments?imdb_id=%s&is_movie=true",id);
  if(k<0||(size_t)k>=n){url[0]=0;return 0;}
  return 1;
}
static int trechoBackup(const char *j,const char *chave,int tipo,IntroTrecho *v){
  char obj[2048]="";
  if(!js_bruto(j,NULL,chave,obj,sizeof obj)||obj[0]!='{')return 0;
  double a=js_num(obj,NULL,"start_sec",NAN),b=js_num(obj,NULL,"end_sec",NAN);
  if(!js_tem(obj,NULL,"start_sec")||!isfinite(a))a=js_num(obj,NULL,"start_ms",NAN)/1000.0;
  if(!js_tem(obj,NULL,"end_sec")||!isfinite(b))b=js_num(obj,NULL,"end_ms",NAN)/1000.0;
  if(!isfinite(a)||!isfinite(b)||a<0||b<=a)return 0;
  *v=(IntroTrecho){a,b,tipo,0};return 1;
}
int intro_extrair_backup(const char *j,const char *id,int t,int e,IntroTrecho *out,int max){
  char eco[24]="",tipo[16]="",filme[8]="";int n=0;
  if(!j||!out||max<1||!imdbValido(id)||t<0||e<0||(!e&&t))return -1;
  while(*j&&isspace((unsigned char)*j))j++;
  const char *fim=js_fim(j);
  if(*j!='{'||fim<=j||fim[-1]!='}')return -1;
  while(*fim&&isspace((unsigned char)*fim))fim++;
  if(*fim)return -1;
  js_texto(j,NULL,"imdb_id",eco,sizeof eco);js_texto(j,NULL,"media_type",tipo,sizeof tipo);
  js_bruto(j,NULL,"is_movie",filme,sizeof filme);
  if(strcmp(eco,id))return -1;
  if(e){
    if(strcmp(tipo,"tv")||strcmp(filme,"false")||js_num(j,NULL,"season",-1)!=t||js_num(j,NULL,"episode",-1)!=e)return -1;
    if(n<max&&trechoBackup(j,"intro",INTRO_ABERTURA,&out[n]))n++;
    if(n<max&&trechoBackup(j,"recap",INTRO_RESUMO,&out[n]))n++;
  }else if(strcmp(tipo,"movie")||strcmp(filme,"true"))return -1;
  if(n<max&&trechoBackup(j,"outro",INTRO_CREDITOS,&out[n])){
    IntroTrecho cena;
    // Como no Nuvio TV, nao pular uma cena pos-creditos junto com os creditos.
    if(!e&&trechoBackup(j,"post_credits",INTRO_PREVIA,&cena)&&
       cena.inicio<out[n].fim&&cena.fim>out[n].inicio)out[n].fim=cena.inicio;
    if(out[n].fim>out[n].inicio)n++;
  }
  return n;
}

// Cache pequeno, por identidade original e sem duracao: a resposta nao muda
// quando o player descobre a duracao. Rede/5xx/resposta errada nao viram miss.
static struct{char id[24];int t,e,n;time_t quando;IntroTrecho v[3];}backupCache[32];
static int backupProx;
static int pedirBackup(const Pedido *p,IntroTrecho *v,int *falha){
  char url[256],*j=NULL;int st=0,n=-1;time_t agora=time(NULL);
  *falha=0;
  if(!intro_montar_url_backup(url,sizeof url,p->id,p->imdbT,p->imdbE))return 0;
  pthread_mutex_lock(&trava);
  for(int i=0;i<32;i++)if(!strcmp(backupCache[i].id,p->id)&&backupCache[i].t==p->imdbT&&
      backupCache[i].e==p->imdbE&&agora>=backupCache[i].quando&&agora-backupCache[i].quando<1800){
    n=backupCache[i].n;memcpy(v,backupCache[i].v,(size_t)n*sizeof *v);break;
  }
  pthread_mutex_unlock(&trava);
  if(n>=0)return n;
  if(buscar==buscarPadrao)buscarPrazo(url,&j,&st,6000,32768);else buscar(url,&j,&st);
  if(st==404||st==204)n=0;
  else if(st>=200&&st<300&&j&&strlen(j)<=32768)n=intro_extrair_backup(j,p->id,p->imdbT,p->imdbE,v,3);
  free(j);
  printf("[intro] IntroDB backup %s S%02dE%02d: HTTP %d, %d marcadores%s\n",
         p->id,p->imdbT,p->imdbE,st,n<0?0:n,n<0?" (resposta indisponivel/invalida)":"");fflush(stdout);
  if(n<0){*falha=1;return 0;}
  pthread_mutex_lock(&trava);
  int i=backupProx;backupProx=(backupProx+1)%32;
  snprintf(backupCache[i].id,sizeof backupCache[i].id,"%s",p->id);
  backupCache[i].t=p->imdbT;backupCache[i].e=p->imdbE;backupCache[i].n=n;backupCache[i].quando=agora;
  memcpy(backupCache[i].v,v,(size_t)n*sizeof *v);
  pthread_mutex_unlock(&trava);return n;
}
static int faltaBackup(const IntroTrecho *v,int n,int episodio){
  int tipos=0;
  for(int i=0;i<n;i++)tipos|=1<<v[i].tipo;
  int alvo=1<<INTRO_CREDITOS;
  if(episodio)alvo|=(1<<INTRO_ABERTURA)|(1<<INTRO_RESUMO);
  return (tipos&alvo)!=alvo;
}

// SEM MARCADOR DESTE EPISODIO: olha os vizinhos da mesma temporada (E-1 e
// E+1, no maximo DOIS pedidos, e nenhum se a temporada ja tem um guardado).
// Guarda o inicio dos creditos e a duracao do vizinho, para o player converter
// em "quanto falta para o fim" na duracao real do episodio que esta tocando.
static void tentarVizinhos(const Pedido *p){
  int cand[2],k,nc=0;double durs[2];
  double ini,dv;
  if(p->t<0||p->e<1||cred_viz_ler(p->id,p->t,&ini,&dv))return;
  if(p->e>1){cand[nc]=p->e-1;durs[nc++]=p->durAnt;}
  cand[nc]=p->e+1;durs[nc++]=p->durProx;
  for(k=0;k<nc;k++){
    char*j;IntroTrecho v[8];int n,i;double melhor=0.0;
    if(!geracaoVale(p->g))return;
    pedirEp(p,p->t,cand[k],0.0,&j);
    n=j?intro_extrair(j,v,8):0;free(j);
    for(i=0;i<n;i++)if(v[i].tipo==INTRO_CREDITOS&&v[i].inicio>melhor)melhor=v[i].inicio;
    if(melhor>1.0){
      cred_viz_guardar(p->id,p->t,melhor,durs[k]);
      printf("[intro] no marker for E%d; neighbour E%d has credits at %.0fs\n",p->e,cand[k],melhor);
      fflush(stdout);
      return;
    }
  }
}

// O ULTIMO PEDIDO, para pedir de novo com a duracao e para tentar de novo
// depois de uma falha. Tudo sob `trava`.
static Pedido ultimo;static int temUltimo;
static double enviadoDur;static int refeitos;
static int falhou,tentativas;static long falhouEm;
#define INTRO_REFAZ_DUR_S   30.0   // duracao mudou mais que isto: pede de novo
#define INTRO_REFAZ_MAX     2      // no maximo dois pedidos a mais por duracao
#define INTRO_RETENTA_S     30L    // falha (5xx/rede): nova tentativa depois
#define INTRO_RETENTA_MAX   3

// ANIME PRIMEIRO NO ANISKIP. Sem MAL, sem resposta ou sem trecho: segue para o
// TheIntroDB quando ha imdb/tmdb para perguntar (item kitsu/mal costuma nao ter).
static int pedirAniskip(const Pedido *p,IntroTrecho *v,int max,int *falha){
  long mal;char url[256],*j=NULL;int st=0,n=0,ep=p->e;
  *falha=0;
  if(ep<1)return 0;
  if(!strncmp(p->id,"kitsu:",6)||!strncmp(p->id,"mal:",4)){
    if(p->t!=1)return 0; // id do addon ja e uma temporada MAL; especial exige mapa proprio
    mal=malDe(p->id);
  }else{
    if(!p->anime)return 0;
#ifdef __EMSCRIPTEN__
    // Fribb streams through rede_pedir on this worker. That reaches an
    // EM_ASYNC_JS outside the deliberately narrow ASYNCIFY_ONLY list.
    printf("[intro] Fribb mapping disabled on Tizen WGT: worker fetch is outside ASYNCIFY_ONLY\n");
    fflush(stdout);
    return 0;
#else
    mal=malMapeado(p->id,p->tmdb,p->t,&ep);
#endif
  }
  if(mal<=0)return 0;
  intro_montar_url_aniskip(url,sizeof url,mal,ep,p->dur);
  buscar(url,&j,&st);
  if(st>=200&&st<=299&&j)n=intro_extrair_aniskip(j,p->dur,v,max);
  else if(st!=404&&st!=204)*falha=1;
  free(j);
  return n;
}

// COBERTURA POR TIPO: cada fonte so preenche o tipo que a anterior nao deu.
int intro_tipos_completos(const IntroTrecho *v,int n){
  int t,i,m=0;
  for(i=0;i<n;i++)if(v[i].tipo>=INTRO_ABERTURA&&v[i].tipo<=INTRO_PREVIA)m|=1<<v[i].tipo;
  for(t=INTRO_ABERTURA;t<=INTRO_PREVIA;t++)if(!(m&(1<<t)))return 0;
  return 1;
}
int intro_completar(IntroTrecho *dst,int n,const IntroTrecho *src,int ns,int max){
  int i,k,n0=n;
  for(i=0;i<ns&&n<max;i++){
    for(k=0;k<n0&&dst[k].tipo!=src[i].tipo;k++);
    if(k==n0){dst[n]=src[i];dst[n++].ani=0;}
  }
  return n;
}

static void *baixar(void *u){
  Pedido*p=u;char*j=NULL;IntroTrecho v[8],limitados[8];
  int caps=0,total=0,falhaAni=0,n=pedirAniskip(p,v,8,&falhaAni),r=R_VAZIO,temCred=0,vale,deAni=n>0;
  int nl=0,deBackup=0;
  int temIdOutro=p->tmdb>0||!strncmp(p->id,"tt",2);
  for(int i=0;i<n;i++)if(v[i].tipo==INTRO_CREDITOS)temCred=1;
  if(temIdOutro&&!intro_tipos_completos(v,n)){
    IntroTrecho w[8];int nw;
    int rj=pedirEp(p,p->t,p->e,p->dur,&j);
    nw=j?intro_extrair(j,w,8):0;
    nl=j&&!temCred?creditosjson_extrair(j,limitados,8):0;
    n=intro_completar(v,n<0?0:n,w,nw,8);
    r=rj==R_FALHA?R_FALHA:n>0?R_OK:rj;
  }
  else if(n<=0&&falhaAni)r=R_FALHA;
  else if(n>0)r=R_OK;
  if(n<0)n=0;
  if(deAni)
    for(int i=0;i<n&&nl<8;i++)
      if(v[i].ani&&v[i].tipo==INTRO_CREDITOS&&v[i].fim>v[i].inicio)limitados[nl++]=v[i];
  free(j);
  if(geracaoVale(p->g)&&imdbValido(p->id)&&faltaBackup(v,n,p->imdbE>0)){
    // Publica o que ja chegou antes de esperar ate 6 s pelo complemento.
    pthread_mutex_lock(&trava);
    if(p->g==geracao&&n>0){
      memcpy(trechos,v,(size_t)n*sizeof *v);nTrechos=n;
      memcpy(creditosLimitados,limitados,(size_t)nl*sizeof *limitados);nCreditosLimitados=nl;
      fundirCapitulos();
    }
    pthread_mutex_unlock(&trava);
    IntroTrecho w[3];int falhaBackup=0,n0=n,nw=pedirBackup(p,w,&falhaBackup);
    n=intro_completar(v,n,w,nw,8);deBackup=n-n0;
    for(int i=n0;i<n&&nl<8;i++)if(v[i].tipo==INTRO_CREDITOS)limitados[nl++]=v[i];
    if(falhaBackup)r=R_FALHA;else if(n>0&&r!=R_FALHA)r=R_OK;
    if(deBackup){printf("[intro] IntroDB backup: %d tipos ausentes adicionados\n",deBackup);fflush(stdout);}
  }
  for(int i=0;i<n;i++)if(v[i].tipo==INTRO_CREDITOS)temCred=1;
  pthread_mutex_lock(&trava);
  vale=p->g==geracao;
  if(vale){
    falhou=r==R_FALHA;
    if(falhou)falhouEm=(long)time(NULL);
    if(!falhou||n>0){
      memcpy(trechos,v,(size_t)n*sizeof *v);nTrechos=n;
      memcpy(creditosLimitados,limitados,(size_t)nl*sizeof *limitados);nCreditosLimitados=nl;
    }
    fundirCapitulos();caps=nCapTr;total=nTrechos;
  }
  pthread_mutex_unlock(&trava);
  if(vale&&r!=R_FALHA){
    printf("[intro] %s S%02dE%02d: fonte=%s%s, %d marcadores, dur %.0fs\n",p->id,p->t,p->e,
           caps?"capitulos":deAni?"AniSkip":n>deBackup?"TheIntroDB":deBackup?"IntroDB":"nenhuma",
           deBackup&&n>deBackup?" + IntroDB":caps&&n>0?(deAni?" + AniSkip":" + TheIntroDB"):"",total,p->dur);fflush(stdout);
    if(!temCred&&temIdOutro)tentarVizinhos(p);
  }
  free(p);return NULL;
}

// Dispara o pedido de `base` com a duracao `dur`. Chamar SEM a trava.
static void disparar(const Pedido *base,double dur,int novo){
  Pedido*p;pthread_t fio;
  p=malloc(sizeof*p);if(!p)return;
  *p=*base;p->dur=dur;
  pthread_mutex_lock(&trava);
  if(novo){nTrechos=0;nCreditosLimitados=0;nCapTr=0;refeitos=0;tentativas=0;}
  falhou=0;
  p->anime=animeDe(p->id,p->tmdb);
  p->g=++geracao;ultimo=*p;temUltimo=1;enviadoDur=dur;
  pthread_mutex_unlock(&trava);
  if(pthread_create(&fio,NULL,baixar,p)==0)pthread_detach(fio);else free(p);
}

void intro_pedir_ids_mapeados(const char *imdb,long tmdb,int t,int e,int imdbT,int imdbE,double durAnt,double durProx){
  Pedido b;int nId;
  // FILME PASSA. A guarda antiga exigia temporada e episodio, porque o servico
  // antigo exigia — era ela que deixava todo filme sem marcador.
  int anime=imdb&&(!strncmp(imdb,"kitsu:",6)||!strncmp(imdb,"mal:",4));
  if(tmdb<=0&&!anime&&(!imdb||strncmp(imdb,"tt",2))){intro_desligar();return;}
  memset(&b,0,sizeof b);
  // O id do catalogo pode vir como "tt123:1:2" (serie com episodio embutido);
  // a API quer so a parte do imdb. "kitsu:12:5" fica "kitsu:12".
  if(imdb){const char *c0=anime?strchr(imdb,':')+1:imdb;
    nId=(int)(c0-imdb)+(int)strcspn(c0,":");if(nId>(int)sizeof b.id-1)nId=(int)sizeof b.id-1;
    memcpy(b.id,imdb,(size_t)nId);}
  b.tmdb=tmdb>0?tmdb:0;
  b.t=t>=0&&e>0?t:0;b.e=t>=0&&e>0?e:0;
  b.imdbT=imdbT;b.imdbE=imdbE;
  b.durAnt=durAnt;b.durProx=durProx;
  disparar(&b,0.0,1);
}
void intro_pedir_ids(const char *imdb,long tmdb,int t,int e,double durAnt,double durProx){
  intro_pedir_ids_mapeados(imdb,tmdb,t,e,t,e,durAnt,durProx);
}

void intro_pedir_vizinhos(const char *imdb,int t,int e,double durAnt,double durProx){
  intro_pedir_ids(imdb,0,t,e,durAnt,durProx);
}

void intro_pedir(const char *imdb,int t,int e){intro_pedir_vizinhos(imdb,t,e,0.0,0.0);}

static int tipoDeCap(int t){return t==INTRO_ABERTURA?1:t==INTRO_RESUMO?2:3;}   // creditos e previa andam juntos
static void fundirCapitulos(void){
  int i,k,j=0;
  if(nCapTr<=0)return;
  for(i=0;i<nTrechos;i++){
    int sub=0;
    for(k=0;k<nCapTr;k++)if(tipoDeCap(capTr[k].tipo)==tipoDeCap(trechos[i].tipo))sub=1;
    if(!sub)trechos[j++]=trechos[i];
  }
  nTrechos=j;
  for(k=0;k<nCapTr&&nTrechos<8;k++)trechos[nTrechos++]=capTr[k];
  verGer=geracao+1;   // forca valido() a reavaliar os vereditos por indice
}
void intro_definir_capitulos(const IntroTrecho *v,int n){
  pthread_mutex_lock(&trava);
  nCapTr=0;
  for(int i=0;v&&i<n&&nCapTr<4;i++)capTr[nCapTr++]=v[i];
  fundirCapitulos();
  if(nCapTr>0&&temUltimo){printf("[intro] %s S%02dE%02d: fonte=capitulos, %d marcadores\n",
      ultimo.id,ultimo.t,ultimo.e,nTrechos);fflush(stdout);}
  pthread_mutex_unlock(&trava);
}
void intro_desligar(void){pthread_mutex_lock(&trava);geracao++;nTrechos=nCreditosLimitados=0;nCapTr=0;botaoVis=0;botaoIdx=-1;temUltimo=0;falhou=0;pthread_mutex_unlock(&trava);}

// DURACAO DA MIDIA E O TIPO (filme/serie), para recusar janelas absurdas.
static double durMidia;static int ehFilme;
// E O GATILHO DO PEDIDO DE NOVO (2.0.3, como o plugin oficial): a duracao real
// chegou ou mudou mais que INTRO_REFAZ_DUR_S desde o ultimo pedido -> pede com
// `duration_ms`; uma falha (5xx/rede) tenta de novo a cada INTRO_RETENTA_S.
void intro_definir_duracao(double dur,int filme){
  Pedido b;int refaz=0;double d=0.0;long agora=(long)time(NULL);
  pthread_mutex_lock(&trava);durMidia=dur>1.0?dur:0.0;ehFilme=filme;
  if(temUltimo){
    if(durMidia>0.0&&refeitos<INTRO_REFAZ_MAX&&
       (enviadoDur<=0.0||durMidia-enviadoDur>INTRO_REFAZ_DUR_S||enviadoDur-durMidia>INTRO_REFAZ_DUR_S)){
      refeitos++;refaz=1;d=durMidia;
    }else if(falhou&&tentativas<INTRO_RETENTA_MAX&&agora-falhouEm>=INTRO_RETENTA_S){
      tentativas++;refaz=1;d=enviadoDur;
    }
    b=ultimo;
  }
  pthread_mutex_unlock(&trava);
  if(refaz)disparar(&b,d,0);
}

// JANELA ACEITA? Um marcador de creditos com inicio errado (ou `end_ms` nulo =
// "ate o fim") fazia o botao "Pular creditos" ficar de pe por 25-30 min num
// filme. Limites: creditos <= 15 min, abertura/resumo <= 3 min, e creditos de
// FILME nao comecam antes de 50% da duracao. Sem duracao conhecida (dur<=0) so
// vale o que da para medir (fim explicito); o auto-hide do botao cobre o resto.
static int fimDeObra(int tipo){return tipo==INTRO_CREDITOS||tipo==INTRO_PREVIA;}

int intro_janela_ok(int tipo,double ini,double fim,double dur,int filme,const char **motivo){
  double fimEf=fim>0.0?fim:dur,jan=fimEf>0.0?fimEf-ini:0.0;
  double max=tipo==INTRO_CREDITOS?900.0:180.0;
  if(motivo)*motivo="ok";
  if(jan>max){if(motivo)*motivo="janela longa";return 0;}
  // CREDITOS/PREVIA SEM A DURACAO DO VIDEO NAO SE ACEITAM (Silo T2E6, TCL
  // 08/10: "janela aceita ... dur=0s" antes de o pipeline dizer 2582 s). Sem
  // duracao so o proprio marcador fala, e e justamente o que a janela existe
  // para desmentir (outro corte). valido() reavalia a cada chamada, entao o
  // veredito sai assim que a duracao real chegar.
  if(fimDeObra(tipo)&&!(dur>0.0)){if(motivo)*motivo="duracao desconhecida";return 0;}
  if(dur>0.0&&ini>=dur){if(motivo)*motivo="inicio alem da duracao";return 0;}
  // FIM EXPLICITO DEPOIS DO FIM DA MIDIA: o marcador foi feito sobre um corte
  // mais longo (outro lancamento). 10 s de folga para arredondamento.
  if(dur>0.0&&fim>dur+10.0){if(motivo)*motivo="fim alem da duracao (outro corte)";return 0;}
  if(fimDeObra(tipo)&&filme&&dur>0.0&&ini<dur*0.5){if(motivo)*motivo="inicio antes de 50% do filme";return 0;}
  // 2.0.3: creditos (e previa) no meio da obra nao sao creditos. Era isso que
  // punha o botao "Pular creditos" (e o cartao) no meio do episodio. Filme tem
  // janela propria, mais larga (creditos de filme passam de 10 min).
  if(fimDeObra(tipo)&&dur>0.0&&
     dur-ini>(filme?intro_creditos_janela_filme(dur):intro_creditos_janela(dur))){
    if(motivo)*motivo=filme?"creditos fora da parte final do filme":"creditos fora da parte final do episodio";return 0;}
  if(!fimDeObra(tipo)&&dur>0.0&&ini>dur*(filme?INTRO_ABERTURA_FILME_FRAC:0.5)){
    if(motivo)*motivo="abertura/resumo fora da parte inicial";return 0;}
  return 1;
}

double intro_creditos_janela_filme(double dur){
  double j=dur*INTRO_CRED_FILME_FRAC;
  if(dur<=0.0)return 0.0;
  if(j<INTRO_CRED_FILME_MIN_S)j=INTRO_CRED_FILME_MIN_S;
  if(j>INTRO_CRED_FILME_MAX_S)j=INTRO_CRED_FILME_MAX_S;
  if(j>dur*0.5)j=dur*0.5;
  return j;
}

double intro_fim_estimado_filme(double dur){
  if(dur<INTRO_FILME_MIN_S)return 0.0;
  return dur<INTRO_FILME_CURTO_ATE_S?INTRO_FIM_FILME_CURTO_S:INTRO_FIM_FILME_S;
}

double intro_creditos_janela(double dur){
  double j=dur*INTRO_CRED_FRAC;
  if(dur<=0.0)return 0.0;
  if(j<INTRO_CRED_MIN_S)j=INTRO_CRED_MIN_S;
  if(j>INTRO_CRED_MAX_S)j=INTRO_CRED_MAX_S;
  if(j>dur*0.5)j=dur*0.5;
  return j;
}

double intro_fim_estimado(double dur){
  if(dur<INTRO_DUR_MIN_S)return 0.0;
  return dur<INTRO_CURTO_ATE_S?INTRO_FIM_CURTO_S:INTRO_FIM_SERIE_S;
}

int intro_resposta_confere(const char *j,int t,int e){
  const char *f;double rt,re;
  if(!j)return 0;
  if(t<0||e<1)return 1;                       // filme: sem par a conferir
  f=js_fim(j);
  rt=js_num(j,f,"season",-1);re=js_num(j,f,"episode",-1);
  // Sem eco (formato antigo): nao ha como provar o contrario, aceita.
  if(rt<0&&re<0)return 1;
  return (int)rt==t&&(int)re==e;
}

static signed char veredito[8];   // 0 nao avaliado, 1 aceito, -1 recusado (por geracao)
static char mostrado[8];          // o botao deste trecho ja apareceu sozinho

static int valido(int i){
  const char *m;int ok;
  if(verGer!=geracao){memset(veredito,0,sizeof veredito);memset(mostrado,0,sizeof mostrado);verGer=geracao;botaoIdx=-1;botaoVis=0;}
  ok=intro_janela_ok(trechos[i].tipo,trechos[i].inicio,trechos[i].fim,durMidia,ehFilme,&m);
  // Loga uma vez por veredito (muda se a duracao real chegar depois).
  if(veredito[i]!=(ok?1:-1)){
    veredito[i]=(signed char)(ok?1:-1);
    printf("[marcador] janela %s tipo=%d ini=%.0fs fim=%.0fs dur=%.0fs (%s)\n",ok?"aceita":"recusada",
           trechos[i].tipo,trechos[i].inicio,trechos[i].fim,durMidia,m);
    fflush(stdout);
  }
  return ok;
}

// O FIM DO PULO: creditos emendados numa previa (inicio da previa ate 2 s do
// fim dos creditos, o caso de anime: One Piece T1E1 creditos 1389-1459, previa
// 1459-1500) pulam as duas. Chamar com a trava.
static double fimEmendado(int i){
  double f=trechos[i].fim;
  if(trechos[i].tipo!=INTRO_CREDITOS||f<=0.0)return f;
  for(int k=0;k<nTrechos;k++)
    if(trechos[k].tipo==INTRO_PREVIA&&trechos[k].inicio>=f-2.0&&trechos[k].inicio<=f+2.0&&valido(k))
      return trechos[k].fim;            // 0 = ate o fim
  return f;
}

// Chamar com a trava: manual e automatico compartilham a escolha/janela.
static int ativoInterno(double pos,double*fim,int*tipo,int automatico){
  int ok=0;
  for(int i=0;i<nTrechos;i++){
    // fim ZERO = ate o fim da midia: basta ter passado do inicio.
    int dentro=trechos[i].fim>0.0
               ? (pos>=trechos[i].inicio&&pos<trechos[i].fim)
               : (pos>=trechos[i].inicio);
    if(trechos[i].tipo==INTRO_PREVIA)continue;   // sem botao proprio (CHAVES)
    if(dentro&&valido(i)){
      double f=fimEmendado(i);
      if(automatico&&trechos[i].tipo==INTRO_CREDITOS){
        int explicito=0;
        autoSincronizar();
        if(autoRecuou&&autoDe>=trechos[i].inicio&&autoAte<trechos[i].fim)continue;
        // Capitulos do proprio arquivo continuam tendo prioridade (#329).
        // Rede: inicio e fim numericos deste titulo, nunca vizinho/estimativa.
        for(int k=0;k<nCapTr;k++)
          if(capTr[k].tipo==INTRO_CREDITOS&&capTr[k].inicio==trechos[i].inicio&&
             capTr[k].fim==trechos[i].fim&&capTr[k].fim>capTr[k].inicio)explicito=1;
        for(int k=0;k<nCreditosLimitados;k++)
          if(creditosLimitados[k].inicio==trechos[i].inicio&&
             creditosLimitados[k].fim==trechos[i].fim)explicito=1;
        if(!explicito)continue;
        f=trechos[i].fim; // nao pula a previa/cena depois dos creditos
      }
      if(fim)*fim=f;if(tipo)*tipo=trechos[i].tipo;ok=1;break;
    }
  }
  return ok;
}
int intro_ativo(double pos,double*fim,int*tipo){
  int ok;pthread_mutex_lock(&trava);ok=ativoInterno(pos,fim,tipo,0);pthread_mutex_unlock(&trava);return ok;
}
int intro_auto_ativo(double pos,double*fim,int*tipo){
  int ok;pthread_mutex_lock(&trava);ok=ativoInterno(pos,fim,tipo,1);pthread_mutex_unlock(&trava);return ok;
}

// O BOTAO DE PULAR, com tempo: aparece sozinho UMA vez por trecho, some em
// 10 s se nao estiver focado, e so volta enquanto os controles (osd) estao de
// pe dentro da janela. Chamado a cada quadro; `agora` em segundos monotonicos.
int intro_botao(double pos,double agora,int osd,int focado,double*fim,int*tipo){
  int i,idx=-1,vis=0;
  pthread_mutex_lock(&trava);
  for(i=0;i<nTrechos;i++){
    int dentro=trechos[i].fim>0.0?(pos>=trechos[i].inicio&&pos<trechos[i].fim):(pos>=trechos[i].inicio);
    if(trechos[i].tipo==INTRO_PREVIA)continue;
    if(dentro&&valido(i)){idx=i;break;}
  }
  if(idx<0){botaoIdx=-1;botaoVis=0;pthread_mutex_unlock(&trava);return 0;}
  if(idx!=botaoIdx){botaoIdx=idx;botaoDesde=agora;}
  if(!mostrado[idx]){mostrado[idx]=1;botaoDesde=agora;}
  if(focado)botaoDesde=agora;                 // nao some debaixo do foco
  vis=(agora-botaoDesde<INTRO_BOTAO_SEG)||osd;
  botaoVis=vis;botaoFim=fimEmendado(idx);botaoTipo=trechos[idx].tipo;
  if(vis){if(fim)*fim=botaoFim;if(tipo)*tipo=botaoTipo;}
  pthread_mutex_unlock(&trava);return vis;
}

// O que o ultimo intro_botao decidiu: as teclas pulam so o que esta na tela.
int intro_botao_visivel(double*fim,int*tipo){
  int v;pthread_mutex_lock(&trava);v=botaoVis;
  if(v){if(fim)*fim=botaoFim;if(tipo)*tipo=botaoTipo;}
  pthread_mutex_unlock(&trava);return v;
}

// Dismiss this appearance; opening the controls can reveal it again.
void intro_botao_esconder(double agora){
  pthread_mutex_lock(&trava);
  botaoVis=0;
  botaoDesde=agora-INTRO_BOTAO_SEG-1.0;
  pthread_mutex_unlock(&trava);
}

// O trecho de creditos que comeca POR ULTIMO, e nao o primeiro da lista
// (#115): a API devolve uma lista por tipo, e um filme com creditos de abertura
// e finais marcados punha o painel de relacionados no comeco.
//
// 2.0.3: so os trechos que a guarda de janela ACEITA (valido). Um marcador de
// creditos recusado para o botao (meio do episodio, outro corte) tambem nao
// pode abrir o cartao do proximo episodio.
// 1 quando o trecho de creditos atual veio do AniSkip, 0 quando do TheIntroDB.
int intro_creditos_aniskip(void){
  int r=0;pthread_mutex_lock(&trava);
  for(int i=0;i<nTrechos;i++)if(trechos[i].tipo==INTRO_CREDITOS&&trechos[i].ani)r=1;
  pthread_mutex_unlock(&trava);return r;
}
double intro_creditos_seg(void){
  double s=0.0;pthread_mutex_lock(&trava);
  for(int i=0;i<nTrechos;i++)
    if(trechos[i].tipo==INTRO_CREDITOS&&trechos[i].inicio>s&&valido(i))s=trechos[i].inicio;
  // SEM CREDITOS MARCADOS, a PREVIA marca o fim do episodio (AoT T1E1: so
  // "preview" aos 1432 s). E o inicio dela que vale para o cartao.
  if(s<=0.0)
    for(int i=0;i<nTrechos;i++)
      if(trechos[i].tipo==INTRO_PREVIA&&(s<=0.0||trechos[i].inicio<s)&&valido(i))s=trechos[i].inicio;
  pthread_mutex_unlock(&trava);return s;
}

// Os trechos conhecidos, para a barra do player marcar onde comecam e acabam
// (os cortes discretos do mockup do Glass UI). Copia sob a trava.
int intro_trechos(IntroTrecho *saida,int max){
  int n;pthread_mutex_lock(&trava);
  n=nTrechos<max?nTrechos:max;
  if(n>0)memcpy(saida,trechos,(size_t)n*sizeof *saida);
  pthread_mutex_unlock(&trava);return n;
}
// So limites numericos explicitos da resposta DESTE titulo. Marcadores abertos,
// vizinhos e estimativas continuam disponiveis apenas pelo caminho manual.
int intro_creditos_limitados(IntroTrecho *saida,int max){
  int n;if(!saida||max<1)return 0;pthread_mutex_lock(&trava);
  n=0;
  for(int i=0;i<nCreditosLimitados&&n<max;i++){
    IntroTrecho v=creditosLimitados[i];
    if(intro_janela_ok(v.tipo,v.inicio,v.fim,durMidia,ehFilme,NULL))saida[n++]=v;
  }
  pthread_mutex_unlock(&trava);return n;
}
#ifdef NV_SHOT_HOOKS
void intro_shot_definir(const IntroTrecho *v,int n){
  pthread_mutex_lock(&trava);geracao++;
  nCreditosLimitados=0;
  nTrechos=n<8?n:8;if(nTrechos>0)memcpy(trechos,v,(size_t)nTrechos*sizeof *v);
  pthread_mutex_unlock(&trava);
}
#endif

void intro_registrar_tmdb(const char *id,long tmdb,const char *json){
  char lang[8]="";int animacao=0,jp=0,i,refaz=0;Pedido b;
  const char *p;
  if(!id||!json)return;
  js_texto_raiz(json,"original_language",lang,sizeof lang);jp=!strcmp(lang,"ja");
  for(p=js_array(json,NULL,"genres");p;p=js_prox(js_fim(p)))
    if(js_num(p,js_fim(p),"id",0)==16)animacao=1;
  char paises[256]="";
  js_bruto(json,NULL,"origin_country",paises,sizeof paises);
  if(strstr(paises,"\"JP\""))jp=1;
  for(p=js_array(json,NULL,"production_countries");p;p=js_prox(js_fim(p))){
    char pais[8]="";js_texto(p,js_fim(p),"iso_3166_1",pais,sizeof pais);
    if(!strcmp(pais,"JP"))jp=1;
  }
  pthread_mutex_lock(&trava);
  for(i=0;i<16;i++)if(!strcmp(animeCache[i].id,id))break;
  if(i==16){i=animeProx;animeProx=(animeProx+1)%16;}
  snprintf(animeCache[i].id,sizeof animeCache[i].id,"%s",id);
  animeCache[i].tmdb=tmdb;animeCache[i].anime=animacao&&jp;
  // A ficha pode chegar depois de o player abrir: refaz UMA vez ao saber anime.
  if(temUltimo&&!ultimo.anime&&animacao&&jp&&
      (!strcmp(ultimo.id,id)||(tmdb>0&&ultimo.tmdb==tmdb))){b=ultimo;refaz=1;}
  pthread_mutex_unlock(&trava);
  if(refaz)disparar(&b,b.dur,0);
}

// #329/#331: quais trechos ja foram pulados sozinhos NESTA reproducao (a geracao muda a cada
// pedido de marcadores, ou seja a cada abertura). Voltar de proposito para dentro do trecho,
// ou para antes dele, nao pula de novo — vale para os tres tipos, e um nao apaga o outro.
void intro_auto_recuar(double de,double para){
  if(!isfinite(de)||!isfinite(para)||para>=de)return;
  pthread_mutex_lock(&trava);autoSincronizar();
  if(!autoRecuou||de>autoDe)autoDe=de;
  if(!autoRecuou||para<autoAte)autoAte=para;
  autoRecuou=1;pthread_mutex_unlock(&trava);
}
int intro_auto_ja(int tipo,double fim){
  int r;if(tipo<1||tipo>3)return 1;
  pthread_mutex_lock(&trava);autoSincronizar();r=autoFim[tipo]==fim;pthread_mutex_unlock(&trava);return r;
}
void intro_auto_marcar(int tipo,double fim){
  if(tipo<1||tipo>3)return;
  pthread_mutex_lock(&trava);autoSincronizar();autoFim[tipo]=fim;pthread_mutex_unlock(&trava);
}

// Abertura/resumo: com o ajuste ligado e fim conhecido. Creditos: so com o ajuste proprio E
// com INICIO e FIM explicitos do marcador (TheIntroDB; fim 0 = "ate o fim da midia" nunca
// vale). Estimativa, os 50 s finais, o aprendido e o episodio vizinho nem passam por aqui: so
// trecho de intro_ativo chega. Creditos que vao ate o fim da midia (fim a menos de 5 s da
// duracao, ou duracao desconhecida) tambem nao: quem os trata e o proximo episodio.
int intro_auto_pular(int tipo,double fim,double dur,int pularAbertura,int pularResumo,int pularCreditos){
  if(!(fim>0.0))return 0;
  if(tipo==INTRO_CREDITOS)return pularCreditos&&dur>0.0&&fim<=dur-5.0;
  return (tipo==INTRO_ABERTURA&&pularAbertura)||(tipo==INTRO_RESUMO&&pularResumo);
}
