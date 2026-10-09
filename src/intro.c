#include "intro.h"
#include "creditosjson.h"
#include "rede.h"
#include "js.h"
#include "credfonte.h"
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


// AS TRES CHAVES QUE A API DEVOLVE, e o tipo de cada uma. "preview" existe no
// servico e fica de fora de proposito: e o trecho do PROXIMO episodio, que este
// app nao pula nem anuncia.
static const struct { const char *chave; int tipo; } CHAVES[] = {
  { "intro",   INTRO_ABERTURA },
  { "recap",   INTRO_RESUMO   },
  { "credits", INTRO_CREDITOS },
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
      if(a<0)a=0;
      if(b<0)b=-1000.0;             // marcador de "sem fim", tratado abaixo
      if(b>=0&&b<=a)continue;       // trecho invertido ou vazio: descarta
      out[n].inicio=a/1000.0;
      out[n].fim=(b<0)?0.0:b/1000.0;
      out[n].tipo=CHAVES[k].tipo;
      n++;
    }
  }
  return n;
}

typedef struct{char id[24];int t,e;double durAnt,durProx;unsigned g;}Pedido;

static void montarUrl(char *url,size_t n,const char *id,int t,int e){
  if(t>0&&e>0)
    snprintf(url,n,"https://api.theintrodb.org/v3/media?imdb_id=%s&season=%d&episode=%d",id,t,e);
  else
    snprintf(url,n,"https://api.theintrodb.org/v3/media?imdb_id=%s",id);
}

// UM EPISODIO NO SERVIDOR, com o cache de "nao conheco": 404 e dado que falta
// (medido: a API esta no ar, so nao tem todo episodio), e repetir o pedido a
// cada abertura so gasta rede. Devolve o JSON (free) ou NULL.
static char *pedirEp(const char *id,int t,int e){
  char url[256],*j;long agora=(long)time(NULL);
  if(t>0&&e>0&&cred_404_visto(id,t,e,agora))return NULL;
  montarUrl(url,sizeof url,id,t,e);
  j=rede_baixar(url,12);
  if(!j&&t>0&&e>0)cred_404_marcar(id,t,e,agora);
  return j;
}

static int geracaoVale(unsigned g){int v;pthread_mutex_lock(&trava);v=(g==geracao);pthread_mutex_unlock(&trava);return v;}

