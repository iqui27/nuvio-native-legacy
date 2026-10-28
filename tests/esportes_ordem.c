/* Exercise the actual worker comparator independently of libc's sort algorithm.
 * Games with the same kickoff must keep one order as scores/pages refresh. */
#include "../src/esportes.c"
#include <assert.h>
int main(void) {
  EspEvento entrada[5]={0};
  const char *ids[]={"game-c","game-a","game-b","early","live"};
  for(int i=0;i<5;i++) {
    snprintf(entrada[i].id,sizeof entrada[i].id,"%s",ids[i]);
    snprintf(entrada[i].modalidade,sizeof entrada[i].modalidade,"football");
    snprintf(entrada[i].ligaId,sizeof entrada[i].ligaId,"nfl");
    entrada[i].inicio=1000;
  }
  entrada[3].inicio=900;entrada[4].estado=1;
  assert(ordem(&entrada[0],&entrada[1])>0);
  assert(ordem(&entrada[1],&entrada[0])<0);
  assert(ordem(&entrada[1],&entrada[1])==0);
  const char *esperado[]={"live","early","game-a","game-b","game-c"};
  for(int pagina=0;pagina<10;pagina++) {
    EspEvento recebidos[5];
    for(int i=0;i<5;i++) {
      recebidos[i]=entrada[(i+pagina)%5];
      snprintf(recebidos[i].participantes[0].pontos,sizeof recebidos[i].participantes[0].pontos,"%d",pagina*7+i);
    }
    qsort(recebidos,5,sizeof *recebidos,ordem);
    for(int i=0;i<5;i++)assert(!strcmp(recebidos[i].id,esperado[i]));
  }
  /* Favorites can contain equal-time events from different sports/leagues. */
  EspEvento a=entrada[1],b=a;
  strcpy(b.modalidade,"soccer");assert(ordem(&a,&b)<0);
  b=a;strcpy(b.ligaId,"college-football");assert(ordem(&a,&b)>0);
  puts("sports same-time matches retain a deterministic order across page/score refreshes: passed");
  return 0;
}
