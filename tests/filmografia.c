#include "filmomodelo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

const char *i18n(const char *s) { return s; }
int main(void) {
  FmBruto b[3] = {{.tmdb=1,.papel=FM_ATUA,.ano=2000,.ordem=0,.votos=20},
                   {.tmdb=1,.papel=FM_DIR,.ano=2000},
                   {.tmdb=2,.tv=1,.papel=FM_ATUA,.ano=2001,.eps=1}};
  FmCredito c[3]; FmEstado st[3]; FmConquista cq[FM_CQ_MAX];
  strcpy(b[0].titulo,"One"); strcpy(b[1].titulo,"One"); strcpy(b[2].titulo,"Guest");
  assert(fm_juntar(b,3,"2026-10-13",c,3)==2);
  assert(c[0].papeis==(FM_ATUA|FM_DIR));
  st[0]=fm_estado(c,0,-1,NULL,FM_COMO_DIRETO,"2026-10-13");
  assert(st[0].estado==FM_VISTO && !fm_conta(st));
  st[0]=fm_estado(c,100,-1,"2026-10-13",FM_COMO_DIRETO,"2026-10-13");
  /* Importing our manual history must not launder it into an achievement. */
  assert(st[0].manual && !fm_conta(st));
  st[0]=fm_estado(c,0,-1,NULL,FM_COMO_QUIZ,"2026-10-13");
  assert(fm_conta(st));
  st[1]=fm_estado(c+1,0,0,NULL,0,NULL); assert(st[1].estado==FM_NAO);
  st[1]=fm_estado(c+1,0,1,"2026-10-13",0,NULL); assert(fm_conta(st+1));
  c[1].eps=0;
  st[1]=fm_estado(c+1,0,1,NULL,0,NULL); assert(st[1].estado==FM_ANDA);
  c[1].eps=10;
  st[1]=fm_estado(c+1,0,9,NULL,0,NULL); assert(st[1].estado==FM_ANDA);
  st[1]=fm_estado(c+1,0,10,"2026-10-13",0,NULL); assert(fm_conta(st+1));
  assert(fm_conquistas(42,"Person",c,st,2,cq)>0 && cq[0].nivel==4);
  char date[11]; fm_data_vista("2000-01-01",1,"2026-10-13",date,sizeof date);
  assert(!strcmp(date,"2000-01-01"));
  assert(fm_spoiler("the ending") && fm_spoiler("morte do herói") && !fm_spoiler("space travel"));
  const char *dp[]={"Jules","Will","Finn"}, *dc[]={"A","B","C"}, *dt[]={"war","music","sport","murder"};
  strcpy(c[0].personagem,"Alice"); strcpy(c[0].com[0],"Bob"); c[0].nCom=1;
  strcpy(c[0].kw[0],"space travel"); strcpy(c[0].kw[1],"ending"); c[0].nKw=2;
  FmQuiz q; fm_quiz_montar(&q,c,"Person",0,dp,3,dc,3,dt,4,1);
  assert(q.nq>=2);
  for(int i=0;i<q.nq;i++) for(int j=0;j<4;j++) assert(!fm_spoiler(q.q[i].opc[j]));
  while(fm_quiz_pergunta(&q)) fm_quiz_responder(&q,fm_quiz_pergunta(&q)->certa);
  assert(q.resultado==1);
  fm_quiz_montar(&q,c,"Person",0,dp,3,dc,3,dt,4,2);
  fm_quiz_responder(&q,(fm_quiz_pergunta(&q)->certa+1)%4);
  fm_quiz_responder(&q,fm_quiz_pergunta(&q)->certa);
  assert(q.nAsk==3);
  fm_quiz_responder(&q,fm_quiz_pergunta(&q)->certa); assert(q.resultado==1);
  long fails[]={100,200}; assert(fm_tentativas(fails,2,300)==0);
  assert(fm_tentativas(fails,2,90000)==2);
  FmFiltro f={.tipo=FM_T_SERIE}; int idx[3]; assert(fm_filtrar(c,st,2,&f,idx,3)==1 && idx[0]==1);
  puts("filmografia: OK"); return 0;
}
