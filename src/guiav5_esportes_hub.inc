/* Sports shelves share the match texture cache and guide focus registration. */
static void gs_hub(void){
  const int section[]={1,0,2,3,4,5};
  float tab=(GV_MW-60)/6;
  for(int i=0;i<6;i++)gv_button(2070+i,i18n(i==1?"Ao vivo":gs_sections[i]),gs_icons[i],GV_MX+i*(tab+12),302,tab,0,1);
  int active=focus>=3000&&focus<5400?(focus-3000)/400:0;
  gs_scroll=anim_mola2(&gs_scroll_v,gs_scroll,active*380.f,gv_dt,T21_SCROLL);
  if(focus>=3000&&focus<5400){
    int at=(focus-3000)%400;
    gv_registra(focus,GV_MX+(at-gs_shelf_offset[active])*GS_CARD_STEP,452,GS_CARD_W,282);
  }
  gfx_recorte(GV_MX-22,380,GV_MW+44,642);
  for(int s=0;s<6;s++){
    float y=394+s*380-gs_scroll;
    if(y+350<380||y>1022)continue;
    const char *title=i18n(s==0?"Ao vivo":gs_sections[section[s]]);
    float labelX=GV_MX;
    if(s<4&&gs_shelf_n[s]){
      const EspEvento *e=&gs_games[gs_shelf_ids[s][gs_shelf_offset[s]<gs_shelf_n[s]?gs_shelf_offset[s]:0]];
      for(int k=0;k<gs_count;k++){
        const EspLiga *l=esportes_catalogo_liga(gs_indices[k]);
        if(l&&!strcmp(l->id,e->ligaId)){
          esportesui_logo(l->logo,gs_sport_id(e->modalidade),(GfxRect){GV_MX,y-3,30,30});labelX+=42;break;
        }
      }
    }
    gv_label(TXT_ILHA_NOME,title,labelX,y,GV_MW-60,0);
    gfx_cor((GfxRect){GV_MX,y+34,GV_MW,1},0,1,1,1,.08f);
    int n=gs_shelf_n[s];
    if(gs_shelf_offset[s]>=n)gs_shelf_offset[s]=0;
    gs_shelf_scroll[s]=anim_mola2(&gs_shelf_velocity[s],gs_shelf_scroll[s],gs_shelf_offset[s],gv_dt,T21_SCROLL);
    int start=(int)floorf(fmaxf(0,gs_shelf_scroll[s]));
    for(int j=0;j<4&&start+j<n;j++){
      int at=start+j,id=3000+s*400+at;
      GfxRect r={GV_MX+(at-gs_shelf_scroll[s])*GS_CARD_STEP,y+58,GS_CARD_W,282};
      if(r.x>=GV_MX+GV_MW+24||r.x+r.w<=GV_MX-24)continue;
      if(s<4){
        const EspEvento *e=&gs_games[gs_shelf_ids[s][at]];
        esportesui_cartao(e,r,gv_btn_foc(id),s*4+at%4);
      }else if(s==4){
        int k=gs_indices[at],m=esportes_catalogo_modalidade(k);const EspLiga *l=esportes_catalogo_liga(k);
        int reg=gv_reg;gv_reg=reg&&r.y>=380&&r.y+232<=1024;
        gs_card(id,l->nome,i18n(esportes_modalidade(m)->nome),"",r.x,r.y);gv_reg=reg;
        esportesui_logo(l->logo,m,(GfxRect){r.x+24,r.y+16,68,68});
      }else{
        const EspNoticia *a=esportes_noticia(at);
        int reg=gv_reg;gv_reg=reg&&r.y>=380&&r.y+232<=1024;
        gs_card(id,a->titulo,a->origem[0]?a->origem:"ESPN","aj_scroll-text",r.x,r.y);gv_reg=reg;
      }
      if(r.y>=380&&r.y+r.h<=1024){
        ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);
        gv_registra(id,r.x,r.y,r.w,r.h);
      }
    }
    if(!n){
      int state=gs_empty_state(gs_pages[section[s]]);
      if(state){
        int reg=gv_reg;gv_reg=reg&&y+58>=380&&y+288<=1024;
        gs_empty_draw(y+58,state,3000+s*400);gv_reg=reg;continue;
      }
      GfxRect r={GV_MX,y+58,GV_MW,232};plrui_cartao(r,24,1);
      gfx_icone((GfxRect){r.x+28,r.y+30,44,44},gs_icons[section[s]],.65f,.67f,.7f,1);
      gv_label(TXT_ILHA_NOME,i18n(s==1?"Escolha seus times favoritos":"Nenhum resultado"),r.x+96,r.y+34,r.w-120,0);
      gv_button(3000+s*400,i18n(s==1?"Escolher times":"Ver todos"),"aj_chevron-right",r.x+96,r.y+106,240,0,0);
    }
  }
  gfx_sem_recorte();
  /* Quiet position rail communicates depth without pagination or counters. */
  gfx_cor((GfxRect){GV_MX+GV_MW+23,394,3,600},.5f,1,1,1,.06f);
  gfx_cor((GfxRect){GV_MX+GV_MW+23,394+fminf(1,gs_scroll/(5*380.f))*500,3,100},.5f,1,1,1,.25f);
}
static int gs_hub_activate(void){
  if(strcmp(screen,"esportes-ligas")||focus<3000||focus>=5400)return 0;
  int s=(focus-3000)/400,at=(focus-3000)%400;
  if(at>=gs_shelf_n[s]){
    const int section[]={1,0,2,3,4,5};
    if(gs_empty_state(gs_pages[section[s]])<0){gs_retry();return 1;}
    if(s==1){gs_choosing=1;gs_sport=-1;gs_open("esportes-modalidades");}
    else gs_open(gs_pages[section[s]]);
    return 1;
  }
  if(s<4)gs_event_open(&gs_games[gs_shelf_ids[s][at]]);
  else if(s==4){
    int k=gs_indices[at];const EspLiga *l=esportes_catalogo_liga(k);
    gs_open("esportes-liga");gs_league=k;gs_sport=esportes_catalogo_modalidade(k);
    esportes_catalogo_suspender();esportes_pedir_eventos(gs_sport,l,0);gs_refresh=SDL_GetTicks();
  }else{gs_article=at;gs_open("esportes-artigo");}
  return 1;
}
