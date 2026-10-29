/* Team picker stays separate from the event/detail renderer. */
static int gs_team_ids[ESP_MAX_TIMES],gs_team_n;
static EspEvento gs_team_event(int i){
  EspEvento e={0};const EspLiga *l=esportes_catalogo_liga(gs_league);
  const EspParticipante *t=esportes_time(i);
  if(l&&t&&esportes_modalidade(gs_sport)){
    snprintf(e.modalidade,sizeof e.modalidade,"%s",esportes_modalidade(gs_sport)->id);
    snprintf(e.ligaId,sizeof e.ligaId,"%s",l->id);e.nParticipantes=1;e.participantes[0]=*t;
  }
  return e;
}
static void gs_team_collect(void){
  gs_team_n=0;
  for(int i=0;i<esportes_times_n();i++)if(gs_contains(esportes_time(i)->nome))gs_team_ids[gs_team_n++]=i;
}
static void gs_team_draw(void){
  gs_team_collect();gs_pager(gs_team_n,6);
  const EspLiga *l=esportes_catalogo_liga(gs_league);
  gv_label(TXT_ILHA_SUB,l?l->nome:"",GV_MX,382,GV_MW,1);
  for(int j=0;j<6&&gs_page*6+j<gs_team_n;j++){
    int k=gs_team_ids[gs_page*6+j];EspEvento e=gs_team_event(k);
    const EspParticipante *t=esportes_time(k);float y=438+j*76;
    gv_opt(2800+j,esportes_segue_time(&e,0)?"aj_check":"aj_plus",t->nome,NULL,GV_MX,y,GV_MW,66);
    esportesui_logo(t->logo,gs_sport,(GfxRect){GV_MX+GV_MW-90,y+9,48,48});
  }
  if(!gs_team_n)gs_empty(458);
}
static int gs_team_arrow(SDL_Keycode k){
  if(strcmp(screen,"esportes-times")||focus<2800||focus>=2806)return 0;
  gs_team_collect();int n=gs_page*6+focus-2800;
  if(k==SDLK_UP&&n==0){focus=2021;return 1;}
  if(k==SDLK_LEFT){gv_side_enter();return 1;}
  if(k==SDLK_RIGHT)return 1;
  int next=n+(k==SDLK_DOWN?1:-1);
  if(next>=0&&next<gs_team_n){gs_page=next/6;focus=2800+next%6;}return 1;
}
static void gs_follow_badge(const EspEvento *e,GfxRect r){
  if(!esportes_segue_evento(e))return;
  float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);
  gfx_icone((GfxRect){r.x+r.w-46,r.y+r.h-38,22,22},esportes_segue_jogo(e)?"pl_bell":"aj_star",ar,ag,ab,1);
}
