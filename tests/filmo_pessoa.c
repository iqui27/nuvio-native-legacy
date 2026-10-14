#include <assert.h>
#include <time.h>
#include "../src/pessoa.c"
const char *desc_chave_tmdb(void){return "test";}
const char *desc_tmdb_idioma(void){return "pt-BR";}
const char *i18n(const char *s){return s;}
static const char *payload;
char *rede_baixar(const char *url,int timeout){(void)url;(void)timeout;return strdup(payload);}
int main(int argc,char **argv){assert(argc==2);
 if(!strcmp(argv[1],"date")){payload="{\"name\":\"A\",\"biography\":\"Bio\",\"cast\":[{\"id\":1,\"title\":\"Future\",\"release_date\":\"2999-01-01\"}]}";idPedido=idEmCurso=1;buscar(NULL);assert(nCred==1&&cred[0].proximo);}
 if(!strcmp(argv[1],"identity")){idPedido=2;assert(!pessoa_ler(1,"{\"name\":\"A\"}","2026-01-01"));assert(idAtual!=1);}
 if(!strcmp(argv[1],"bio")){char *b=malloc(20000);memset(b,'x',19999);b[19999]=0;char *j=malloc(20100);snprintf(j,20100,"{\"name\":\"A\",\"biography\":\"%s\"}",b);assert(pessoa_ler(1,j,"2026-01-01"));assert(strlen(pessoa_bio())==19999);free(j);free(b);}
 puts(argv[1]);return 0;
}
