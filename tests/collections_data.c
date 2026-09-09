// Exercises the real paged reader with a fake network, including stale responses.
#include <assert.h>
#include <unistd.h>
#include "../src/descoberta.c"
// descoberta.c passou a traduzir os rotulos que monta ("Filme", "Serie", a
// data por extenso) e este teste nao linka idioma.c: linkar puxaria
// ajustes_idioma_ingles e, atras dele, ajustes.c e o resto do app — o oposto
// do que um teste do leitor paginado deve carregar. Devolver a entrada e o que
// i18n faz com o idioma em portugues, que e o padrao, entao os rotulos que as
// asserçoes comparam sao exatamente os de producao. Mesmo stub de
// tests/colfileiras.c.
// 0 = portugues, o padrao — e o idioma em que as asserçoes deste arquivo
// escreveram os rotulos esperados.
int ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s) { return s; }
static int calls;
static pthread_mutex_t fakeLock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fakeCond=PTHREAD_COND_INITIALIZER;
static int slowStarted,releaseSlow;
char *rede_baixar(const char *url,int timeout) {
  (void)timeout;calls++;
  if(strstr(url,"/slow/")) {
    pthread_mutex_lock(&fakeLock);slowStarted=1;pthread_cond_broadcast(&fakeCond);
    while(!releaseSlow)pthread_cond_wait(&fakeCond,&fakeLock);
    pthread_mutex_unlock(&fakeLock);
    return strdup("{\"metas\":[{\"id\":\"ttold\",\"name\":\"OLD\",\"poster\":\"old.jpg\"}]}");
  }
  if(strstr(url,"/error/"))return NULL;
  if(strstr(url,"/empty/"))return strdup("{\"metas\":[]}");
  if(strstr(url,"skip=2"))return strdup("{\"metas\":[{\"id\":\"tt3\",\"name\":\"Third\",\"poster\":\"3.jpg\"}]}");
  if(strstr(url,"skip=3"))return strdup("{\"metas\":[]}");
  return strdup("{\"metas\":[{\"id\":\"tt1\",\"name\":\"First\",\"poster\":\"1.jpg\"},{\"id\":\"tt2\",\"name\":\"Second\",\"poster\":\"2.jpg\"}]}");
}
static void waitDone(void) {for(int i=0;i<2000&&desc_vertudo_carregando();i++)usleep(1000);assert(!desc_vertudo_carregando());}
int main(void) {
  desc_vertudo_abrir("https://example.invalid","movie","rank");waitDone();
  assert(desc_vertudo_n()==2&&!desc_vertudo_fim());
  desc_vertudo_mais();waitDone();assert(desc_vertudo_n()==3);
  CatItem i;assert(desc_vertudo_item(2,&i)&&!strcmp(i.imdb,"tt3"));
  desc_vertudo_mais();waitDone();assert(desc_vertudo_fim());
  desc_vertudo_abrir("https://example.invalid/slow","movie","slow");
  pthread_mutex_lock(&fakeLock);while(!slowStarted)pthread_cond_wait(&fakeCond,&fakeLock);pthread_mutex_unlock(&fakeLock);
  desc_vertudo_abrir("https://example.invalid/new","series","new");
  pthread_mutex_lock(&fakeLock);releaseSlow=1;pthread_cond_broadcast(&fakeCond);pthread_mutex_unlock(&fakeLock);
  waitDone();assert(desc_vertudo_n()==2);assert(desc_vertudo_item(0,&i)&&!strcmp(i.tipo,"series")&&!strcmp(i.imdb,"tt1"));
  desc_vertudo_abrir("https://example.invalid/error","movie","error");waitDone();assert(desc_vertudo_erro()&&!desc_vertudo_fim());
  int before=calls;desc_vertudo_mais();waitDone();assert(calls>before);
  desc_vertudo_abrir("https://example.invalid/empty","movie","empty");waitDone();assert(!desc_vertudo_erro()&&desc_vertudo_fim()&&desc_vertudo_n()==0);
  puts("collections data: PASS (short pages, rank order, stale tab discarded, retry, empty)");
}
