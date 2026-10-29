/* Offline production renderer and input proof, shared with the Sports fixture. */
static void event_proof(SDL_Window *w,const char *out){
  const char *phase=getenv("ESPEVENTO_PROOF");int before=!strcmp(phase,"before");
  char name[100];
  SDL_GL_SetSwapInterval(0);
  for(int i=0;i<8;i++){
    rowCache[i].linha=i;rowCache[i].n=50;rowCache[i].estado=1;
    rowCache[i].eventos=realloc(rowCache[i].eventos,50*sizeof *eventos);
    assert(rowCache[i].eventos);
    for(int j=0;j<50;j++){
      EspEvento *e=&rowCache[i].eventos[j];*e=eventos[j%8];
      snprintf(e->id,sizeof e->id,"stress-%d-%d",i,j);
      snprintf(e->nome,sizeof e->nome,"%s vs %s",j%2?"Manchester United":"Flamengo",j%2?"Wolverhampton Wanderers":"Palmeiras");
      snprintf(e->participantes[0].nome,120,"%s",j%2?"Manchester United":"Flamengo");
      snprintf(e->participantes[0].curto,100,"%s",e->participantes[0].nome);
      snprintf(e->participantes[1].nome,120,"%s",j%2?"Wolverhampton Wanderers":"Palmeiras");
      snprintf(e->participantes[1].curto,100,"%s",e->participantes[1].nome);
    }
  }
  esp_fixture_revision++;
  const char *pages[]={"esportes-ligas","esportes-hoje","esportes-resultados"};
  const char *shots[]={"sports","live-grid","results"};
  for(int p=0;p<3;p++){
    g5ui_open(pages[p]);quadro();focus=p?2200:2071;
    snprintf(name,sizeof name,"%s-%s",phase,shots[p]);capture(w,out,name);
    double update=0,draw=0,worst=0;int rects=0,texts=0,rasters=0,uploads=0;
    Uint64 hz=SDL_GetPerformanceFrequency();
    for(int i=0;i<150;i++){
      txt_novo_quadro();tex_novo_quadro();gfx_novo_quadro();
      txt_ms=0;txt_rasterizadas=0;tex_upl_n=0;tex_bombear(8);
      Uint64 a=SDL_GetPerformanceCounter();gv_tick();
      Uint64 b=SDL_GetPerformanceCounter();g5ui_draw();glFinish();
      double ms=(SDL_GetPerformanceCounter()-b)*1000.0/hz;
      if(i>=30){update+=(b-a)*1000.0/hz;draw+=ms;if(ms>worst)worst=ms;rects+=gfx_n_rect;texts+=gfx_n_text;rasters+=txt_rasterizadas;uploads+=tex_upl_n;}
      SDL_GL_SwapWindow(w);
    }
    printf("[espevento] %s %s events=%d frames=120 update=%.3fms draw=%.3fms worst=%.3fms rects=%.1f text_draws=%.1f text_uploads=%d art_uploads=%d\n",phase,shots[p],gs_n,update/120,draw/120,worst,rects/120.0,texts/120.0,rasters,uploads);
    fflush(stdout);
    if(!before){assert(draw/120<16);assert(rasters==0);}
  }
  g5ui_open("esportes-liga");gs_page=0;quadro();focus=2200;gv_event(SDLK_RETURN,0);gv_tecla_solta(SDLK_RETURN);quadro();
  snprintf(name,sizeof name,"%s-live-event",phase);capture(w,out,name);
  if(!before)assert(!strcmp(screen,"esportes-evento")&&!stream_folha_canais_aberta());
  if(before){stream_folha_canais_fechar();}
#ifdef ESPEVENTO_IMPLEMENTED
  else{
    assert(!strcmp(screen,"esportes-evento")&&!stream_folha_canais_aberta());
    assert(gs_watch_n>0&&!strcmp(gs_watch[0].id,"c1"));
    /* A carrying channel remains playable without matching EPG. */
    EspEvento selected=gs_selected;strcpy(gs_selected.nome,"Other matchup");
    strcpy(gs_selected.participantes[0].nome,"Other home");strcpy(gs_selected.participantes[0].curto,"Other home");
    strcpy(gs_selected.participantes[1].nome,"Other away");strcpy(gs_selected.participantes[1].curto,"Other away");
    gs_watch_n=gs_watch_cursor=0;gs_watch_step();
    assert(gs_watch_n==1&&!gs_watch[0].confirmed&&!strcmp(gs_watch[0].id,"c1"));gs_selected=selected;
    focus=2080;gv_event(SDLK_RETURN,0);CatItem played={0};assert(guia_pediu_preview(&played)&&!strcmp(played.imdb,"c1"));
    gs_back();quadro();assert(focus==2200);
  }
#endif
  EspEvento saved=eventos[0];eventos[0].nEmissoras=0;strcpy(eventos[0].nome,"Unknown opponents");
  strcpy(eventos[0].participantes[0].nome,"Unknown home");strcpy(eventos[0].participantes[0].curto,"Unknown home");
  strcpy(eventos[0].participantes[1].nome,"Unknown away");strcpy(eventos[0].participantes[1].curto,"Unknown away");
  strcpy(eventos[0].participantes[0].logo,"");strcpy(eventos[0].participantes[1].logo,"");
  esp_fixture_revision++;g5ui_open("esportes-liga");gs_page=0;quadro();focus=2200;gv_event(SDLK_RETURN,0);gv_tecla_solta(SDLK_RETURN);quadro();
  snprintf(name,sizeof name,"%s-live-no-channel",phase);capture(w,out,name);
  if(before)stream_folha_canais_fechar();
#ifdef ESPEVENTO_IMPLEMENTED
  else{assert(!strcmp(screen,"esportes-evento")&&!gs_watch_n&&!stream_folha_canais_aberta());gs_back();}
#endif
  eventos[0]=saved;
  g5ui_open("esportes-proximos");quadro();focus=2200;gv_event(SDLK_RETURN,0);gv_tecla_solta(SDLK_RETURN);quadro();
  snprintf(name,sizeof name,"%s-upcoming-event",phase);capture(w,out,name);
#ifdef ESPEVENTO_IMPLEMENTED
  if(!before){
    assert(!strcmp(screen,"esportes-evento")&&focus==2088);
    int n=lembrete_n();gv_event(SDLK_RETURN,0);assert(lembrete_n()==n+1);
    assert(lembrete_achar(gs_reminder_id,gs_selected.inicio,gs_selected.nome)>=0);
    lembrete_carregar(perfis_ativo());assert(lembrete_n()==n+1);
    gv_event(SDLK_RETURN,0);assert(lembrete_n()==n);
    gs_back();quadro();assert(!strcmp(screen,"esportes-proximos")&&focus==2200);
    puts("PASS event view: live channel playback, empty playback details, upcoming reminder toggle, Back focus");
  }
#endif
}
