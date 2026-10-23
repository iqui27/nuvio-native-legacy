#include "../src/streams.c"
#include <assert.h>
#include <stdio.h>
int main(void){
  Stream fixtures[4]={0};lista=fixtures;n=4;
  strcpy(fixtures[0].bingeGroup,"plex|fixture");
  strcpy(fixtures[1].bingeGroup,"jellyfin|fixture");
  strcpy(fixtures[2].bingeGroup,"emby|fixture");
  filtro=soMp4=soCache=soDub=0;
  for(int p=0;p<3;p++){stream_folha_servidor(p);for(int i=0;i<4;i++)assert(passaFiltro(i)==(i==p));}
  stream_folha_servidor(-1);for(int i=0;i<4;i++)assert(passaFiltro(i));
  puts("Sources: provider chip filters only its server, all sources restore: PASS");return 0;
}