// SEM MARCADOR DESTE EPISODIO: olha os vizinhos da mesma temporada (E-1 e
// E+1, no maximo DOIS pedidos, e nenhum se a temporada ja tem um guardado).
// Guarda o inicio dos creditos e a duracao do vizinho, para o player converter
// em "quanto falta para o fim" na duracao real do episodio que esta tocando.
static void tentarVizinhos(const Pedido *p){
  int cand[2],k,nc=0;double durs[2];
  double ini,dv;
  if(p->t<1||p->e<1||cred_viz_ler(p->id,p->t,&ini,&dv))return;
  if(p->e>1){cand[nc]=p->e-1;durs[nc++]=p->durAnt;}
  cand[nc]=p->e+1;durs[nc++]=p->durProx;
  for(k=0;k<nc;k++){
    char*j;IntroTrecho v[8];int n,i;double melhor=0.0;
    if(!geracaoVale(p->g))return;
    j=pedirEp(p->id,p->t,cand[k]);
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

static void *baixar(void *u){
  Pedido*p=u;char*j=pedirEp(p->id,p->t,p->e);IntroTrecho v[8],limitados[8];
  int n=j?intro_extrair(j,v,8):0,temCred=0;
  int nl=j?creditosjson_extrair(j,limitados,8):0;
  free(j);
  for(int i=0;i<n;i++)if(v[i].tipo==INTRO_CREDITOS)temCred=1;
  pthread_mutex_lock(&trava);
  if(p->g==geracao){
    memcpy(trechos,v,(size_t)n*sizeof *v);nTrechos=n;
    memcpy(creditosLimitados,limitados,(size_t)nl*sizeof *limitados);nCreditosLimitados=nl;
  }
  pthread_mutex_unlock(&trava);
  printf("[intro] %d marcadores\n",n);fflush(stdout);
  if(!temCred)tentarVizinhos(p);
  free(p);return NULL;
}

void intro_pedir_vizinhos(const char *imdb,int t,int e,double durAnt,double durProx){
  Pedido*p;pthread_t fio;int nId;unsigned g;
  // FILME PASSA. A guarda antiga exigia temporada e episodio, porque o servico
  // antigo exigia — era ela que deixava todo filme sem marcador.
  if(!imdb||strncmp(imdb,"tt",2)){intro_desligar();return;}
  pthread_mutex_lock(&trava);nTrechos=nCreditosLimitados=0;g=++geracao;pthread_mutex_unlock(&trava);
  p=calloc(1,sizeof*p);if(!p)return;p->g=g;
  // O id do catalogo pode vir como "tt123:1:2" (serie com episodio embutido);
  // a API quer so a parte do imdb.
  nId=(int)strcspn(imdb,":");if(nId>(int)sizeof p->id-1)nId=(int)sizeof p->id-1;
  memcpy(p->id,imdb,(size_t)nId);
  p->t=t>0&&e>0?t:0;p->e=t>0&&e>0?e:0;
  p->durAnt=durAnt;p->durProx=durProx;
  if(pthread_create(&fio,NULL,baixar,p)==0)pthread_detach(fio);else free(p);
}

void intro_pedir(const char *imdb,int t,int e){intro_pedir_vizinhos(imdb,t,e,0.0,0.0);}

void intro_desligar(void){pthread_mutex_lock(&trava);geracao++;nTrechos=nCreditosLimitados=0;botaoVis=0;botaoIdx=-1;pthread_mutex_unlock(&trava);}

// DURACAO DA MIDIA E O TIPO (filme/serie), para recusar janelas absurdas.
static double durMidia;static int ehFilme;
void intro_definir_duracao(double dur,int filme){
  pthread_mutex_lock(&trava);durMidia=dur>1.0?dur:0.0;ehFilme=filme;pthread_mutex_unlock(&trava);
}

// JANELA ACEITA? Um marcador de creditos com inicio errado (ou `end_ms` nulo =
// "ate o fim") fazia o botao "Pular creditos" ficar de pe por 25-30 min num
// filme. Limites: creditos <= 15 min, abertura/resumo <= 3 min, e creditos de
// FILME nao comecam antes de 50% da duracao. Sem duracao conhecida (dur<=0) so
// vale o que da para medir (fim explicito); o auto-hide do botao cobre o resto.
int intro_janela_ok(int tipo,double ini,double fim,double dur,int filme,const char **motivo){
  double fimEf=fim>0.0?fim:dur,jan=fimEf>0.0?fimEf-ini:0.0;
  double max=tipo==INTRO_CREDITOS?900.0:180.0;
  if(motivo)*motivo="ok";
  if(jan>max){if(motivo)*motivo="janela longa";return 0;}
  if(dur>0.0&&ini>=dur){if(motivo)*motivo="inicio alem da duracao";return 0;}
  if(tipo==INTRO_CREDITOS&&filme&&dur>0.0&&ini<dur*0.5){if(motivo)*motivo="inicio antes de 50% do filme";return 0;}
  return 1;
}

static signed char veredito[8];   // 0 nao avaliado, 1 aceito, -1 recusado (por geracao)
static unsigned verGer;
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

int intro_ativo(double pos,double*fim,int*tipo){
  int ok=0;pthread_mutex_lock(&trava);
  for(int i=0;i<nTrechos;i++){
    // fim ZERO = ate o fim da midia: basta ter passado do inicio.
    int dentro=trechos[i].fim>0.0
               ? (pos>=trechos[i].inicio&&pos<trechos[i].fim)
               : (pos>=trechos[i].inicio);
    if(dentro&&valido(i)){if(fim)*fim=trechos[i].fim;if(tipo)*tipo=trechos[i].tipo;ok=1;break;}
  }
  pthread_mutex_unlock(&trava);return ok;
}

// O BOTAO DE PULAR, com tempo: aparece sozinho UMA vez por trecho, some em
// 10 s se nao estiver focado, e so volta enquanto os controles (osd) estao de
// pe dentro da janela. Chamado a cada quadro; `agora` em segundos monotonicos.
int intro_botao(double pos,double agora,int osd,int focado,double*fim,int*tipo){
  int i,idx=-1,vis=0;
  pthread_mutex_lock(&trava);
  for(i=0;i<nTrechos;i++){
    int dentro=trechos[i].fim>0.0?(pos>=trechos[i].inicio&&pos<trechos[i].fim):(pos>=trechos[i].inicio);
    if(dentro&&valido(i)){idx=i;break;}
  }
  if(idx<0){botaoIdx=-1;botaoVis=0;pthread_mutex_unlock(&trava);return 0;}
  if(idx!=botaoIdx){botaoIdx=idx;botaoDesde=agora;}
  if(!mostrado[idx]){mostrado[idx]=1;botaoDesde=agora;}
  if(focado)botaoDesde=agora;                 // nao some debaixo do foco
  vis=(agora-botaoDesde<INTRO_BOTAO_SEG)||osd;
  botaoVis=vis;botaoFim=trechos[idx].fim;botaoTipo=trechos[idx].tipo;
  if(vis){if(fim)*fim=botaoFim;if(tipo)*tipo=botaoTipo;}
  pthread_mutex_unlock(&trava);return vis;
}

// O que o ultimo intro_botao decidiu: as teclas pulam so o que esta na tela.
int intro_botao_visivel(double*fim,int*tipo){
  int v;pthread_mutex_lock(&trava);v=botaoVis;
  if(v){if(fim)*fim=botaoFim;if(tipo)*tipo=botaoTipo;}
  pthread_mutex_unlock(&trava);return v;
}

// O trecho de creditos que comeca POR ULTIMO, e nao o primeiro da lista
// (#115): a API devolve uma lista por tipo, e um filme com creditos de abertura
// e finais marcados punha o painel de relacionados no comeco.
double intro_creditos_seg(void){
  double s=0.0;pthread_mutex_lock(&trava);
  for(int i=0;i<nTrechos;i++)
    if(trechos[i].tipo==INTRO_CREDITOS&&trechos[i].inicio>s)s=trechos[i].inicio;
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
  n=nCreditosLimitados<max?nCreditosLimitados:max;
  if(n>0)memcpy(saida,creditosLimitados,(size_t)n*sizeof *saida);
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
