#include "../src/guiav5ui.c"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <sys/stat.h>

static void capture(SDL_Window *window,const char *dir,const char *name){
  char path[1024];snprintf(path,sizeof path,"%s/%s-app.png",dir,name);
  for(int frame=0;frame<90;frame++){
    SDL_PumpEvents();txt_novo_quadro();tex_novo_quadro();tex_bombear(8);gfx_novo_quadro();
    glClearColor(.04,.04,.04,1);glClear(GL_COLOR_BUFFER_BIT);g5ui_draw();
    SDL_Delay(8);
    if(frame==89){
      unsigned char *pixels=malloc(1920*1080*4);assert(pixels);
      SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(surface);
      glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
      for(int y=0;y<1080;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
      assert(IMG_SavePNG(surface,path)==0);SDL_FreeSurface(surface);free(pixels);
    }SDL_GL_SwapWindow(window);
  }
}
int main(int argc,char **argv){
  const char *dir=argc>1?argv[1]:"/Volumes/ExternalSSD/tmp/rev/guiav5-png";
  mkdir(dir,0755);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *window=SDL_CreateWindow("Guide v5",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(window);
  assert(SDL_GL_CreateContext(window));SDL_GL_SetSwapInterval(0);glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  /* NUVIO_SHOT_ESCALA=85|100 and NUVIO_SHOT_VIDRO=0|1 pick the text scale and
   * material; default stays 100% glass. */
  {
    char aj[160];
    const char *esc = getenv("NUVIO_SHOT_ESCALA");
    snprintf(aj, sizeof aj, "idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal %d\nescalaTextoVersaoLocal 1\n",
             esc && atoi(esc) == 85 ? 0 : 2);
    dados_gravar("ajustes.txt", aj);
  }
  ajustes_dir(getenv("NUVIO_DADOS"));
  gfx_escala_txt_definir(ajustes_escala_texto());
  ajustes_definir_vidro(!getenv("NUVIO_SHOT_VIDRO") || atoi(getenv("NUVIO_SHOT_VIDRO")));
  assert(ajustes_idioma()==0);context();
  const char *names[]={"setup-tipo","setup-credenciais","setup-arquivo","setup-teste","setup-categorias","setup-epg","setup-importar","setup-pronto","setup-celular-tv","fontes-gerenciar","vod-filmes","vod-series","vod-serie-temporadas","vod-titulo","vod-busca","vod-carregando","vod-vazio","vod-sem-catalogo","parental-ajustes","parental-canal-bloqueado","parental-pin","parental-desbloqueio","parental-perfil-infantil","multiview-2","multiview-3","multiview-4","multiview-escolher","multiview-limite"};
  inventory=calloc(30,sizeof *inventory);assert(inventory);count=24;source_loaded=1;
  const char *titles[]={"Duna: Parte Dois","Interestelar","A Chegada","Blade Runner 2049","Oppenheimer","O Senhor dos Anéis","Fallout","Ruptura","The Last of Us","Silo","Dark","Fundação"};
  for(int i=0;i<count;i++){
    G5Item *it=&inventory[i];it->kind=i<6?G5_MOVIE:i<12?G5_SERIES:G5_LIVE;it->age=10;
    snprintf(it->name,sizeof it->name,"%s",titles[i%12]);snprintf(it->id,sizeof it->id,"%d",i+1);snprintf(it->source,sizeof it->source,"xtream");snprintf(it->group,sizeof it->group,"%s",i<6?"Cinema":i<12?"Séries":"Esportes");
    const char *art[]={"duna-poster.jpg","p00.jpg","p09.jpg","p12.jpg","p13.jpg","toy-story.jpg","last-of-us.jpg","severance.jpg","last-of-us.jpg","severance.jpg","p00.jpg","p09.jpg"};
    snprintf(it->poster,sizeof it->poster,"/Volumes/ExternalSSD/tmp/rev/guiatv-estado/v5/assets/%s",i<12?art[i]:"futebol.jpg");
  }build_groups();mv_count=4;for(int i=0;i<4;i++){mv_items[i]=inventory[12+i];snprintf(mv_items[i].name,sizeof mv_items[i].name,"%s",i==0?"SporTV":i==1?"ESPN":i==2?"Premiere":"SporTV 2");}
  selected=0;test_ok=1;tested.conexoes=1;tested.maxConexoes=2;
  snprintf(draft.name,sizeof draft.name,"IPTV da sala");
  for(int i=0;i<12;i++)snprintf(inventory[i].backdrop,sizeof inventory[i].backdrop,"/Volumes/ExternalSSD/tmp/rev/guiatv-estado/v5/assets/%s",i<6?"duna-backdrop.jpg":"fallout-backdrop.jpg");
  /* Real inline PIN controls: digits, clear, delete, cancellation. */
  want_pin(0);activate(20);activate(21);activate(30);assert(!strcmp(pin_value,"120"));
  activate(31);assert(!strcmp(pin_value,"12"));activate(29);assert(!pin_value[0]);
  for(int i=0;i<6;i++)activate(20+i);assert(strlen(pin_value)==4);
  activate(1);assert(!pin_value[0]&&!strcmp(screen,"grade"));
  go("setup-credenciais");credential_field=6;activate(100);assert(strchr(draft.name,'A'));draft.name[strlen(draft.name)-1]=0;
  const char *only=getenv("NUVIO_SHOT_SO");   /* iterate on one state */
  for(unsigned n=0;n<sizeof names/sizeof *names;n++){
    if(only&&*only&&!strstr(only,names[n]))continue;
    go(names[n]);draft.live=draft.movies=draft.series=draft.epg=1;focus=0;mode=strstr(names[n],"-4")?4:strstr(names[n],"-3")?3:2;
    if(!strcmp(names[n],"vod-serie-temporadas")){
      series_catalog=inventory;series_count=count;parent_series=6;inventory=calloc(4,sizeof *inventory);count=4;episodes_mode=1;selected=0;
      for(int i=0;i<4;i++){inventory[i].kind=G5_EPISODE;inventory[i].age=10;snprintf(inventory[i].name,sizeof inventory[i].name,"%d. %s",i+1,i==0?"O fim":i==1?"O alvo":i==2?"A cabeça":"Os necróticos");snprintf(inventory[i].poster,sizeof inventory[i].poster,"/Volumes/ExternalSSD/tmp/rev/guiatv-estado/v5/assets/fallout-backdrop.jpg");}
    }else selected=0;
    if(!strcmp(names[n],"parental-perfil-infantil")){rules.kids=1;}else rules.kids=0;
    if(!strcmp(names[n],"setup-celular-tv")){
      phone();char *html=rede_baixar(celular_url(),5);
      if(html){char path[1024];snprintf(path,sizeof path,"%s/phone.html",dir);FILE *f=fopen(path,"w");assert(f);fputs(html,f);fclose(f);free(html);}
    }
    if(!strcmp(names[n],"vod-vazio"))snprintf(query,sizeof query,"%s","no fixture matches");else query[0]=0;
    capture(window,dir,names[n]);
    if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;episodes_mode=0;}
  }
  puts("PASS: 28 native v5 states captured");return 0;
}
