#define SPORTS_V5_MAIN previous_sports_main
#include "esportes_v5_shot.c"
int main(int argc,char **argv){
  const char *out=argc>1?argv[1]:"/Volumes/ExternalSSD/tmp/rev/espseguir";
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Follow Sports proof",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(w&&SDL_GL_CreateContext(w));
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");ajustes_dir(getenv("NUVIO_DADOS"));ajustes_definir_vidro(0);context();semear();fixture();
  g5ui_open("esportes-ligas");quadro();focus=2070;
  assert(!strcmp(gs_pages[0],"esportes-favoritos"));capture(w,out,"my-teams-row");
  gv_event(SDLK_RETURN,0);quadro();assert(!strcmp(screen,"esportes-favoritos")&&gs_n>0);
  focus=2022;gv_event(SDLK_RETURN,0);quadro();assert(!strcmp(screen,"esportes-modalidades"));
  focus=2400;gv_event(SDLK_RETURN,0);quadro();assert(!strcmp(screen,"esportes-catalogo"));
  focus=2500;gv_event(SDLK_RETURN,0);quadro();assert(!strcmp(screen,"esportes-times"));
  esportes_cancelar(); /* Offline roster fixture, same production published type. */
  times=calloc(8,sizeof *times);nTimes=8;
  const char *names[]={"Flamengo","Palmeiras","Botafogo","Cruzeiro","Santos","São Paulo","Vasco da Gama","Grêmio"};
  for(int i=0;i<8;i++){
    snprintf(times[i].id,sizeof times[i].id,"team%d",i);snprintf(times[i].nome,sizeof times[i].nome,"%s",names[i]);
    if(i<2)strcpy(times[i].logo,eventos[0].participantes[i].logo);
  }
  strcpy(times[0].id,"fla");gs_team_collect();focus=2800;gv_event(SDLK_RETURN,0);assert(!esportes_favorito(&eventos[0],0));
  gv_event(SDLK_RETURN,0);assert(esportes_favorito(&eventos[0],0));
  for(int i=1;i<8;i++){gv_event(SDLK_DOWN,0);assert(gs_page*6+focus-2800==i);}
  gs_page=0;focus=2800;capture(w,out,"team-picker");
  ajustes_definir_vidro(1);capture(w,out,"team-picker-glass");ajustes_definir_vidro(0);
  gs_choosing=0;g5ui_open("esportes-liga");gs_league=0;gs_page=0;quadro();focus=2200;
  capture(w,out,"followed-game");
  gv_ok_segurado();quadro();focus=2040+gs_selected.nParticipantes;gv_event(SDLK_RETURN,0);
  assert(esportes_segue_jogo(&gs_selected));capture(w,out,"follow-game-action");
  gv_event(SDLK_RETURN,0);assert(!esportes_segue_jogo(&gs_selected));
  gv_event(SDLK_ESCAPE,0);quadro();focus=2202;gv_event(SDLK_RETURN,0);gv_tecla_solta(SDLK_RETURN);quadro();
  assert(!strcmp(screen,"esportes-evento")&&focus==2088&&gs_follow_action&&gs_follow_state);
  assert(!gs_follow_state(&gs_selected));
  gv_event(SDLK_RIGHT,0);assert(focus==2089);gv_event(SDLK_RETURN,0);quadro();
  assert(esportes_segue_jogo(&gs_selected)&&gs_follow_state(&gs_selected));
  int reminded=0;
  for(int i=0;i<lembrete_n();i++)if(!strncmp(lembrete_item(i)->base,"sports|",7)&&lembrete_item(i)->ini==gs_selected.inicio)reminded=1;
  assert(reminded);capture(w,out,"event-following");
  gv_event(SDLK_RETURN,0);assert(!esportes_segue_jogo(&gs_selected));
  gv_event(SDLK_ESCAPE,0);quadro();assert(!strcmp(screen,"esportes-liga")&&focus==2202);
  setenv("ESPEVENTO_PROOF","after",1);event_proof(w,out);
  puts("PASS native team picker, paging, My teams filter, highlight, event follow toggle, and auto reminders");return 0;
}
