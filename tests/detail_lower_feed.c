// Exercise whole-title pagination through the existing review worker, offline.
#define rede_pedir feedRequest
#define trakt_cabecalhos_publicos feedHeaders
#include "../src/extras.c"
#include <assert.h>
#include <SDL2/SDL.h>
static int requests, movies, ratings;
int feedHeaders(const char **cab,char *key,size_t size) {
  (void)key; (void)size; cab[0]="trakt-api-version: 2"; cab[1]=NULL; return 1;
}
int feedRequest(const RedePedido *request,RedeResposta *response) {
  memset(response,0,sizeof *response);
  response->status=200; response->erro=REDE_OK;
  if (strstr(request->url,"/ratings")) ratings++;
  assert(!strstr(request->url,"/seasons/"));
  assert(strstr(request->url,movies ? "/movies/tt9000100/comments/likes?page=" : "/shows/tt9000100/comments/likes?page="));
  requests++;
  int second=strstr(request->url,"page=2")!=NULL;
  char body[4096]="[";
  for (int i=0;i<(second ? 1 : 8);i++) {
    char row[320];
    snprintf(row,sizeof row,"%s{\"comment\":\"Complete review %d\",\"user\":{\"username\":\"Reader %d\"},\"created_at\":\"2026-10-01\",\"likes\":2}",i ? "," : "",second ? 8 : i,second ? 8 : i);
    strcat(body,row);
  }
  strcat(body,"]"); response->corpo=strdup(body); return 1;
}
static void waitFeed(void) {
  for (int i=0;i<2000 && extras_comentarios_ep_carregando();i++) SDL_Delay(1);
  assert(!extras_comentarios_ep_carregando());
}
int main(void) {
  assert(!SDL_Init(SDL_INIT_TIMER));
  CatItem item={0}; snprintf(item.imdb,sizeof item.imdb,"tt9000100");
  for (movies=0;movies<2;movies++) {
    snprintf(item.tipo,sizeof item.tipo,"%s",movies ? "movie" : "series");
    cat_definir_tudo(&item,1,NULL,0);
    extras_abrir_sessao(); requests=0;
    extras_pedir_comentarios_ep(item.imdb,0,0); waitFeed();
    assert(requests==1 && extras_n_comentarios_ep()==8 && extras_comentarios_ep_tem_mais());
    assert(extras_comentarios_ep_do(item.imdb,0,0));
    extras_comentarios_ep_mais(); waitFeed();
    assert(requests==2 && extras_n_comentarios_ep()==9 && !extras_comentarios_ep_tem_mais());
    assert(!strcmp(extras_comentario_ep_texto(8),"Complete review 8"));
    extras_comentarios_ep_mais(); assert(requests==2 && !ratings);
  }
  puts("detail lower feed: movies/series, pagination, full text and endpoint selection OK");
  SDL_Quit(); return 0;
}
