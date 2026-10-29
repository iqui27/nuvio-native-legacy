/* PR #619 supplies metadata, artwork, profile preferences and Sources matching.
 * This adapter keeps guide v5 geometry, focus, motion and Back navigation. */
static int gs_choosing;
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
static void gs_reset(void){
  gs_choosing=0;
  if(gs_started)esportesui_fechar();
  gs_started=gs_depth=gs_page=gs_n=gs_count=0;gs_sport=gs_league=-1;
  gs_revision=~0u;gs_collection_page[0]=0;gs_key=0;
}
static const char *gs_sections[]={"Meus times","Ao vivo / Jogos de hoje","Próximos jogos","Resultados","Por liga","Notícias"};
static const char *gs_icons[]={"aj_star","aj_activity","aj_calendar","aj_trophy","aj_rows-3","aj_scroll-text"};
static const char *gs_pages[]={"esportes-favoritos","esportes-hoje","esportes-proximos","esportes-resultados","esportes-modalidades","esportes-noticias"};
static int gs_team_arrow(SDL_Keycode k);
static void gs_start(void){
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
  if((x->estado==1)!=(y->estado==1))return x->estado==1?-1:1;
  if(x->inicio!=y->inicio)return x->inicio<y->inicio?-1:1;
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
  int old=gs_page*3+focus-2200;
  if(focus>=2200&&focus<2203&&old<gs_n){
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
  }
  qsort(gs_games,(size_t)gs_n,sizeof *gs_games,gs_order);
  if(selected[0])for(int i=0;i<gs_n;i++)if(!strcmp(selected,gs_games[i].id)&&!strcmp(sport,gs_games[i].modalidade)&&!strcmp(league,gs_games[i].ligaId)){
    gs_page=i/3;focus=2200+i%3;break;
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
static int gs_arrow(SDL_Keycode k,int repeat){
  Uint32 now=SDL_GetTicks();
  if(!repeat||k!=gs_key){gs_key=k;gs_repeat_at=now+(repeat?120:300);}
  else{if((Sint32)(now-gs_repeat_at)<0)return 1;gs_repeat_at=now+120;}
  if(gs_team_arrow(k))return 1;
  if(focus>=1100&&focus<=1105&&k==SDLK_DOWN){gv_zone=0;focus=2001;return 1;}
  if(gv_overlay()){
    int first=!strcmp(screen,"esportes-ajustes")?2030:2040;
    int n=first==2030?5:gs_selected.nParticipantes+1;
    if(focus>=first&&focus<first+n){
      if(k==SDLK_UP&&focus>first)focus--;
      if(k==SDLK_DOWN&&focus<first+n-1)focus++;
    }
    return 1;
  }
  if(focus>=2200&&focus<2203&&(k==SDLK_LEFT||k==SDLK_RIGHT)){
    gs_prepare();int i=gs_page*3+focus-2200+(k==SDLK_RIGHT?1:-1);
    if(i>=0&&i<gs_n){gs_page=i/3;focus=2200+i%3;}
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
    int next=at+(k==SDLK_LEFT?-1:k==SDLK_RIGHT?1:k==SDLK_UP?-3:3);
    if(k==SDLK_UP&&at%6<3){focus=2021;return 1;}
    if(next>=0&&next<n){gs_page=next/6;focus=base+(sports?ids[next]:next%6);}
    return 1;
  }
  if(focus==2076){
    if(k==SDLK_UP)focus=2073;
    else if(k==SDLK_LEFT)gv_side_enter();
    return 1;
  }
  /* Keep the section grid and the optional live channels row reachable. */
  if(focus>=2070&&focus<2076){
    int i=focus-2070,j=i;
    if(k==SDLK_LEFT&&i%3==0){gv_side_enter();return 1;}
    if(k==SDLK_LEFT&&i%3)j--;
    else if(k==SDLK_RIGHT&&i%3<2)j++;
    else if(k==SDLK_DOWN&&i<3)j+=3;
    else if(k==SDLK_DOWN&&i>=3){nav_spatial(k);return 1;}
    else if(k==SDLK_UP&&i>=3)j-=3;
    else if(k==SDLK_UP){gv_zone=5;focus=1102;return 1;}
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
static void gs_empty(float y){
  int loading=esportes_carregando()||esportes_catalogo_carregando()||esportes_favoritos_carregando();
  gv_label(TXT_ILHA_NOME,i18n(loading?"Carregando…":esportes_erro()||esportes_favoritos_erro()?"Não foi possível carregar. Use Atualizar para tentar novamente.":"Nenhum resultado"),GV_MX,y,GV_MW,1);
}
static void gs_card(int id,const char *title,const char *sub,const char *icon,float x,float y){
  GfxRect r={x,y,466,232};plrui_cartao(r,24,1);
  if(gv_btn_foc(id))plrui_linha_foco(gv_foco_folha(r),24,1);
  gfx_icone((GfxRect){x+24,y+24,48,48},icon,1,1,1,.85f);
  gv_label(TXT_ILHA_NOME,title,x+24,y+100,418,0);
  gv_label(TXT_ILHA_SUB,sub,x+24,y+148,418,1);
  ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);gv_registra(id,r.x,r.y,r.w,r.h);
}
static void gs_pager(int n,int per){
  int max=n?(n-1)/per:0;if(gs_page>max)gs_page=max;
  if(!max)return;
  if(gs_page>0)gv_button(2060,i18n("Anterior"),"pl_chevron-left",GV_MX,954,190,0,1);
  if((gs_page+1)*per<n)gv_button(2061,i18n("Próximo"),"pl_chevron-right",GV_MX+204,954,190,0,1);
  char s[80];snprintf(s,sizeof s,"%d / %d",gs_page+1,n?(n+per-1)/per:1);
  gv_label(TXT_ILHA_SUB,s,GV_MX+430,970,200,1);
}
/* Read the published live guide, including category and parental filters.
 * ESPN and EPG are optional metadata, never prerequisites for watching. */
static int gs_live(int nth,const char **id,const char **name,const char **group){
  int n=0;const char *cid,*title,*cat,*base;
  for(int i=0;guia_canal_regra_campos(i,NULL,NULL,NULL,NULL,NULL);i++){
    if(!guia_canal_campos(i,&cid,&title,NULL,&cat,&base)||
       !guia_canal_esporte(title,cat)||!g5ui_channel_allowed(cid,title,cat,base)||
       (!gs_contains(title)&&!gs_contains(cat)))continue;
    if(n++==nth){if(id)*id=cid;if(name)*name=title;if(group)*group=cat;}
  }
  return n;
}
#include "guiav5_esportes_seguir.inc"

static void gs_hold(int id,int unused){(void)unused;focus=id;int i=gs_page*3+id-2200;if(i>=0&&i<gs_n){gs_selected=gs_games[i];gs_open("esportes-onde-assistir");}}
static void gv_sports_screen(void){
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
  int live=gs_live(-1,NULL,NULL,NULL);
  if(!strcmp(screen,"esportes-ligas")){
    for(int i=0;i<6;i++){
      char count[96];int n=i==0?esportes_favoritos_times():i==4?gs_count:i==5?esportes_noticias_n():gs_n;
      if(i>=1&&i<=3){n=0;for(int e=0;e<gs_n;e++)if(i==1?(gs_games[e].estado==1||gs_today(gs_games[e].inicio)):i==2?(gs_games[e].estado==0&&gs_games[e].inicio>time(NULL)):gs_games[e].estado==2)n++;}
      snprintf(count,sizeof count,"%d · %s",n,i18n(i==4?"Ligas":i==0?"Times":"Itens carregados"));
      float x=GV_MX+(i%3)*486,y=340+(i/3)*264;
      gs_card(2070+i,i18n(gs_sections[i]),count,gs_icons[i],x,y);
      if(i>=1&&i<=3)for(int e=0;e<gs_n;e++){
        const EspEvento *game=&gs_games[e];
        if(!(i==1?(game->estado==1||gs_today(game->inicio)):i==2?(game->estado==0&&game->inicio>time(NULL)):game->estado==2))continue;
        int m=gs_sport_id(game->modalidade);
        for(int p=0;p<game->nParticipantes;p++)esportesui_logo(game->participantes[p].logo,m,(GfxRect){x+286+p*72,y+18,64,64});
        break;
      }
    }
    if(live)gv_button(2076,i18n("Canais ao vivo"),"aj_activity",GV_MX,884,GV_MW,0,1);
    else gs_empty(884);
    if(!live)gv_label(TXT_ILHA_SUB,i18n("Seu provedor define o conteúdo disponível."),GV_MX,928,GV_MW,1);
    return;
  }
  gv_button(2020,i18n("Voltar"),"pl_chevron-left",GV_MX,314,180,0,1);
  gv_button(2021,i18n("Buscar esportes"),"aj_search",GV_MX+196,314,260,0,1);
  if(!strcmp(screen,"esportes-canais")){
    if(gs_page*6>=live)gs_page=0;
    for(int j=0;j<6&&gs_page*6+j<live;j++){
      const char *id=NULL,*name=NULL,*group=NULL;
      gs_live(gs_page*6+j,&id,&name,&group);
      gs_card(2700+j,name,group,"aj_activity",GV_MX+(j%3)*486,392+(j/3)*264);
    }
    if(!live)gs_empty(410);
    gs_pager(live,6);return;
  }
  if(!strcmp(screen,"esportes-favoritos")||!strcmp(screen,"esportes-liga"))gv_button(2022,i18n("Escolher times"),"aj_plus",GV_MX+472,314,300,0,1);
  if(!strcmp(screen,"esportes-times")){gs_team_draw();return;}
  if(!strcmp(screen,"esportes-favoritos")&&!gs_n){
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
      gs_card(2400+m,i18n(s->nome),i18n(config?(esportes_habilitado(m)?"Ligado":"Desligado"):"Ligas"),icon,GV_MX+(j%3)*486,392+(j/3)*264);
    }
    gs_pager(n,6);return;
  }
  if(!strcmp(screen,"esportes-catalogo")||!strcmp(screen,"esportes-visibilidade")){
    int config=!strcmp(screen,"esportes-visibilidade");
    for(int j=0;j<6&&gs_page*6+j<gs_count;j++){
      int k=gs_indices[gs_page*6+j],m=esportes_catalogo_modalidade(k);const EspLiga *l=esportes_catalogo_liga(k);
      float x=GV_MX+(j%3)*486,y=392+(j/3)*264;
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
      gs_card(2600+j,a->titulo,a->origem[0]?a->origem:"ESPN","aj_scroll-text",GV_MX+(j%3)*486,392+(j/3)*264);
    }
    if(!n)gv_label(TXT_ILHA_NOME,i18n(esportes_noticias_estado()==0?"Carregando…":esportes_noticias_estado()<0?"Não foi possível carregar. Use Atualizar para tentar novamente.":"Nenhum resultado"),GV_MX,410,GV_MW,1);
    gs_pager(n,6);return;
  }
  for(int j=0;j<3&&gs_page*3+j<gs_n;j++){
    GfxRect r={GV_MX+j*486,412,466,314};int id=2200+j;
    esportesui_cartao(&gs_games[gs_page*3+j],r,gv_btn_foc(id));
    gs_follow_badge(&gs_games[gs_page*3+j],r);
    ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);ponteiro_alvo_segurar(gs_hold);gv_registra(id,r.x,r.y,r.w,r.h);
  }
  if(!gs_n)gs_empty(420);
  gv_label(TXT_ILHA_SUB,i18n("Segure OK para escolher os times favoritos. OK abre Fontes."),GV_MX,786,GV_MW,1);
  if(esportes_erro())gv_label(TXT_ILHA_SUB,i18n("Não foi possível atualizar os eventos"),GV_MX,842,GV_MW,1);
  else if(gs_n==ESP_MAX_EVENTOS)gv_label(TXT_ILHA_SUB,i18n("Mostrando os primeiros 400 eventos"),GV_MX,842,GV_MW,1);
  gs_pager(gs_n,3);
}
static int gs_activate(void){
  if(focus>=2700&&focus<2706){const char *id=NULL;gs_live(gs_page*6+focus-2700,&id,NULL,NULL);if(id)gv_assistir(id);return 1;}
  if(focus==2076){gs_open("esportes-canais");return 1;}
  if(focus==2000){gs_depth=gs_page=0;gs_sport=-1;query[0]=0;esportes_catalogo_iniciar(-1,0);gv_go("esportes-ligas");return 1;}
  if(focus==2001){gs_open("esportes-favoritos");esportes_pedir_favoritos(0);return 1;}
  if(focus==2002){gs_open("esportes-ajustes");return 1;}
  if(focus==2020){gs_back();return 1;}
  if(focus==2021){input(5,"Buscar esportes",query,0);return 1;}
  if(focus==2022){gs_choosing=1;if(!strcmp(screen,"esportes-liga")&&gs_league>=0){const EspLiga *l=esportes_catalogo_liga(gs_league);gs_open("esportes-times");esportes_pedir_times(gs_sport,l);}else{gs_sport=-1;gs_open("esportes-modalidades");}return 1;}
  if(focus==2060||focus==2061){gs_page+=focus==2060?-1:1;if(gs_page<0)gs_page=0;focus=2020;return 1;}
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
      if(!esportes_seguir_jogo(&gs_selected,!esportes_segue_jogo(&gs_selected)))glem_aviso_curto(i18n("Não foi possível salvar"));
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
  if(focus>=2200&&focus<2203){int i=gs_page*3+focus-2200;if(i<gs_n)esportesui_fontes(&gs_games[i]);return 1;}
  return 1;
}
