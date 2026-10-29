// Recent rows share the provider library pager without altering library IDs.
#include "servidores.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char asked[192];
int ajustes_jellyfin_ligado(void){return 1;}
int jellyfin_chave_fileira(const char *k){return !strncmp(k,"jellyfin_",9);}
int emby_chave_fileira(const char *k){return !strncmp(k,"emby_",5);}
int plex_chave_fileira(const char *k){return !strncmp(k,"plex_",5);}
static int page(const char *k,int start,int limit,CatItem *out,int max,int *total,int *raw){
 (void)out;(void)max;assert(start==0&&limit==24);snprintf(asked,sizeof asked,"%s",k);*total=1;*raw=1;return 1;
}
int jellyfin_pagina(const char *k,int a,int b,CatItem *c,int d,int *e,int *f){return page(k,a,b,c,d,e,f);}
int emby_pagina(const char *k,int a,int b,CatItem *c,int d,int *e,int *f){return page(k,a,b,c,d,e,f);}
int plex_pagina(const char *k,int a,int b,CatItem *c,int d,int *e,int *f){return page(k,a,b,c,d,e,f);}
int main(void){
 CatItem item;int total,raw;
 const char *keys[]={"plex_recent_42","jellyfin_recent_abcdef","emby_recent_1234","plex_group_5"};
 const char *expected[]={"plex_42","jellyfin_abcdef","emby_1234","plex_group_5"};
 for(int i=0;i<4;i++){assert(servidores_pagina(keys[i],0,24,&item,1,&total,&raw)==1);assert(!strcmp(asked,expected[i])&&total==1&&raw==1);}
 puts("Server row pager: recent library aliases and collection keys: PASS");return 0;
}
