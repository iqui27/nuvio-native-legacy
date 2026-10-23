#include "servidores.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static char *saved;static char filename[80];
int dados_gravar_leve(const char *name,const char *body){snprintf(filename,sizeof filename,"%s",name);free(saved);saved=strdup(body);return 1;}
char *dados_ler(const char *name){return saved&&!strcmp(name,filename)?strdup(saved):NULL;}
int dados_apagar(const char *name){if(!strcmp(name,filename)){free(saved);saved=NULL;}return 1;}
int main(void){
 CatItem item={0},out={0};strcpy(item.imdb,"tt123");strcpy(item.tipo,"movie");strcpy(item.titulo,"Offline fixture");strcpy(item.poster,"https://host/secret?api_key=fixture-secret");item.progresso=42;
 assert(servidores_cache_gravar(1,2,"server-a",&item,1));assert(!strstr(saved,"fixture-secret")&&!strstr(saved,"https://"));
 assert(!servidores_cache_ler(2,2,"server-a",&out,1));assert(!servidores_cache_ler(1,2,"server-b",&out,1));
 assert(servidores_cache_ler(1,2,"server-a",&out,1)==1&&out.progresso==42&&!out.poster[0]);
 assert(servidores_cache_instante(1,2,"server-a")>0);
 assert(!servidores_cache_instante(2,2,"server-a")&&!servidores_cache_instante(1,2,"server-b"));
 servidores_cache_apagar(1,2);assert(!saved);assert(!servidores_cache_instante(1,2,"server-a"));puts("Server cache: profile/server isolation, offline metadata, no art credentials, removal: PASS");return 0;
}
