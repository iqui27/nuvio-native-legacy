/* Offline end-to-end guide v5 Sports proof, using production state/renderers. */
#define main legacy_guide_shots
#include "guiav5_principal_shot.c"
#undef main
#define hash esp_fixture_hash
#define perfil esp_fixture_profile
#define arquivo esp_fixture_file
#define erro esp_fixture_error
#define revisao esp_fixture_revision
#define ocupado esp_fixture_busy
#define pronto esp_fixture_ready
#define fila esp_fixture_queue
#define resultado esp_fixture_result
#define buscar esp_fixture_worker
#define ordem esp_fixture_order
#include "../src/esportes.c"
#undef hash
#undef perfil
#undef arquivo
#undef erro
#undef revisao
#undef ocupado
#undef pronto
#undef fila
#undef resultado
#undef buscar
#undef ordem

static void fixture(void){
  esportes_abrir_sessao();gs_started=1;esportesui_recursos("deploy/app/art");
  catalogo=calloc(9,sizeof *catalogo);nCatalogo=9;catalogoIdioma=ajustes_idioma();catalogoFeitos=0xffff;
  for(int i=0;i<9;i++){
    catalogo[i].modalidade=0;
    snprintf(catalogo[i].liga.id,sizeof catalogo[i].liga.id,"league%d",i);
    snprintf(catalogo[i].liga.nome,sizeof catalogo[i].liga.nome,"%s %d",i?"Competition":"Brasileirão",i+1);
  }
  eventos=calloc(8,sizeof *eventos);nEventos=8;eventosModalidade=0;eventosIdioma=ajustes_idioma();strcpy(eventosLiga,"league0");
  for(int i=0;i<8;i++){
    EspEvento *e=&eventos[i];snprintf(e->id,sizeof e->id,"event%d",i);
    strcpy(e->modalidade,"soccer");strcpy(e->ligaId,"league0");strcpy(e->liga,"Brasileirão");
    strcpy(e->nome,"Flamengo × Palmeiras");strcpy(e->local,"Maracanã");
    e->nParticipantes=2;e->estado=i==0?1:i==1?2:0;e->inicio=time(NULL)+(i==0?-20:i==1?-180:120*i)*60;e->fim=e->inicio+7200;
    strcpy(e->participantes[0].id,"fla");strcpy(e->participantes[0].nome,"Flamengo");strcpy(e->participantes[0].curto,"Flamengo");strcpy(e->participantes[0].pontos,"2");
    strcpy(e->participantes[1].id,"pal");strcpy(e->participantes[1].nome,"Palmeiras");strcpy(e->participantes[1].curto,"Palmeiras");strcpy(e->participantes[1].pontos,"1");
    strcpy(e->participantes[0].logo,"tests/fixtures/guiacat-team-0.png");
    strcpy(e->participantes[1].logo,"tests/fixtures/guiacat-team-1.png");
  }
  catalogoUso=1;
  rowCache[0]=(EspRowCache){.linha=0,.n=8,.estado=1,.dia=8,.uso=1,.desde=time(NULL),.eventos=calloc(8,sizeof *eventos)};
  memcpy(rowCache[0].eventos,eventos,8*sizeof *eventos);
  for(int i=1;i<8;i++)rowCache[i].linha=-1;
  eventosFav=calloc(2,sizeof *eventosFav);nEventosFav=2;eventosFav[0]=eventos[0];eventosFav[1]=eventos[2];
  strcpy(eventosFav[1].liga,"Copa do Brasil");strcpy(eventosFav[1].ligaId,"cup");
  strcpy(favoritos[0],"soccer|league0|fla");nFavoritos=1;favoritosCompleto=1;favoritosDesde=time(NULL);
  nNoticias=1;noticiasEstado=1;strcpy(noticias[0].titulo,"Championship weekend: fixtures and team news");strcpy(noticias[0].resumo,"Offline sample of the ESPN metadata news feed.");strcpy(noticias[0].origem,"ESPN");
  gs_sport=-1;gs_league=0;gs_page=gs_depth=0;
  nLtvF=1;strcpy(ltvF[0].nome,"Fixture IPTV");strcpy(ltvF[0].chave,"fixture");ltvF[0].tipo=LTV_M3U;
  for(int i=0;i<nCanais;i++)canais[i].base="";
  fontesOk=cacheLido=1;perfilPrefs=perfis_ativo();estado=G_PRONTO;
  epg_so_provedores(1); /* Fixture XML only; Sources must not download public EPG. */
}
static void sportsux_proof(SDL_Window *w,const char *out){
  const char *phase=getenv("SPORTSUX_PROOF");if(!phase)return;
  char name[100];
  g5ui_open("esportes-favoritos");quadro();
  int count=nEventosFav,teams=nFavoritos;nEventosFav=nFavoritos=0;esp_fixture_revision++;gs_collect();
  snprintf(name,sizeof name,"%s-empty",phase);capture(w,out,name);
  nEventosFav=count;nFavoritos=teams;esp_fixture_revision++;gs_collect();
  g5ui_open("esportes-ligas");quadro();focus=2070;
  snprintf(name,sizeof name,"%s-home",phase);capture(w,out,name);
  /* Provider channels add a row without replacing any Sports section. Walk
   * every section using D-pad/OK, then return to the same home focus. */
  assert(gs_live(-1,NULL,NULL,NULL)>0);
  const SDL_Keycode steps[]={SDLK_RIGHT,SDLK_RIGHT,SDLK_DOWN,SDLK_LEFT,SDLK_LEFT};
  const int sections[]={0,1,2,5,4,3};
  for(int i=0;i<6;i++){
    if(i)gv_event(steps[i-1],0);
    assert(focus==2070+sections[i]);
    int visible=0;for(int j=0;j<num_focus;j++)if(next_focus[j]==focus)visible=1;
    assert(visible);
    gv_event(SDLK_RETURN,0);quadro();assert(!strcmp(screen,gs_pages[sections[i]]));
    gv_event(SDLK_ESCAPE,0);quadro();assert(!strcmp(screen,"esportes-ligas")&&focus==2070+sections[i]);
  }
  gv_event(SDLK_DOWN,0);assert(focus==2076);
  gv_event(SDLK_RETURN,0);quadro();assert(!strcmp(screen,"esportes-canais"));
  focus=2021;gv_event(SDLK_DOWN,0);assert(focus>=2700&&focus<2706);
  int first=focus;gv_event(SDLK_RIGHT,0);assert(focus==first+1);quadro();
  snprintf(name,sizeof name,"%s-live-channels",phase);capture(w,out,name);
  gv_event(SDLK_ESCAPE,0);quadro();assert(!strcmp(screen,"esportes-ligas")&&focus==2076);
  gv_event(SDLK_UP,0);assert(focus==2073);
  /* Measure the same draw interval and counters as main.c, excluding swap. */
  for(int i=0;i<8;i++){
    rowCache[i].linha=i;rowCache[i].n=50;rowCache[i].estado=1;
    rowCache[i].eventos=realloc(rowCache[i].eventos,50*sizeof *eventos);
    assert(rowCache[i].eventos);
    for(int j=0;j<50;j++){
      rowCache[i].eventos[j]=eventos[2];
      snprintf(rowCache[i].eventos[j].id,100,"stress-%d-%d",i,j);
    }
  }
  esp_fixture_revision++;
  double sum=0,worst=0;Uint64 frequency=SDL_GetPerformanceFrequency();
  for(int i=0;i<120;i++){
    txt_ms=0;txt_rasterizadas=0;
    txt_novo_quadro();tex_novo_quadro();tex_bombear(8);gfx_novo_quadro();
    Uint64 start=SDL_GetPerformanceCounter();g5ui_draw();
    double ms=(SDL_GetPerformanceCounter()-start)*1000.0/frequency;
    if(i>=30){sum+=ms;if(ms>worst)worst=ms;}
    SDL_GL_SwapWindow(w);
  }
  assert(gs_n==ESP_MAX_EVENTOS);
  printf("[quadro] sportsux=%s samples=90 events=%d des=%.3f worst=%.3fms\n",phase,gs_n,sum/90,worst);
  printf("[quadro-des] rects=%d(p%d,b%d) out=%.3fms/%d fill=%.2fx cheias=%d texto=%.3fms/%d\n",gfx_n_rect,gfx_n_prog,gfx_n_bind,gfx_ms_outros,gfx_n_outros,gfx_fill,gfx_n_cheio,txt_ms,txt_rasterizadas);
  fflush(stdout);
  for(int i=0;i<8;i++){rowCache[i].n=0;rowCache[i].linha=-1;}
  rowCache[0].linha=0;rowCache[0].n=8;memcpy(rowCache[0].eventos,eventos,8*sizeof *eventos);
  esp_fixture_revision++;gs_collect();
  /* One press moves one card, including a page boundary, with no draw between
   * queued events. Up/Down follow rows rather than jumping to the top tab. */
  focus=1102;gv_return_focus=2505;gv_zone=5;gv_event(SDLK_DOWN,0);assert(focus==2001);
  focus=2070;gv_event(SDLK_DOWN,0);assert(focus==2073);
  gv_event(SDLK_UP,0);assert(focus==2070);
  gv_event(SDLK_LEFT,0);assert(gv_side_open);gv_event(SDLK_RIGHT,0);assert(!gv_side_open&&focus==2070);
  gv_event(SDLK_LEFT,0);gv_event(SDLK_ESCAPE,0);assert(!gv_side_open&&!strcmp(screen,"esportes-ligas"));
  gv_event(SDLK_UP,0);assert(focus==1102&&gv_zone==5);gv_event(SDLK_DOWN,0);assert(focus==2001);
  g5ui_open("esportes-visibilidade");quadro();focus=2500;
  for(int i=1;i<9;i++){gv_event(SDLK_RIGHT,0);assert(gs_page*6+focus-2500==i);quadro();}
  gs_page=0;focus=2500;gv_event(SDLK_DOWN,0);assert(focus==2503);gv_event(SDLK_UP,0);assert(focus==2500);
  g5ui_open("esportes-liga");gs_league=0;gs_page=0;quadro();focus=2200;
  for(int i=1;i<=7;i++){gv_event(SDLK_RIGHT,0);assert(gs_page*3+focus-2200==i);quadro();}
  for(int i=6;i>=0;i--){gv_event(SDLK_LEFT,0);assert(gs_page*3+focus-2200==i);quadro();}
  gv_event(SDLK_RIGHT,0);int at=gs_page*3+focus-2200;
  for(int i=0;i<20;i++)gv_event(SDLK_RIGHT,1);
  assert(gs_page*3+focus-2200==at);
  SDL_Delay(310);gv_event(SDLK_RIGHT,1);assert(gs_page*3+focus-2200==at+1);
  quadro();int visible=0;for(int i=0;i<num_focus;i++)if(next_focus[i]==focus){visible=1;assert(focus_rect[i].x>=0&&focus_rect[i].x+focus_rect[i].w<=1920);}
  assert(visible);gv_event(SDLK_DOWN,0);gv_event(SDLK_UP,0);assert(focus>=2200&&focus<2203);
  /* Exercise production dispatch, not gs_activate directly. */
  g5ui_open("esportes-ligas");quadro();
  /* Carrying an old guide zone must not redirect a short OK on Sports. */
  gv_zone=4;focus=2002;gv_tecla_solta(SDLK_RETURN);gv_event(SDLK_RETURN,0);quadro();
  assert(!strcmp(screen,"esportes-ajustes")&&focus==2030);
  snprintf(name,sizeof name,"%s-settings",phase);capture(w,out,name);
  gv_event(SDLK_RETURN,0);quadro();assert(!strcmp(screen,"esportes-modalidades-config"));
  focus=2401;gv_event(SDLK_RETURN,0);assert(!esportes_habilitado(1));
  gv_event(SDLK_RETURN,0);assert(esportes_habilitado(1));
  gv_event(SDLK_ESCAPE,0);quadro();assert(focus==2030);
  gv_event(SDLK_DOWN,0);assert(focus==2031);gv_event(SDLK_RETURN,0);quadro();
  assert(!strcmp(screen,"esportes-visibilidade"));
  int m=esportes_catalogo_modalidade(gs_indices[0]);const EspLiga *l=esportes_catalogo_liga(gs_indices[0]);
  char league[80];snprintf(league,sizeof league,"%s",l->id);
  focus=2500;gv_event(SDLK_RETURN,0);assert(!esportes_liga_habilitada(m,league));
  gv_event(SDLK_RETURN,0);assert(esportes_liga_habilitada(m,league));
  gv_event(SDLK_ESCAPE,0);quadro();assert(!strcmp(screen,"esportes-ajustes")&&focus==2031);
  focus=2032;gv_event(SDLK_RETURN,0);assert(esportes_fundo_qualidade()==1);
  gv_event(SDLK_DOWN,0);assert(focus==2033);gv_event(SDLK_RETURN,0);quadro();assert(!strcmp(screen,"esportes-favoritos"));
  gv_event(SDLK_ESCAPE,0);quadro();assert(!strcmp(screen,"esportes-ajustes")&&focus==2033);
  gv_event(SDLK_DOWN,0);assert(focus==2034);gv_event(SDLK_UP,0);assert(focus==2033);
  gv_event(SDLK_ESCAPE,0);quadro();assert(!strcmp(screen,"esportes-ligas")&&focus==2002);
  point_activate(2002,0);quadro();assert(!strcmp(screen,"esportes-ajustes"));
  gv_event(SDLK_ESCAPE,0);quadro();
  /* A held OK opens team choices; actual OK saves, survives a session reopen,
   * and immediately includes known live/upcoming games in Favorites. */
  nFavoritos=0;nEventosFav=0;
  g5ui_open("esportes-liga");gs_page=0;quadro();focus=2200;
  gv_event(SDLK_RETURN,0);gv_ok_desde=SDL_GetTicks()-2000;gv_tick();quadro();
  assert(!strcmp(screen,"esportes-onde-assistir")&&focus==2040);
  gv_tecla_solta(SDLK_RETURN);gv_event(SDLK_RETURN,0);assert(esportes_favorito(&eventos[0],0));
  gv_event(SDLK_ESCAPE,0);quadro();assert(focus==2200);
  focus=2001;gv_event(SDLK_RETURN,0);quadro();assert(gs_n>=1);
  EspEvento saved=eventos[0];
  int original=perfis_ativo();perfis_definir_ativo(original+1);esportes_abrir_sessao();assert(!esportes_favorito(&saved,0));
  perfis_definir_ativo(original);esportes_abrir_sessao();assert(esportes_favorito(&saved,0)&&esportes_fundo_qualidade()==1);
  esportes_cancelar(); /* Keep fixture proof offline. */
  eventos=calloc(1,sizeof *eventos);*eventos=saved;nEventos=1;
  g5ui_open("esportes-liga");gs_page=0;quadro();focus=2001;gv_event(SDLK_RETURN,0);quadro();assert(gs_n==1);
  snprintf(name,sizeof name,"%s-favorites",phase);capture(w,out,name);
  focus=2200;gv_ok_segurado();quadro();focus=2040;gv_event(SDLK_RETURN,0);
  assert(!esportes_favorito(&saved,0));gv_event(SDLK_ESCAPE,0);quadro();assert(gs_n==0);
  puts("PASS Sports UX: exact card steps, paced repeat, rows, visible focus, Settings dispatch, profile save/reopen, favorite/unfavorite");
}
#ifndef SPORTS_V5_MAIN
#define SPORTS_V5_MAIN main
#endif
int SPORTS_V5_MAIN(int argc,char **argv){
  const char *out=argc>1?argv[1]:"/Volumes/ExternalSSD/tmp/rev/esportes619";mkdir(out,0755);
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Sports v5 proof",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(w&&SDL_GL_CreateContext(w));
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");ajustes_dir(getenv("NUVIO_DADOS"));ajustes_definir_vidro(0);context();semear();fixture();
  if(getenv("SPORTSUX_PROOF")){sportsux_proof(w,out);return 0;}
  g5ui_open("esportes-ligas");quadro();assert(gs_count==9&&gs_n==9);capture(w,out,"sports-home");
  g5ui_open("esportes-hoje");quadro();assert(gs_n>0);capture(w,out,"sports-games-crests");
  g5ui_open("esportes-ligas");quadro();
  focus=2074;gs_activate();assert(!strcmp(screen,"esportes-modalidades"));quadro();focus=2400;gs_activate();quadro();assert(!strcmp(screen,"esportes-catalogo")&&gs_count==9);
  focus=2061;gs_activate();quadro();assert(gs_page==1);focus=2500;gs_activate();assert(gs_league==6&&!strcmp(screen,"esportes-liga"));gs_back();quadro();assert(gs_page==1&&focus==2500);
  gs_page=0;focus=2500;gs_activate();quadro();capture(w,out,"league");
  focus=2200;gv_ok_segurado();quadro();assert(!strcmp(screen,"esportes-onde-assistir")&&gs_selected.nParticipantes==2);gs_back();quadro();assert(focus==2200);
  focus=2200;gs_activate();assert(stream_folha_canais_aberta());
  for(int i=0;i<80;i++)esportesui_fontes_passo();
  const FontesCanais *sources=fontescanais_estado();assert(sources->n==1&&!strcmp(sources->canais[0].nome,"SporTV"));
  capture(w,out,"match-sources");stream_folha_canais_fechar();
  /* Hidden source excludes the otherwise confirmed channel. */
  nLtvF=0;esportesui_fontes(&eventos[0]);for(int i=0;i<80;i++)esportesui_fontes_passo();assert(!fontescanais_estado()->n);stream_folha_canais_fechar();nLtvF=1;
  g5ui_open("esportes-favoritos");gs_page=0;quadro();assert(gs_n==8);capture(w,out,"favorites");
  assert(esportes_liga_habilitar(0,"cup",0));gs_collect();assert(gs_n==8);
  assert(esportes_liga_habilitar(0,"cup",1));
  focus=2201;time_t saved=eventosFav[1].inicio;eventosFav[1].inicio=eventosFav[0].inicio-60;
  eventosFav[0].estado=0;gs_collect();assert(!strcmp(gs_games[gs_page*3+focus-2200].id,"event2"));
  eventosFav[0].estado=1;eventosFav[1].inicio=saved;gs_collect();
  g5ui_open("esportes-noticias");quadro();focus=2600;gs_activate();quadro();assert(!strcmp(screen,"esportes-artigo"));gs_back();assert(!strcmp(screen,"esportes-noticias"));
  g5ui_open("esportes-ligas");focus=2002;gs_activate();quadro();focus=2032;gs_activate();assert(esportes_fundo_qualidade()==1);gs_back();assert(!strcmp(screen,"esportes-ligas"));
  focus=2002;gs_activate();quadro();focus=2030;gs_activate();quadro();
  assert(!strcmp(gv_pagina,"esportes-modalidades-config"));gs_back();quadro();
  assert(!strcmp(screen,"esportes-ajustes")&&!strcmp(gv_pagina,"esportes-ligas"));
  focus=2031;gs_activate();quadro();assert(gs_count==9);gs_back();quadro();
  assert(!strcmp(screen,"esportes-ajustes")&&!strcmp(gv_pagina,"esportes-ligas"));gs_back();
  /* Read the captured public ESPN payload through the production parser. */
  FILE *payload=fopen("tests/fixtures/guiacat-scoreboard.json","rb");assert(payload);
  char json[12000];size_t bytes=fread(json,1,sizeof json-1,payload);fclose(payload);json[bytes]=0;
  EspEvento parsed[2];EspLiga brazil={0};strcpy(brazil.id,"bra.1");strcpy(brazil.nome,"Brasileirão");
  assert(esportes_eventos_ler(json,"soccer",&brazil,parsed,2)==1);
  assert(parsed[0].nParticipantes==2);
  for(int p=0;p<2;p++)assert(strstr(parsed[0].participantes[p].logo,"https://a.espncdn.com/i/teamlogos/"));
  /* 2000 published channels, 93 source groups and a missing group. */
  g5ui_open("grade");free(canais);free(cats);
  canais=calloc(2000,sizeof *canais);cats=calloc(93,sizeof *cats);assert(canais&&cats);
  static char ids[2000][32],names[93][64];
  nCanais=2000;nCats=93;
  for(int i=0;i<93;i++){snprintf(names[i],64,"Provider group %02d",i+1);cats[i].nome=names[i];}
  for(int i=0;i<2000;i++){
    snprintf(ids[i],32,"fixture-%d",i);
    canais[i]=(GCanal){.id=ids[i],.nome=ids[i],.cat=i%93,.base="",.logo="",.tipo="channel",.desc="",.epg=-1};
  }
  canais[1999].cat=-1;gv_grupo[0]=0;gv_off=gv_lin=0;gv_snap_force=1;gv_snapshot();
  assert(gv_total_n==2000&&gv_cats==94&&gv_cats_total==94);
  int sum=0;for(int i=0;i<gv_cats;i++)sum+=gv_cat_n[i];assert(sum==2000);
  gv_zone=3;gv_sb=6+93;gv_cat_scroll();assert(gv_cat_off==94-GV_CAT_WIN);
  assert(gv_sb_rect(gv_sb,GV_BASE).y+44<1000);
  capture(w,out,"categories-2000-counts");
  gv_filtrar("Outros","grade");assert(gv_match==1);
  g5ui_close();assert(!gs_started&&!gs_depth);
  puts("PASS Sports v5: sections, sport/league paging, Back, team options, combined favorites, confirmed Sources and hidden source exclusion");return 0;
}
