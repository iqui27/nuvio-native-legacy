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
int main(int argc,char **argv){
  const char *out=argc>1?argv[1]:"/Volumes/ExternalSSD/tmp/rev/esportes619";mkdir(out,0755);
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Sports v5 proof",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(w&&SDL_GL_CreateContext(w));
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");ajustes_dir(getenv("NUVIO_DADOS"));ajustes_definir_vidro(0);context();semear();fixture();
  g5ui_open("esportes-ligas");quadro();assert(gs_count==9&&gs_n==9);capture(w,out,"sports-home");
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
  g5ui_open("esportes-favoritos");gs_page=0;quadro();assert(gs_n==2);capture(w,out,"favorites");
  assert(esportes_liga_habilitar(0,"cup",0));gs_collect();assert(gs_n==2);
  assert(esportes_liga_habilitar(0,"cup",1));
  focus=2201;time_t saved=eventosFav[1].inicio;eventosFav[1].inicio=eventosFav[0].inicio-60;
  eventosFav[0].estado=0;gs_collect();assert(!strcmp(gs_games[focus-2200].id,"event2"));
  eventosFav[0].estado=1;eventosFav[1].inicio=saved;gs_collect();
  g5ui_open("esportes-noticias");quadro();focus=2600;gs_activate();quadro();assert(!strcmp(screen,"esportes-artigo"));gs_back();assert(!strcmp(screen,"esportes-noticias"));
  g5ui_open("esportes-ligas");focus=2002;gs_activate();quadro();focus=2032;gs_activate();assert(esportes_fundo_qualidade()==1);gs_back();assert(!strcmp(screen,"esportes-ligas"));
  focus=2002;gs_activate();quadro();focus=2030;gs_activate();quadro();
  assert(!strcmp(gv_pagina,"esportes-modalidades-config"));gs_back();quadro();
  assert(!strcmp(screen,"esportes-ajustes")&&!strcmp(gv_pagina,"esportes-ligas"));
  focus=2031;gs_activate();quadro();assert(gs_count==9);gs_back();quadro();
  assert(!strcmp(screen,"esportes-ajustes")&&!strcmp(gv_pagina,"esportes-ligas"));gs_back();
  g5ui_close();assert(!gs_started&&!gs_depth);
  puts("PASS Sports v5: sections, sport/league paging, Back, team options, combined favorites, confirmed Sources and hidden source exclusion");return 0;
}
