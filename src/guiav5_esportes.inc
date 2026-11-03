/* PR #619 supplies metadata, artwork, profile preferences and Sources matching.
 * This adapter keeps guide v5 geometry, focus, motion and Back navigation. */
#define ESPOLIR_IMPLEMENTED 1
#define GS_CARD_W ((GV_MW-48)/3)
#define GS_CARD_STEP (GS_CARD_W+24)
static int gs_choosing;
static float gs_scroll,gs_scroll_v,gs_grid_row,gs_grid_v;
static char gs_grid_page[48];
static int gs_shelf_offset[6];
static float gs_shelf_scroll[6],gs_shelf_velocity[6];
static int gs_shelf_ids[6][ESP_MAX_EVENTOS],gs_shelf_n[6];
static int gs_sport=-1,gs_league=-1,gs_page;
typedef struct { char page[48],query[140];int focus,offset,sport,league; } GSStep;
static GSStep gs_history[12];static int gs_depth;
static EspEvento gs_games[ESP_MAX_EVENTOS],gs_selected;
static int gs_n,gs_indices[ESP_MAX_FILEIRAS],gs_count,gs_article;
static Uint32 gs_refresh;
static int gs_started;
static unsigned gs_revision=~0u;
static char gs_collection_page[48],gs_collection_query[140];
static int gs_collection_sport=-2;
static time_t gs_collection_minute;
static SDL_Keycode gs_key;static Uint32 gs_repeat_at;
static void gs_cards_clear(void);
static void gs_reset(void){
  gs_choosing=0;
  gs_cards_clear();
  if(gs_started)esportesui_fechar();
  gs_started=gs_depth=gs_page=gs_n=gs_count=0;gs_sport=gs_league=-1;
  gs_scroll=gs_scroll_v=gs_grid_row=gs_grid_v=0;gs_grid_page[0]=0;
  memset(gs_shelf_offset,0,sizeof gs_shelf_offset);
  memset(gs_shelf_scroll,0,sizeof gs_shelf_scroll);
  memset(gs_shelf_velocity,0,sizeof gs_shelf_velocity);
  gs_revision=~0u;gs_collection_page[0]=0;gs_key=0;
}
static const char *gs_sections[]={"Meus times","Ao vivo / Jogos de hoje","Próximos jogos","Resultados","Por liga","Notícias"};
static const char *gs_icons[]={"aj_star","aj_activity","aj_calendar","aj_trophy","aj_rows-3","aj_scroll-text"};
static const char *gs_pages[]={"esportes-favoritos","esportes-hoje","esportes-proximos","esportes-resultados","esportes-modalidades","esportes-noticias"};
static int gs_team_arrow(SDL_Keycode k);
static void gs_follow_toggle(const EspEvento *e){
  if(!esportes_seguir_jogo(e,!esportes_segue_jogo(e)))glem_aviso_curto(i18n("Não foi possível salvar"));
  esportes_seguir_lembretes();gs_revision=~0u;
}
static void gs_start(void){
  g5ui_sports_follow(gs_follow_toggle,esportes_segue_jogo);
  if(gs_started)return;
  esportesui_iniciar();gs_started=1;esportes_noticias_pedir(0);
}
static void gs_open(const char *page){
  if(gs_depth<12){GSStep *s=&gs_history[gs_depth++];snprintf(s->page,sizeof s->page,"%s",screen);snprintf(s->query,sizeof s->query,"%s",query);s->focus=focus;s->offset=gs_page;s->sport=gs_sport;s->league=gs_league;}
  gs_page=0;query[0]=0;gv_go(page);gv_zone=0;
  focus=!strcmp(page,"esportes-ajustes")?2030:!strcmp(page,"esportes-onde-assistir")?2040:2020;
}
static void gs_back(void){
  if(gs_depth){
    GSStep s=gs_history[--gs_depth];
    if(gv_overlay())gv_voltar();
    if(strcmp(screen,s.page)){
      if(gv_overlay_nome(s.page)&&gs_depth)snprintf(gv_pagina,sizeof gv_pagina,"%s",gs_history[gs_depth-1].page);
      gv_go(s.page);
    }
    focus=s.focus;gs_page=s.offset;gs_sport=s.sport;gs_league=s.league;snprintf(query,sizeof query,"%s",s.query);
    if(!strcmp(screen,"esportes-ligas"))gs_choosing=0;
    esportes_catalogo_iniciar(gs_sport,0);
  }else{gs_page=0;gs_sport=-1;query[0]=0;gv_go(!strcmp(screen,"esportes-ligas")?"grade":"esportes-ligas");}
}
static int gs_contains(const char *s){char a[1800],b[140];if(!query[0])return 1;busca_normalizar(s,a,sizeof a);busca_normalizar(query,b,sizeof b);return strstr(a,b)!=NULL;}
static int gs_visible(int m,const char *id){return esportes_habilitado(m)&&esportes_liga_habilitada(m,id);}
static int gs_sport_id(const char *id){for(int i=0;i<esportes_modalidades_n();i++)if(!strcmp(esportes_modalidade(i)->id,id))return i;return -1;}
static int gs_today(time_t t){
  struct tm a,b;time_t now=time(NULL);return localtime_r(&t,&a)&&localtime_r(&now,&b)&&a.tm_year==b.tm_year&&a.tm_yday==b.tm_yday;
}
static int gs_accept(const EspEvento *e){
  if(!e)return 0;
  int m=gs_sport_id(e->modalidade);
  if(!esportes_habilitado(m)||
     (strcmp(screen,"esportes-favoritos")&&!esportes_liga_habilitada(m,e->ligaId))||
     (!gs_contains(e->nome)&&!gs_contains(e->liga)))return 0;
  if(!strcmp(screen,"esportes-hoje"))return e->estado==1||gs_today(e->inicio);
  if(!strcmp(screen,"esportes-proximos"))return e->estado==0&&e->inicio>time(NULL);
  if(!strcmp(screen,"esportes-resultados"))return e->estado==2;
  if(!strcmp(screen,"esportes-favoritos"))return esportes_segue_evento(e)&&
    (e->estado==1||(e->estado==0&&e->inicio>=time(NULL)-6*3600));
  return 1;
}
static int gs_order(const void *a,const void *b){
  const EspEvento *x=a,*y=b;
  int xs=x->estado==1?0:x->estado==0?1:x->estado==2?2:3;
  int ys=y->estado==1?0:y->estado==0?1:y->estado==2?2:3;
  if(xs!=ys)return xs<ys?-1:1;
  int xf=esportes_segue_evento(x),yf=esportes_segue_evento(y);
  if(xf!=yf)return xf?-1:1;
  if(x->inicio!=y->inicio)return x->estado==2&&y->estado==2?(x->inicio>y->inicio?-1:1):(x->inicio<y->inicio?-1:1);
  int k=strcmp(x->modalidade,y->modalidade);if(k)return k;
  k=strcmp(x->ligaId,y->ligaId);return k?k:strcmp(x->id,y->id);
}
static void gs_add(const EspEvento *e){
  if(!gs_accept(e))return;
  for(int i=0;i<gs_n;i++)if(!strcmp(gs_games[i].id,e->id)&&!strcmp(gs_games[i].ligaId,e->ligaId)&&!strcmp(gs_games[i].modalidade,e->modalidade))return;
  if(gs_n<ESP_MAX_EVENTOS)gs_games[gs_n++]=*e;
}
static void gs_collect(void){
  char selected[100]="",sport[32]="",league[80]="";
  if(strcmp(gs_collection_page,screen))gs_n=0;
  int shelf=!strcmp(screen,"esportes-ligas")&&focus>=3000&&focus<5400?(focus-3000)/400:-1;
  int at=shelf>=0?(focus-3000)%400:-1;
  int old=shelf>=0?(shelf<4&&at<gs_shelf_n[shelf]?gs_shelf_ids[shelf][at]:-1):
    focus>=2200&&focus<2206?gs_page*3+focus-2200:-1;
  if(old>=0&&old<gs_n){
    snprintf(selected,sizeof selected,"%s",gs_games[old].id);
    snprintf(sport,sizeof sport,"%s",gs_games[old].modalidade);
    snprintf(league,sizeof league,"%s",gs_games[old].ligaId);
  }
  gs_n=gs_count=0;
  if(!strcmp(screen,"esportes-favoritos")){
    for(int i=0;i<esportes_favoritos_n();i++)gs_add(esportes_favoritos_evento(i));
    /* Saved teams appear immediately from known games while their feed loads. */
    for(int i=0;i<esportes_eventos_n();i++)gs_add(esportes_evento(i));
    for(int i=0;i<esportes_catalogo_n();i++)
      for(int j=0;j<esportes_catalogo_eventos_n(i);j++)gs_add(esportes_catalogo_evento(i,j));
    for(int i=0;i<esportes_jogos_seguidos_n();i++)gs_add(esportes_jogo_seguido(i));
  }else if(!strcmp(screen,"esportes-liga")){
    for(int i=0;i<esportes_eventos_n();i++)gs_add(esportes_evento(i));
  }else{
    for(int i=0;i<esportes_catalogo_n();i++){
      const EspLiga *l=esportes_catalogo_liga(i);int m=esportes_catalogo_modalidade(i);
      int config=!strcmp(screen,"esportes-visibilidade");
      if(!l||(!config&&!esportes_habilitado(m))||(!config&&gs_sport>=0&&gs_sport!=m))continue;
      if((esportes_liga_habilitada(m,l->id)||!strcmp(screen,"esportes-visibilidade"))&&(gs_contains(l->nome)||gs_contains(esportes_modalidade(m)->nome)))gs_indices[gs_count++]=i;
      for(int e=0;e<esportes_catalogo_eventos_n(i);e++)gs_add(esportes_catalogo_evento(i,e));
    }
    for(int i=0;i<esportes_favoritos_n();i++)gs_add(esportes_favoritos_evento(i));
    if(!strcmp(screen,"esportes-ligas"))for(int i=0;i<esportes_jogos_seguidos_n();i++)gs_add(esportes_jogo_seguido(i));
  }
  qsort(gs_games,(size_t)gs_n,sizeof *gs_games,gs_order);
  memset(gs_shelf_n,0,sizeof gs_shelf_n);
  for(int i=0;i<gs_n;i++){
    const EspEvento *e=&gs_games[i];
    if(e->estado==1)gs_shelf_ids[0][gs_shelf_n[0]++]=i;
    if(esportes_segue_evento(e))gs_shelf_ids[1][gs_shelf_n[1]++]=i;
    if(e->estado==0&&e->inicio>time(NULL))gs_shelf_ids[2][gs_shelf_n[2]++]=i;
    if(e->estado==2)gs_shelf_ids[3][gs_shelf_n[3]++]=i;
  }
  gs_shelf_n[4]=gs_count;gs_shelf_n[5]=esportes_noticias_n();
  if(selected[0])for(int i=0;i<gs_n;i++)if(!strcmp(selected,gs_games[i].id)&&!strcmp(sport,gs_games[i].modalidade)&&!strcmp(league,gs_games[i].ligaId)){
    if(shelf>=0){
      for(int j=0;j<gs_shelf_n[shelf];j++)if(gs_shelf_ids[shelf][j]==i){focus=3000+shelf*400+j;break;}
    }else{
      if(i<gs_page*3||i>=gs_page*3+6)gs_page=i/3;
      focus=2200+i-gs_page*3;
    }
    break;
  }
  if(shelf>=0){
    int n=gs_shelf_n[shelf],at=(focus-3000)%400;
    if(at>=n)at=n?n-1:0;
    focus=3000+shelf*400+at;
    if(gs_shelf_offset[shelf]>at)gs_shelf_offset[shelf]=at;
    if(at>=gs_shelf_offset[shelf]+3)gs_shelf_offset[shelf]=at-2;
  }
  gs_revision=esportes_revisao();gs_collection_sport=gs_sport;gs_collection_minute=time(NULL)/60;
  snprintf(gs_collection_page,sizeof gs_collection_page,"%s",screen);
  snprintf(gs_collection_query,sizeof gs_collection_query,"%s",query);
}
static void gs_prepare(void){
  if(gs_revision!=esportes_revisao()||strcmp(gs_collection_page,screen)||
     strcmp(gs_collection_query,query)||gs_collection_sport!=gs_sport||gs_collection_minute!=time(NULL)/60)gs_collect();
}
/* Input follows logical cards, never an interpolated focus rectangle. */
static int gs_live(int nth,const char **id,const char **name,const char **group);
#include "guiav5_evento.inc"
static int gs_arrow(SDL_Keycode k,int repeat){
  Uint32 now=SDL_GetTicks();
  if(!repeat||k!=gs_key){gs_key=k;gs_repeat_at=now+(repeat?120:300);}
  else{if((Sint32)(now-gs_repeat_at)<0)return 1;gs_repeat_at=now+120;}
  if(gs_team_arrow(k))return 1;
  if(!strcmp(screen,"esportes-evento")){
    int at=-1,pick=-1;float best=1e9f;
    for(int i=0;i<num_focus;i++)if(next_focus[i]==focus){at=i;break;}
    if(at>=0){
      GfxRect a=focus_rect[at];float ax=a.x+a.w*.5f,ay=a.y+a.h*.5f;
      for(int i=0;i<num_focus;i++){
        GfxRect b=focus_rect[i];float dx=b.x+b.w*.5f-ax,dy=b.y+b.h*.5f-ay;
        float main=k==SDLK_RIGHT?dx:k==SDLK_LEFT?-dx:k==SDLK_DOWN?dy:-dy;
        float side=k==SDLK_LEFT||k==SDLK_RIGHT?fabsf(dy):fabsf(dx);
        if(main>1&&main+3*side<best){best=main+3*side;pick=i;}
      }
      if(pick>=0)focus=next_focus[pick];
    }
    return 1;
  }
  if(!strcmp(screen,"esportes-ligas")&&focus>=3000&&focus<5400){
    int s=(focus-3000)/400,at=(focus-3000)%400,n=gs_shelf_n[s];
    if(k==SDLK_LEFT){if(at>0)at--;else{gv_side_enter();return 1;}}
    else if(k==SDLK_RIGHT){if(at+1<n)at++;}
    else if(k==SDLK_UP){if(s==0){focus=2071;return 1;}s--;at=gs_shelf_offset[s]+at-gs_shelf_offset[s+1];}
    else if(k==SDLK_DOWN){if(s==5)return 1;s++;at=gs_shelf_offset[s]+at-gs_shelf_offset[s-1];}
    if(at<0)at=0;if(at>=gs_shelf_n[s])at=gs_shelf_n[s]?gs_shelf_n[s]-1:0;
    focus=3000+s*400+at;
    if(at<gs_shelf_offset[s])gs_shelf_offset[s]=at;
    if(at>=gs_shelf_offset[s]+3)gs_shelf_offset[s]=at-2;
    return 1;
  }
  if(focus>=1100&&focus<=1105&&k==SDLK_DOWN){gv_zone=0;nav_spatial(k);return 1;}
  if(gv_overlay()){
    int first=!strcmp(screen,"esportes-ajustes")?2030:2040;
    int n=first==2030?5:gs_selected.nParticipantes+1;
    if(focus>=first&&focus<first+n){
      if(k==SDLK_UP&&focus>first)focus--;
      if(k==SDLK_DOWN&&focus<first+n-1)focus++;
    }
    return 1;
  }
  if(focus>=2200&&focus<2206){
    gs_prepare();int at=focus-2200,i=gs_page*3+at,next=i;
    if(k==SDLK_LEFT){if(at%3==0){gv_side_enter();return 1;}next--;}
    else if(k==SDLK_RIGHT){if(at%3==2)return 1;next++;}
    else if(k==SDLK_UP){if(i<3){nav_spatial(k);return 1;}next-=3;}
    else if(k==SDLK_DOWN)next+=3;
    if(next>=0&&next<gs_n){
      if(next<gs_page*3)gs_page--;
      if(next>=gs_page*3+6)gs_page++;
      focus=2200+next-gs_page*3;
    }
    return 1;
  }
  int catalog=focus>=2500&&focus<2506,news=focus>=2600&&focus<2606;
  int live=focus>=2700&&focus<2706;
  int sports=focus>=2400&&focus<2416;
  if(catalog||news||sports||live){
    int ids[16],n=0;
    if(sports){for(int i=0;i<esportes_modalidades_n();i++)
      if((!strcmp(screen,"esportes-modalidades-config")||esportes_habilitado(i))&&gs_contains(i18n(esportes_modalidade(i)->nome)))ids[n++]=i;
    }else n=catalog?gs_count:live?gs_live(-1,NULL,NULL,NULL):esportes_noticias_n();
    int base=catalog?2500:news?2600:live?2700:2400;
    int at=gs_page*6+focus-base;
    if(sports){at=-1;for(int i=0;i<n;i++)if(ids[i]==focus-base)at=i;}
    if(k==SDLK_LEFT&&at%3==0){gv_side_enter();return 1;}
    if(k==SDLK_RIGHT&&at%3==2)return 1;
    int next=at+(k==SDLK_LEFT?-1:k==SDLK_RIGHT?1:k==SDLK_UP?-3:3);
    if(k==SDLK_UP&&at<3){nav_spatial(k);return 1;}
    if(next>=0&&next<n){gs_page=next/6;focus=base+(sports?ids[next]:next%6);}
    return 1;
  }
  if(focus==2076){nav_spatial(k);return 1;}
  /* Keep the section grid and the optional live channels row reachable. */
  if(focus>=2070&&focus<2076){
    int i=focus-2070,j=i;
    if(k==SDLK_LEFT&&i==0){gv_side_enter();return 1;}
    if(k==SDLK_LEFT&&i>0)j--;
    else if(k==SDLK_RIGHT&&i<5)j++;
    else if(k==SDLK_DOWN){int at=i/2;if(at>=gs_shelf_n[0])at=gs_shelf_n[0]?gs_shelf_n[0]-1:0;focus=3000+at;return 1;}
    else if(k==SDLK_UP){nav_spatial(k);return 1;}
    focus=2070+j;return 1;
  }
  if(focus>=2000&&k==SDLK_UP){if(nav_spatial(k))return 1;focus=1102;return 1;}
  if(focus>=2000&&(k==SDLK_LEFT||k==SDLK_RIGHT||k==SDLK_DOWN)){
    nav_spatial(k);return 1;
  }
  return 0;
}
static void gs_tick(void){
  if(!gv_sports()){
    if(gs_started)gs_reset();
    return;
  }
  gs_start();
  if(!strcmp(screen,"esportes-evento"))gs_watch_step();
  if(player_aberto()&&!player_mini_no_guia_ativo())return;
  esportes_pedir_favoritos(0);
  esportes_passo();esportesui_fontes_passo();
  if(!strcmp(screen,"esportes-liga")&&gs_league>=0){
    if(SDL_GetTicks()-gs_refresh>=60000&&!esportes_carregando()){
      const EspLiga *l=esportes_catalogo_liga(gs_league);
      if(l)esportes_pedir_eventos(gs_sport,l,1);gs_refresh=SDL_GetTicks();
    }
  }else if(!strcmp(screen,"esportes-ligas")||!strcmp(screen,"esportes-hoje")||!strcmp(screen,"esportes-proximos")||!strcmp(screen,"esportes-resultados")){
    /* Same eight payload budget as #619. Browsing remaining leagues is on demand. */
    esportes_catalogo_visiveis();int n=0;
    for(int i=0;i<esportes_catalogo_n()&&n<8;i++){
      const EspLiga *l=esportes_catalogo_liga(i);int m=esportes_catalogo_modalidade(i);
      if(l&&gs_visible(m,l->id)&&(gs_sport<0||gs_sport==m)){esportes_catalogo_pedir(i);n++;}
    }
  }

}
static void gv_sports_header(const char *title,const char *sub,const char *unused){
  (void)unused;
  gv_label(TXT_ILHA_TITULO,title,GV_MX,214,920,0);
  if(sub&&*sub)gv_label(TXT_ILHA_CORPO,sub,GV_MX,272,920,1);
  gv_button(2001,i18n("Meus times"),"aj_star",GV_MX+GV_MW-410,190,210,0,1);
  gv_button(2002,i18n("Ajustes"),"aj_sliders-horizontal",GV_MX+GV_MW-188,190,188,0,1);
}
/* A feed's empty state must not depend on unrelated background requests. */
static int gs_empty_state(const char *page){
  if(!strcmp(page,"esportes-canais"))return 0;
  if(!strcmp(page,"esportes-noticias"))return esportes_noticias_estado()==0?1:esportes_noticias_estado()<0?-1:0;
  if(!strcmp(page,"esportes-favoritos"))return esportes_favoritos_carregando()?1:esportes_favoritos_erro()?-1:0;
  if(!strcmp(page,"esportes-liga"))return esportes_carregando()?1:esportes_erro()?-1:0;
  if(esportes_catalogo_carregando()||esportes_carregando())return 1;
  if(esportes_erro())return -1;
  for(int i=0;i<gs_count;i++)if(esportes_catalogo_estado(gs_indices[i])<0)return -1;
  return 0;
}
static void gs_retry(void){
  if(!strcmp(screen,"esportes-liga")&&gs_league>=0)esportes_pedir_eventos(gs_sport,esportes_catalogo_liga(gs_league),1);
  else{esportes_catalogo_iniciar(gs_sport,1);esportes_pedir_favoritos(1);esportes_noticias_pedir(1);}
}
static void gs_empty_draw(float y,int state,int action){
  if(state>0){
    float alpha=.06f+.025f*sinf(SDL_GetTicks()*.003f);
    for(int j=0;j<3;j++){
      float x=GV_MX+j*GS_CARD_STEP;plrui_cartao((GfxRect){x,y,GS_CARD_W,290},22,1);
      gfx_cor((GfxRect){x+24,y+24,210,14},.5f,1,1,1,alpha);
      for(int p=0;p<2;p++){
        gfx_cor((GfxRect){x+24,y+64+p*76,60,60},.5f,1,1,1,alpha);
        gfx_cor((GfxRect){x+104,y+86+p*76,240,18},.5f,1,1,1,alpha);
      }
      gfx_cor((GfxRect){x+24,y+244,170,14},.5f,1,1,1,alpha);
    }
    return;
  }
  int failed=state<0;
  GfxRect r={GV_MX,y,GV_MW,230};plrui_cartao(r,28,1);
  gfx_icone((GfxRect){r.x+32,y+40,48,48},failed?"aj_wifi-off":"aj_calendar",.7f,.72f,.76f,1);
  gv_label(TXT_ILHA_NOME,i18n(failed?"Não foi possível atualizar os eventos":"Nenhum resultado"),r.x+108,y+42,r.w-150,0);
  gv_label(TXT_ILHA_SUB,i18n(failed?"Não foi possível carregar. Use Atualizar para tentar novamente.":"Seu provedor define o conteúdo disponível."),r.x+108,y+90,r.w-150,1);
  if(failed)gv_button(action,i18n("Atualizar"),"aj_rotate-cw",r.x+108,y+144,210,0,0);
}
static void gs_empty(float y){gs_empty_draw(y,gs_empty_state(screen),2034);}
typedef struct {GfxMini image;char title[300],sub[200],icon[80];float scale,textScale,focus;int glass,focused;unsigned missing;Uint32 retry;} GSCard;
static GSCard gs_cards[6];static unsigned gs_card_next;
static void gs_cards_clear(void){for(int i=0;i<6;i++){gfx_mini_liberar(&gs_cards[i].image);gs_cards[i].title[0]=0;}}
static void gs_card_content(const char *title,const char *sub,const char *icon,float x,float y){
  if(icon&&*icon)gfx_icone((GfxRect){x+24,y+24,48,48},icon,1,1,1,.85f);
  txt_bloco_bruto(TXT_ILHA_NOME,title,243,242,239,x+24,y+100,GS_CARD_W-48,1.12f,1,2);
  gv_label(TXT_ILHA_SUB,sub,x+24,y+186,GS_CARD_W-48,1);
}
static void gs_card(int id,const char *title,const char *sub,const char *icon,float x,float y){
  GfxRect r={x,y,GS_CARD_W,232};GSCard *c=NULL;int focused=gv_btn_foc(id);
  for(int i=0;i<6;i++)if(!strcmp(gs_cards[i].title,title)&&!strcmp(gs_cards[i].sub,sub)&&!strcmp(gs_cards[i].icon,icon)){c=&gs_cards[i];break;}
  if(!c){c=&gs_cards[gs_card_next++%6];c->focus=0;}
  c->focus=anim_mola(c->focus,focused?1:0,gv_dt,T21_FOCO);
  float k=1+.08f*c->focus;
  GfxRect raised={r.x-(k-1)*r.w*.5f,r.y-(k-1)*r.h*.5f,r.w*k,r.h*k};
  plrui_cartao(raised,24,1);
  if(c->focus>.002f)plrui_linha_foco(raised,24,c->focus);
  float scale=gfx_escala();
  int dirty=!c->image.tex||strcmp(c->title,title)||strcmp(c->sub,sub)||strcmp(c->icon,icon)||
    c->image.w!=(int)ceilf(r.w*scale)||c->scale!=scale||c->textScale!=gfx_escala_txt()||c->glass!=ajustes_vidro()||c->focused!=focused||(c->missing&&SDL_GetTicks()-c->retry>=250);
  if(!gfx_mini_ativo()&&gfx_mini_alvo(&c->image,(int)ceilf(r.w*scale),(int)ceilf(r.h*scale))){
    if(dirty){
      unsigned missing=tex_n_falta;int pending=txt_pendentes;float opacity=gfx_opacidade_grupo;gfx_opacidade_grupo=1;
      gfx_mini_comecar(&c->image,0,0,scale);gs_card_content(title,sub,icon,0,0);
      gfx_mini_terminar();gfx_opacidade_grupo=opacity;
      snprintf(c->title,sizeof c->title,"%s",title);snprintf(c->sub,sizeof c->sub,"%s",sub);snprintf(c->icon,sizeof c->icon,"%s",icon);
      c->scale=scale;c->textScale=gfx_escala_txt();c->glass=ajustes_vidro();c->focused=focused;c->missing=tex_n_falta!=missing||txt_pendentes!=pending;c->retry=SDL_GetTicks();
    }
    gfx_mini_desenhar(&c->image,raised,24,1);
  }else gs_card_content(title,sub,icon,x,y);
  ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);gv_registra(id,r.x,r.y,r.w,r.h);
}
static void gs_pager(int n,int per){
  int max=n?(n-1)/per:0;if(gs_page>max)gs_page=max;
}
/* Read the published live guide, including category and parental filters.
 * ESPN and EPG are optional metadata, never prerequisites for watching. */
