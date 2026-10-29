#include "horafmt.h"
#define ESPEVENTO_IMPLEMENTED 1
/* Event details share guide navigation, playback and profile reminders.
 * espseguir owns following; it may register these two callbacks on startup. */
static void (*gs_follow_action)(const EspEvento *);
static int (*gs_follow_state)(const EspEvento *);
void g5ui_sports_follow(void (*action)(const EspEvento *),int (*state)(const EspEvento *)){
  gs_follow_action=action;gs_follow_state=state;
}
typedef struct {char id[128],name[200],base[512];int confirmed;} GSWatch;
static GSWatch gs_watch[8];static int gs_watch_n,gs_watch_cursor;
static unsigned gs_watch_revision,gs_watch_epg,gs_event_revision;
static char gs_kickoff[100],gs_venue[300],gs_reminder_id[128];
static void gs_watch_add(const char *id,const char *name,const char *base,int confirmed){
  for(int j=0;j<gs_watch_n;j++)if(!strcmp(gs_watch[j].id,id)){gs_watch[j].confirmed|=confirmed;return;}
  if(gs_watch_n>=8)return;
  GSWatch *c=&gs_watch[gs_watch_n++];memset(c,0,sizeof *c);
  snprintf(c->id,sizeof c->id,"%s",id);snprintf(c->name,sizeof c->name,"%s",name);
  snprintf(c->base,sizeof c->base,"%s",base);c->confirmed=confirmed;
}
static void gs_watch_step(void){
  if(gs_event_revision!=esportes_revisao()){
    gs_event_revision=esportes_revisao();
    for(int i=0;i<esportes_eventos_n();i++){
      const EspEvento *e=esportes_evento(i);
      if(!strcmp(e->id,gs_selected.id)&&!strcmp(e->ligaId,gs_selected.ligaId)&&!strcmp(e->modalidade,gs_selected.modalidade)){gs_selected=*e;break;}
    }
  }
  if(gs_watch_revision!=guia_busca_revisao()||gs_watch_epg!=epg_revisao()){
    gs_watch_n=gs_watch_cursor=0;gs_watch_revision=guia_busca_revisao();gs_watch_epg=epg_revisao();
  }
  Uint32 start=SDL_GetTicks();
  for(int batch=0;batch<8&&SDL_GetTicks()-start<2;batch++){
    const char *id,*name,*group,*base;
    if(!guia_canal_regra_campos(gs_watch_cursor,NULL,NULL,NULL,NULL,NULL))break;
    int i=gs_watch_cursor++;
    if(!guia_canal_campos(i,&id,&name,NULL,&group,&base)||!guia_busca_elegivel(i)||!g5ui_channel_allowed(id,name,group,base))continue;
    int confirmed=0;
    /* Broadcaster links explicitly say which channel carries the event; EPG
     * confirmation is additional evidence, never fabricated from affiliation. */
    EpgProg programs[16];time_t end=gs_selected.fim>gs_selected.inicio?gs_selected.fim:gs_selected.inicio+6*3600;
    int n=guia_busca_programas(i,gs_selected.inicio-3*3600,end,0,programs,16);
    for(int p=0;p<n;p++)if(esportes_corresponde(&gs_selected,programs[p].titulo,programs[p].ini,programs[p].fim))confirmed=1;
    if(confirmed||esportes_emissora(&gs_selected,name))gs_watch_add(id,name,base,confirmed);
  }
}
static void gs_event_open(const EspEvento *e){
  gs_selected=*e;gs_event_revision=esportes_revisao();gs_watch_n=gs_watch_cursor=0;
  gs_watch_revision=guia_busca_revisao();gs_watch_epg=epg_revisao();
  struct tm tm;gs_kickoff[0]=0;
  if(localtime_r(&e->inicio,&tm)){
    char date[64],hour[24];strftime(date,sizeof date,ajustes_idioma()==IDIOMA_EN?"%m/%d/%Y":"%d/%m/%Y",&tm);hora_tela(hour,sizeof hour,&tm);
    snprintf(gs_kickoff,sizeof gs_kickoff,"%s · %s",date,hour);
  }
  snprintf(gs_venue,sizeof gs_venue,"%s%s%s",e->local,e->cidade[0]&&e->local[0]?" · ":"",e->cidade);
  snprintf(gs_reminder_id,sizeof gs_reminder_id,"sport:%.16s:%.24s:%.40s",e->modalidade,e->ligaId,e->id);
  guia_carregar();epg_iniciar();gs_watch_step();
  gs_open("esportes-evento");focus=e->estado==0?2088:gs_watch_n?2080:2088;
}
static const char *gs_reminder_key(void){return gs_reminder_id;}
static void gs_remind(void){
  if(gs_selected.estado!=0&&gs_selected.estado!=1)return;
  if(lembrete_perfil()!=perfis_ativo())lembrete_carregar(perfis_ativo());
  /* Channel-independent reminders have no Watch action when they expire. */
  int r=lembrete_alternar(gs_reminder_key(),gs_selected.liga,gs_selected.nome,"",gs_selected.inicio,gs_selected.fim>gs_selected.inicio?gs_selected.fim:gs_selected.inicio+7200);
  glem_aviso_curto(i18n(r<0?"Não foi possível salvar":r?"Lembrete marcado  ·  OK cancela":"Lembrete cancelado"));
}
static void gs_team(const EspParticipante *p,int sport,float x,float y,float size){
  GLuint texture=p->logo[0]?tex_obter_larg(p->logo,size):0;
  if(texture){esportesui_logo(p->logo,sport,(GfxRect){x,y,size,size});return;}
  char mark[20]="";
  if(p->sigla[0])snprintf(mark,sizeof mark,"%.19s",p->sigla);
  else{
    const unsigned char *s=(const unsigned char *)p->nome;int n=0,letters=0;
    while(*s&&letters++<3){int len=(*s&0xf8)==0xf0?4:(*s&0xf0)==0xe0?3:(*s&0xe0)==0xc0?2:1;
      for(int j=0;j<len&&*s;j++)mark[n++]=(char)*s++;
      while(*s&&*s!=' ')s++;while(*s==' ')s++;}
  }
  gfx_cor((GfxRect){x,y,size,size},.5f,.16f,.17f,.20f,1);
  TxtLinha l=txt_linha_bruta(TXT_ILHA_TITULO,mark,243,242,239,255);
  txt_desenhar(l,x+(size-l.w)*.5f,y+(size-l.h)*.5f);
}
static void gs_event_draw(void){
  const EspEvento *e=&gs_selected;int sport=gs_sport_id(e->modalidade);
  gv_button(2020,i18n("Voltar"),"pl_chevron-left",GV_MX,210,180,0,1);
  gv_label(TXT_ILHA_NOME,e->liga,GV_MX+210,222,1000,1);
  GfxRect panel={GV_MX,306,GV_MW,370};plrui_cartao(panel,28,1);
  for(int p=0;p<e->nParticipantes;p++){
    float center=GV_MX+GV_MW*(p?.76f:.24f);
    gs_team(&e->participantes[p],sport,center-75,330,150);
    TxtLinha name=txt_linha_corta_bruta(TXT_ILHA_NOME,e->participantes[p].nome,243,242,239,255,GV_MW*.36f);
    txt_desenhar(name,center-name.w*.5f,502);
  }
  if(e->nParticipantes!=2)gv_label(TXT_ILHA_TITULO,e->nome,GV_MX+40,400,GV_MW-80,0);
  char score[80];snprintf(score,sizeof score,"%s  :  %s",e->participantes[0].pontos[0]?e->participantes[0].pontos:"–",e->participantes[1].pontos[0]?e->participantes[1].pontos:"–");
  const char *status=e->estado==1?"Ao vivo":e->estado==2?"Encerrado":e->estado==3?e->detalhe:"Próximos jogos";
  TxtLinha state=txt_linha_corta_bruta(TXT_ILHA_SUB,i18n(status),154,156,158,255,GV_MW*.3f);
  txt_desenhar(state,GV_MX+(GV_MW-state.w)*.5f,340);
  if(e->nParticipantes==2){
    TxtLinha value=txt_linha_bruta(e->estado==0?TXT_ILHA_TITULO:TXT_AJ_NUM64,e->estado==0?"×":score,243,242,239,255);
    txt_desenhar(value,GV_MX+(GV_MW-value.w)*.5f,396);
  }
  TxtLinha detail=txt_linha_corta_bruta(TXT_ILHA_SUB,e->detalhe,154,156,158,255,GV_MW*.3f);
  txt_desenhar(detail,GV_MX+(GV_MW-detail.w)*.5f,486);
  gv_label(TXT_ILHA_SUB,gs_venue,GV_MX+40,582,GV_MW-80,1);
  gv_label(TXT_ILHA_SUB,gs_kickoff,GV_MX+40,620,GV_MW-80,1);
  gv_label(TXT_ILHA_NOME,i18n("Onde assistir"),GV_MX,712,600,0);
  for(int j=0;j<gs_watch_n;j++)gv_button(2080+j,gs_watch[j].name,"aj_play",GV_MX+(j%4)*360,766+(j/4)*70,342,0,1);
  if(!gs_watch_n){
    char broadcasters[700]="";
    for(int j=0;j<e->nEmissoras;j++){size_t n=strlen(broadcasters);snprintf(broadcasters+n,sizeof broadcasters-n,"%s%s",j?" · ":"",e->emissoras[j]);}
    gv_label(TXT_ILHA_SUB,broadcasters[0]?broadcasters:i18n("Nenhum canal encontrado para este evento"),GV_MX,772,GV_MW,1);
  }
  if(e->estado==0||e->estado==1)gv_button(2088,i18n(lembrete_achar(gs_reminder_key(),e->inicio,e->nome)>=0?"Lembrete marcado  ·  OK cancela":"Lembrar-me"),"aj_bell",GV_MX,gs_watch_n>4?922:866,530,0,1);
  if(gs_follow_action)gv_button(2089,i18n(gs_follow_state&&gs_follow_state(e)?"Seguindo":"Seguir"),"aj_star",GV_MX+550,gs_watch_n>4?922:866,260,0,1);
}
