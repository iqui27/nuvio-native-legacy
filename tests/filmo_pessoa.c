#include <assert.h>
#include <time.h>
#include "filmomodelo.h"
int ajustes_tmdb_elenco(void) { return 1; }
static int trocar;
static int juntarDuranteTroca(const FmBruto *,int,const char *,FmCredito *,int);
#define fm_juntar juntarDuranteTroca
#include "../src/pessoa.c"
#undef fm_juntar
static int juntarDuranteTroca(const FmBruto *b,int n,const char *h,FmCredito *c,int max){int result=fm_juntar(b,n,h,c,max);if(trocar)idPedido=2;return result;}
const char *desc_chave_tmdb(void){return "test";}
const char *desc_tmdb_idioma(void){return "pt-BR";}
const char *i18n(const char *s){return s;}
static const char *payload;
char *rede_baixar(const char *url,int timeout){(void)url;(void)timeout;return strdup(payload);}
int main(int argc,char **argv){assert(argc==2);
 if(!strcmp(argv[1],"lifetime")){
  assert(pessoa_ler(1,"{\"name\":\"A\",\"biography\":\"Original biography\"}","2026-01-01"));
  char *rendering=pessoa_bio();
  payload="{\"name\":\"A\",\"biography\":\"English fallback\"}";idPedido=idEmCurso=1;
  buscar(NULL);
  assert(!strcmp(rendering,"Original biography"));
  char *current=pessoa_bio();assert(!strcmp(current,"English fallback"));free(current);free(rendering);
 }
 if(!strcmp(argv[1],"date")){payload="{\"name\":\"A\",\"biography\":\"Bio\",\"cast\":[{\"id\":1,\"title\":\"Future\",\"release_date\":\"2999-01-01\"}]}";idPedido=idEmCurso=1;buscar(NULL);assert(nCred==1&&cred[0].proximo);}
 if(!strcmp(argv[1],"identity")){idPedido=1;trocar=1;assert(!pessoa_ler(1,"{\"name\":\"A\"}","2026-01-01"));assert(idAtual!=1);}
 if(!strcmp(argv[1],"bio")){char *b=malloc(20000);memset(b,'x',19999);b[19999]=0;char *j=malloc(20100);snprintf(j,20100,"{\"name\":\"A\",\"biography\":\"%s\"}",b);assert(pessoa_ler(1,j,"2026-01-01"));char *snapshot=pessoa_bio();assert(strlen(snapshot)==19999);free(snapshot);free(j);free(b);}
 puts(argv[1]);return 0;
}