static int gs_live(int nth,const char **id,const char **name,const char **group){
  static int indices[G5_CAP],n;static unsigned revision;static Uint32 checked;
  static uint64_t session;static char filter[140];
  Uint32 now=SDL_GetTicks();unsigned current=guia_busca_revisao();
  if(!checked||revision!=current||now-checked>=2000||session!=perfis_sessao_geracao()||strcmp(filter,query)){
    n=0;const char *cid,*title,*cat,*base;
    for(int i=0;guia_canal_regra_campos(i,NULL,NULL,NULL,NULL,NULL)&&n<G5_CAP;i++){
      if(!guia_canal_campos(i,&cid,&title,NULL,&cat,&base)||
         !guia_canal_esporte(title,cat)||!g5ui_channel_allowed(cid,title,cat,base)||
         (!gs_contains(title)&&!gs_contains(cat)))continue;
      indices[n++]=i;
    }
    revision=current;checked=now?now:1;session=perfis_sessao_geracao();snprintf(filter,sizeof filter,"%s",query);
  }
  if(nth>=0&&nth<n)guia_canal_campos(indices[nth],id,name,NULL,group,NULL);
  return n;
}
#include "guiav5_esportes_seguir.inc"
#include "guiav5_esportes_hub.inc"

static void gs_hold(int id,int unused){(void)unused;focus=id;int i=gs_page*3+id-2200;if(i>=0&&i<gs_n){gs_selected=gs_games[i];gs_open("esportes-onde-assistir");}}
static void gv_sports_screen(void){
  if(!strcmp(screen,"esportes-evento")){gs_event_draw();return;}
  if(!gv_overlay())gs_prepare();
  int per=(!strcmp(screen,"esportes-liga")||!strcmp(screen,"esportes-favoritos")||!strcmp(screen,"esportes-hoje")||!strcmp(screen,"esportes-proximos")||!strcmp(screen,"esportes-resultados"))?3:6;
  int n=per==3?gs_n:!strcmp(screen,"esportes-noticias")?esportes_noticias_n():gs_count;
  if(per==3&&gs_page*per>=n)gs_page=n?(n-1)/per:0;
  if(!strcmp(screen,"esportes-ajustes")){
    gv_dim(0,.30f);float x=gv_sheet_base(480,180,1120,790);
    gv_label(TXT_TITULO2,i18n("Ajustes de esportes"),x,260,900,0);
    gv_opt(2030,"aj_eye",i18n("Esportes neste perfil"),i18n("Ligar ou desligar"),x,340,1056,80);
    gv_opt(2031,"aj_rows-3",i18n("Ligas visíveis"),i18n("Ligar ou desligar"),x,432,1056,80);
    int q=esportes_fundo_qualidade();
    gv_opt(2032,"aj_image",i18n("Qualidade da imagem"),i18n(q==1?"Baixa":q==2?"Alta":"Automático"),x,524,1056,80);
    gv_opt(2033,"aj_star",i18n("Times favoritos"),i18n("Escolher times"),x,616,1056,80);
    gv_opt(2034,"aj_arrow-up-down",i18n("Atualizar"),NULL,x,708,1056,80);
    gv_label(TXT_ILHA_SUB,i18n("Fundos ilustrativos do esporte. ESPN fornece apenas informações; os canais vêm das suas contas IPTV."),x,838,1056,1);return;
  }
  if(!strcmp(screen,"esportes-onde-assistir")){
    gv_dim(0,.42f);float x=gv_sheet_base(520,180,1080,700);
    gv_label(TXT_TITULO2,i18n("Favoritos"),x,270,960,0);
    gv_label(TXT_ILHA_SUB,gs_selected.nome,x,330,960,1);
    for(int p=0;p<gs_selected.nParticipantes;p++)gv_opt(2040+p,"aj_star",gs_selected.participantes[p].nome,i18n(esportes_favorito(&gs_selected,p)?"Remover dos favoritos":"Adicionar aos favoritos"),x,392+p*100,1016,86);
    gv_opt(2040+gs_selected.nParticipantes,"pl_bell",gs_selected.nome,i18n(esportes_segue_jogo(&gs_selected)?"Seguindo":"Seguir"),x,392+gs_selected.nParticipantes*100,1016,86);
    return;
  }
  const char *title=i18n("Esportes"),*sub="";
  for(int i=0;i<6;i++)if(!strcmp(screen,gs_pages[i]))title=i18n(gs_sections[i]);
  if(!strcmp(screen,"esportes-liga")){const EspLiga *l=esportes_catalogo_liga(gs_league);if(l)title=l->nome;sub=gs_sport>=0?i18n(esportes_modalidade(gs_sport)->nome):i18n("Ligas");}
  if(!strcmp(screen,"esportes-catalogo")){title=i18n("Ligas");sub=gs_sport>=0?i18n(esportes_modalidade(gs_sport)->nome):i18n("Todos");}
  if(!strcmp(screen,"esportes-visibilidade"))title=i18n("Ligas visíveis");
  if(!strcmp(screen,"esportes-modalidades-config"))title=i18n("Esportes neste perfil");
  if(!strcmp(screen,"esportes-canais"))title=i18n("Canais ao vivo");
  if(!strcmp(screen,"esportes-times"))title=i18n("Escolher times");
  gv_sports_header(title,sub,NULL);
  int live=(!strcmp(screen,"esportes-ligas")||!strcmp(screen,"esportes-canais"))?gs_live(-1,NULL,NULL,NULL):0;
  if(!strcmp(screen,"esportes-ligas")){
    if(live)gv_button(2076,i18n("Canais ao vivo"),"aj_activity",GV_MX+780,190,250,0,1);
    gs_hub();return;
  }
  gv_button(2020,i18n("Voltar"),"pl_chevron-left",GV_MX,314,180,0,1);
  gv_button(2021,i18n("Buscar esportes"),"aj_search",GV_MX+196,314,260,0,1);
  if(!strcmp(screen,"esportes-canais")){
    if(gs_page*6>=live)gs_page=0;
    for(int j=0;j<6&&gs_page*6+j<live;j++){
      const char *id=NULL,*name=NULL,*group=NULL;
      gs_live(gs_page*6+j,&id,&name,&group);
      gs_card(2700+j,name,group,"aj_activity",GV_MX+(j%3)*GS_CARD_STEP,392+(j/3)*264);
    }
    if(!live)gs_empty(410);
    gs_pager(live,6);return;
  }
  if(!strcmp(screen,"esportes-favoritos")||!strcmp(screen,"esportes-liga"))gv_button(2022,i18n("Escolher times"),"aj_plus",GV_MX+472,314,300,0,1);
  if(!strcmp(screen,"esportes-times")){gs_team_draw();return;}
  if(!strcmp(screen,"esportes-favoritos")&&!gs_n){
    if(esportes_favoritos_carregando()||esportes_favoritos_erro()){gs_empty(412);return;}
    float x=gv_sheet_base(GV_MX,412,GV_MW,300);
    gfx_icone((GfxRect){x,450,48,48},"aj_star",.8f,.8f,.8f,1);
    gv_label(TXT_ILHA_NOME,i18n(esportes_favoritos_times()?"Nenhum jogo ao vivo ou próximo":"Escolha seus times favoritos"),x+76,448,1100,0);
    gv_label(TXT_ILHA_SUB,i18n("Escolha seus times favoritos"),x+76,500,1200,1);
    return;
  }
  if(!strcmp(screen,"esportes-artigo")){
    const EspNoticia *a=esportes_noticia(gs_article);if(!a)return;
    gv_label(TXT_ILHA_SUB,a->origem[0]?a->origem:"ESPN",GV_MX,400,GV_MW,1);
    txt_bloco_bruto(TXT_ILHA_NOME,a->titulo,243,242,239,GV_MX,448,GV_MW,1.3f,1,4);
    txt_bloco_bruto(TXT_ILHA_CORPO,a->resumo,200,202,204,GV_MX,610,GV_MW,1.4f,1,8);return;
  }
  if(!strcmp(screen,"esportes-modalidades")||!strcmp(screen,"esportes-modalidades-config")){
    int config=!strcmp(screen,"esportes-modalidades-config"),ids[16],n=0;
    for(int m=0;m<esportes_modalidades_n();m++)if((config||esportes_habilitado(m))&&gs_contains(i18n(esportes_modalidade(m)->nome)))ids[n++]=m;
    for(int j=0;j<6&&gs_page*6+j<n;j++){
      int m=ids[gs_page*6+j];const EspModalidade *s=esportes_modalidade(m);
      char icon[80];snprintf(icon,sizeof icon,"esp_%s",s->id);
      gs_card(2400+m,i18n(s->nome),i18n(config?(esportes_habilitado(m)?"Ligado":"Desligado"):"Ligas"),icon,GV_MX+(j%3)*GS_CARD_STEP,392+(j/3)*264);
    }
    gs_pager(n,6);return;
  }
  if(!strcmp(screen,"esportes-catalogo")||!strcmp(screen,"esportes-visibilidade")){
    int config=!strcmp(screen,"esportes-visibilidade");
    for(int j=0;j<6&&gs_page*6+j<gs_count;j++){
      int k=gs_indices[gs_page*6+j],m=esportes_catalogo_modalidade(k);const EspLiga *l=esportes_catalogo_liga(k);
      float x=GV_MX+(j%3)*GS_CARD_STEP,y=392+(j/3)*264;
      gs_card(2500+j,l->nome,i18n(config?(esportes_liga_habilitada(m,l->id)?"Ligado":"Desligado"):esportes_modalidade(m)->nome),"",x,y);
      esportesui_fundo((GfxRect){x+300,y+16,140,76},m,0);
      esportesui_logo(l->logo,m,(GfxRect){x+24,y+18,68,68});
    }
    if(!gs_count)gs_empty(410);gs_pager(gs_count,6);return;
  }
  if(!strcmp(screen,"esportes-noticias")){
    int n=esportes_noticias_n();
    for(int j=0;j<6&&gs_page*6+j<n;j++){
      const EspNoticia *a=esportes_noticia(gs_page*6+j);
      gs_card(2600+j,a->titulo,a->origem[0]?a->origem:"ESPN","aj_scroll-text",GV_MX+(j%3)*GS_CARD_STEP,392+(j/3)*264);
    }
    if(!n)gs_empty(410);
    gs_pager(n,6);return;
  }
  if(strcmp(gs_grid_page,screen)){snprintf(gs_grid_page,sizeof gs_grid_page,"%s",screen);gs_grid_row=gs_page;gs_grid_v=0;}
  gs_grid_row=anim_mola2(&gs_grid_v,gs_grid_row,gs_page,gv_dt,T21_SCROLL);
  int first=(int)floorf(fmaxf(0,gs_grid_row))*3;
  gfx_recorte(GV_MX-24,388,GV_MW+48,636);
  for(int i=first;i<gs_n&&i<first+9;i++){
    int j=i-gs_page*3,id=2200+j;
    GfxRect r={GV_MX+(i%3)*GS_CARD_STEP,410+(i/3-gs_grid_row)*312,GS_CARD_W,282};
    esportesui_cartao(&gs_games[i],r,j>=0&&j<6&&gv_btn_foc(id),24+i%8);
    gs_follow_badge(&gs_games[i],r);
    if(j>=0&&j<6){
      ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);ponteiro_alvo_segurar(gs_hold);
      gv_registra(id,GV_MX+(i%3)*GS_CARD_STEP,410+(j/3)*312,GS_CARD_W,282);
    }
  }
  gfx_sem_recorte();
  if(!gs_n)gs_empty(420);
  if(esportes_erro()&&gs_n)gv_label(TXT_ILHA_SUB,i18n("Não foi possível atualizar os eventos"),GV_MX,970,GV_MW,1);
  gs_pager(gs_n,3);
}
static int gs_activate(void){
  if(gs_hub_activate())return 1;
  if(focus==2034&&strcmp(screen,"esportes-ajustes")){
    gs_retry();return 1;
  }
  if(!strcmp(screen,"esportes-evento")){
    if(focus==2020)gs_back();
    else if(focus>=2080&&focus<2080+gs_watch_n)gv_assistir(gs_watch[focus-2080].id);
    else if(focus==2088)gs_remind();
    else if(focus==2089&&gs_follow_action)gs_follow_action(&gs_selected);
    return 1;
  }
  if(focus>=2700&&focus<2706){const char *id=NULL;gs_live(gs_page*6+focus-2700,&id,NULL,NULL);if(id)gv_assistir(id);return 1;}
  if(focus==2076){gs_open("esportes-canais");return 1;}
  if(focus==2000){gs_depth=gs_page=0;gs_sport=-1;query[0]=0;esportes_catalogo_iniciar(-1,0);gv_go("esportes-ligas");return 1;}
  if(focus==2001){gs_open("esportes-favoritos");esportes_pedir_favoritos(0);return 1;}
  if(focus==2002){gs_open("esportes-ajustes");return 1;}
  if(focus==2020){gs_back();return 1;}
  if(focus==2021){input(5,"Buscar esportes",query,0);return 1;}
  if(focus==2022){gs_choosing=1;if(!strcmp(screen,"esportes-liga")&&gs_league>=0){const EspLiga *l=esportes_catalogo_liga(gs_league);gs_open("esportes-times");esportes_pedir_times(gs_sport,l);}else{gs_sport=-1;gs_open("esportes-modalidades");}return 1;}
  if(focus>=2070&&focus<2076){gs_choosing=0;int k=focus-2070;gs_open(gs_pages[k]);if(k==5)esportes_noticias_pedir(0);return 1;}
  if(!strcmp(screen,"esportes-ajustes")){
    if(focus==2030)gs_open("esportes-modalidades-config");
    else if(focus==2031)gs_open("esportes-visibilidade");
    else if(focus==2032&&!esportes_fundo_definir_qualidade((esportes_fundo_qualidade()+1)%3))glem_aviso_curto(i18n("Não foi possível salvar"));
    else if(focus==2033)gs_open("esportes-favoritos");
    else if(focus==2034){esportes_catalogo_iniciar(gs_sport,1);esportes_pedir_favoritos(1);esportes_noticias_pedir(1);gs_back();}
    return 1;
  }
  if(!strcmp(screen,"esportes-onde-assistir")){
    if(focus>=2040&&focus<2040+gs_selected.nParticipantes){
      if(!esportes_alternar_favorito(&gs_selected,focus-2040))glem_aviso_curto(i18n("Não foi possível salvar"));
      gs_revision=~0u;
    }else if(focus==2040+gs_selected.nParticipantes){
      gs_follow_toggle(&gs_selected);
    }
    esportes_seguir_lembretes();return 1;
  }
  if(focus>=2400&&focus<2416){
    int m=focus-2400;
    if(!strcmp(screen,"esportes-modalidades-config")){
      if(!esportes_habilitar(m,!esportes_habilitado(m)))glem_aviso_curto(i18n("Não foi possível salvar"));
      gs_revision=~0u;
    }else{gs_open("esportes-catalogo");gs_sport=m;esportes_catalogo_iniciar(m,0);}
    return 1;
  }
  if(focus>=2500&&focus<2506){
    int pos=gs_page*6+focus-2500;if(pos>=gs_count)return 1;
    int k=gs_indices[pos],m=esportes_catalogo_modalidade(k);const EspLiga *l=esportes_catalogo_liga(k);if(!l)return 1;
    if(!strcmp(screen,"esportes-visibilidade")){
      if(!esportes_liga_habilitar(m,l->id,!esportes_liga_habilitada(m,l->id)))glem_aviso_curto(i18n("Não foi possível salvar"));
    }else{gs_open(gs_choosing?"esportes-times":"esportes-liga");gs_league=k;gs_sport=m;esportes_catalogo_suspender();if(gs_choosing)esportes_pedir_times(m,l);else esportes_pedir_eventos(m,l,0);gs_refresh=SDL_GetTicks();}
    return 1;
  }
  if(focus>=2800&&focus<2806&&!strcmp(screen,"esportes-times")){
    gs_team_collect();int at=gs_page*6+focus-2800;
    if(at<gs_team_n){EspEvento e=gs_team_event(gs_team_ids[at]);if(!esportes_seguir_time(&e,0,!esportes_segue_time(&e,0)))glem_aviso_curto(i18n("Não foi possível salvar"));}
    return 1;
  }
  if(focus>=2600&&focus<2606){gs_article=gs_page*6+focus-2600;gs_open("esportes-artigo");return 1;}
  if(focus>=2200&&focus<2206){int i=gs_page*3+focus-2200;if(i<gs_n)gs_event_open(&gs_games[i]);return 1;}
  return 1;
}
