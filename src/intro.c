#include "intro.h"
#include "rede.h"
#include "js.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t trava=PTHREAD_MUTEX_INITIALIZER;
static IntroTrecho trechos[8];static int nTrechos;static unsigned geracao;

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

typedef struct{char url[256];unsigned g;}Pedido;

static void *baixar(void *u){
  Pedido*p=u;char*j=rede_baixar(p->url,12);IntroTrecho v[8];
  int n=j?intro_extrair(j,v,8):0;
  free(j);
  pthread_mutex_lock(&trava);
  if(p->g==geracao){memcpy(trechos,v,(size_t)n*sizeof *v);nTrechos=n;}
  pthread_mutex_unlock(&trava);
  printf("[intro] %d marcadores\n",n);fflush(stdout);
  free(p);return NULL;
}

void intro_pedir(const char *imdb,int t,int e){
  Pedido*p;pthread_t fio;int nId;
  // FILME PASSA. A guarda antiga exigia temporada e episodio, porque o servico
  // antigo exigia — era ela que deixava todo filme sem marcador.
  if(!imdb||strncmp(imdb,"tt",2)){intro_desligar();return;}
  p=calloc(1,sizeof*p);if(!p)return;
  pthread_mutex_lock(&trava);nTrechos=0;p->g=++geracao;pthread_mutex_unlock(&trava);
  // O id do catalogo pode vir como "tt123:1:2" (serie com episodio embutido);
  // a API quer so a parte do imdb.
  nId=(int)strcspn(imdb,":");
  if(t>0&&e>0)
    snprintf(p->url,sizeof p->url,
             "https://api.theintrodb.org/v3/media?imdb_id=%.*s&season=%d&episode=%d",
             nId,imdb,t,e);
  else
    snprintf(p->url,sizeof p->url,
             "https://api.theintrodb.org/v3/media?imdb_id=%.*s",nId,imdb);
  if(pthread_create(&fio,NULL,baixar,p)==0)pthread_detach(fio);else free(p);
}

void intro_desligar(void){pthread_mutex_lock(&trava);geracao++;nTrechos=0;pthread_mutex_unlock(&trava);}

int intro_ativo(double pos,double*fim,int*tipo){
  int ok=0;pthread_mutex_lock(&trava);
  for(int i=0;i<nTrechos;i++){
    // fim ZERO = ate o fim da midia: basta ter passado do inicio.
    int dentro=trechos[i].fim>0.0
               ? (pos>=trechos[i].inicio&&pos<trechos[i].fim)
               : (pos>=trechos[i].inicio);
    if(dentro){if(fim)*fim=trechos[i].fim;if(tipo)*tipo=trechos[i].tipo;ok=1;break;}
  }
  pthread_mutex_unlock(&trava);return ok;
}

double intro_creditos_seg(void){
  double s=0.0;pthread_mutex_lock(&trava);
  for(int i=0;i<nTrechos;i++)
    if(trechos[i].tipo==INTRO_CREDITOS){s=trechos[i].inicio;break;}
  pthread_mutex_unlock(&trava);return s;
}
