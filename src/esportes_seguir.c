/* Follow reminders use the guide's persistent queue and animated notification. */
#include "esportes.h"
#include "lembrete.h"
#include "perfis.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void reminder(const EspEvento *e,int update){
  if(!esportes_segue_evento(e)||e->estado!=0||e->inicio<=time(NULL))return;
  char key[300],id[80],base[600];
  snprintf(key,sizeof key,"%s|%s|%s",e->modalidade,e->ligaId,e->id);
  uint64_t h=UINT64_C(14695981039346656037);
  for(const unsigned char *p=(const unsigned char *)key;*p;p++){h^=*p;h*=UINT64_C(1099511628211);}
  snprintf(id,sizeof id,"sports-%016llx",(unsigned long long)h);
  snprintf(base,sizeof base,"sports|%s|%s|%s|%s|%s",e->modalidade,e->ligaId,e->id,e->nParticipantes>0?e->participantes[0].id:"-",e->nParticipantes>1?e->participantes[1].id:"-");
  /* Match by stable event ID even if the provider reschedules or renames it. */
  for(int i=0;i<lembrete_n();i++)if(!strcmp(lembrete_item(i)->canal,id)){
    if(update)lembrete_ajustar(i,e->inicio,e->fim>e->inicio?e->fim:e->inicio+3*3600);
    return;
  }
  lembrete_alternar(id,e->liga[0]?e->liga:"Esportes",e->nome,base,e->inicio,e->fim>e->inicio?e->fim:e->inicio+3*3600);
}
void esportes_seguir_lembretes(void){
  esportes_abrir_sessao();
  if(lembrete_perfil()!=perfis_ativo())lembrete_carregar(perfis_ativo());
  for(int i=lembrete_n()-1;i>=0;i--){
    const Lembrete *l=lembrete_item(i);if(strncmp(l->base,"sports|",7))continue;
    EspEvento e={0};
    if(sscanf(l->base+7,"%31[^|]|%79[^|]|%99[^|]|%79[^|]|%79[^|]",e.modalidade,e.ligaId,e.id,e.participantes[0].id,e.participantes[1].id)!=5)continue;
    e.nParticipantes=2;
    if(!esportes_segue_evento(&e)){
      char id[80],title[120];strcpy(id,l->canal);strcpy(title,l->titulo);time_t t=l->ini;
      lembrete_alternar(id,"",title,"",t,t+60);
    }
  }
  for(int i=0;i<esportes_jogos_seguidos_n();i++)reminder(esportes_jogo_seguido(i),0);
  for(int i=0;i<esportes_favoritos_n();i++)reminder(esportes_favoritos_evento(i),1);
  for(int i=0;i<esportes_eventos_n();i++)reminder(esportes_evento(i),1);
  for(int i=0;i<esportes_catalogo_n();i++)for(int j=0;j<esportes_catalogo_eventos_n(i);j++)reminder(esportes_catalogo_evento(i,j),1);
}
