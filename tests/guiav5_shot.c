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
  /* D-pad: paged lists reach every entry; modals keep the D-pad inside. */
  {SDL_Event e={0};e.type=SDL_KEYDOWN;
#define KEY(k) (e.key.keysym.sym=(k),g5ui_event(&e))
   int gc=group_count;group_count=12;for(int i=0;i<12;i++)snprintf(groups[i],sizeof groups[i],"Grupo %d",i);
   go("parental-lista");rules_mode=0;focus=7;KEY(SDLK_DOWN);assert(focus==8);KEY(SDLK_UP);assert(focus==7);
   go("setup-categorias");query[0]=0;focus=2+9;KEY(SDLK_DOWN);assert(focus==2+11);focus=2+8;KEY(SDLK_DOWN);assert(focus==2+10);
   group_count=gc;build_groups();
   int pc=picker_count;picker_count=10;go("multiview-escolher");assert(focus==-1);
   focus=906;KEY(SDLK_DOWN);assert(focus==907);focus=900;KEY(SDLK_UP);assert(focus==900&&nav==-1);
   picker_count=0;G5Item first=mv_items[0];focus=-1;KEY(SDLK_RETURN);assert(!strcmp(screen,"multiview-escolher")&&!strcmp(mv_items[0].id,first.id));
   picker_count=pc;
   want_pin(0);focus=3;KEY(SDLK_UP);assert(nav==-1);KEY(SDLK_TAB);assert(nav==-1);
   go("fontes-gerenciar");focus=0;KEY(SDLK_UP);assert(nav==0);KEY(SDLK_DOWN);assert(nav==-1);
   /* Back walks setup, sources and parental one level, focus on the opener. */
   go("grade");go("fontes-gerenciar");focus=0;go("setup-tipo");focus=1;go("setup-credenciais");focus=3;go("setup-teste");
   focus=0;go("setup-importar");focus=4;go("setup-categorias");
   e.type=SDL_KEYDOWN;
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"setup-importar")&&focus==4);
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"setup-teste")&&focus==0);
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"setup-credenciais")&&focus==3);
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"setup-tipo")&&focus==1);
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"fontes-gerenciar")&&focus==0);
   go("parental-ajustes");focus=2;activate(2);assert(!strcmp(screen,"parental-lista"));
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"parental-ajustes")&&focus==2);
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"fontes-gerenciar"));
   focus=4;want_pin(1);go("parental-ajustes");KEY(SDLK_ESCAPE);assert(!strcmp(screen,"fontes-gerenciar")&&focus==4);
   /* On-screen Back buttons pop the same history as the remote. */
   go("grade");go("setup-tipo");focus=2;activate(2);assert(!strcmp(screen,"setup-arquivo"));
   focus=1;activate(1);assert(!strcmp(screen,"setup-credenciais"));activate(5);assert(!strcmp(screen,"setup-arquivo")&&focus==1);
   go("grade");go("fontes-gerenciar");focus=2;activate(2);assert(!strcmp(screen,"setup-credenciais"));
   activate(5);assert(!strcmp(screen,"fontes-gerenciar")&&focus==2);
   focus=0;activate(0);assert(!strcmp(screen,"setup-tipo"));activate(4);assert(!strcmp(screen,"fontes-gerenciar")&&focus==0);
   go("parental-ajustes");focus=2;activate(2);assert(!strcmp(screen,"parental-lista"));
   activate(900);assert(!strcmp(screen,"parental-ajustes")&&focus==2);
   /* VOD: a title opened from search goes back to search and that result. */
   go("grade");go("vod-filmes");focus=802;go("vod-busca");focus=3;select_item(3);assert(strncmp(screen,"vod-busca",9));
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"vod-busca")&&focus==3);
   focus=3;select_item(3);activate(2);assert(!strcmp(screen,"vod-busca")&&focus==3);
   KEY(SDLK_ESCAPE);assert(!strcmp(screen,"vod-filmes")&&focus==802);
   /* PIN over a blocked title closes to the blocked page, then search. */
   {int ip=-1;for(int i=0;i<count&&ip<0;i++)if(inventory[i].kind==G5_LIVE)ip=i;assert(ip>=0);
    int kids=rules.kids;rules.kids=1;assert(restricted(&inventory[ip]));
    for(int pass=0;pass<2;pass++){
     go("grade");go("vod-filmes");focus=802;go("vod-busca");focus=3;select_item(ip);assert(!strcmp(screen,"parental-canal-bloqueado"));
     focus=0;activate(0);assert(!strcmp(screen,"parental-pin"));
     if(pass)KEY(SDLK_ESCAPE);else activate(1);
     assert(!strcmp(screen,"parental-canal-bloqueado")&&focus==0);
     KEY(SDLK_ESCAPE);assert(!strcmp(screen,"vod-busca")&&focus==3);}
    go("grade");go("vod-filmes");focus=802;go("vod-busca");focus=3;select_item(ip);assert(!strcmp(screen,"parental-canal-bloqueado"));
    activate(1);assert(!strcmp(screen,"vod-busca")&&focus==3);
    rules.kids=kids;}
   /* Series: Back leaves the episode list first, on Load episodes. */
   {int is=-1;for(int i=0;i<count&&is<0;i++)if(inventory[i].kind==G5_SERIES)is=i;assert(is>=0);
    for(int pass=0;pass<2;pass++){
     go("grade");go("vod-series");focus=is;go("vod-serie-temporadas");selected=is;
     series_catalog=inventory;series_count=count;parent_series=is;inventory=calloc(1,sizeof *inventory);count=1;episodes_mode=1;selected=0;
     if(pass)KEY(SDLK_ESCAPE);else activate(2);
     assert(!strcmp(screen,"vod-serie-temporadas")&&!episodes_mode&&selected==is&&focus==1);
     if(pass)KEY(SDLK_ESCAPE);else activate(2);
     assert(!strcmp(screen,"vod-series")&&focus==is);}}
   /* Multiview: limits over the picker go back to the picker and its pick,
    * then to the layout and Add channel; a pick closes to the layout. */
   {int mc=mv_count,pc2=picker_count,md=mode;mode=4;picker_count=10;
    go("grade");go("multiview-4");focus=803;go("multiview-escolher");mv_count=4;focus=905;activate(905);assert(!strcmp(screen,"multiview-limite"));
    KEY(SDLK_ESCAPE);assert(!strcmp(screen,"multiview-escolher")&&focus==905);
    KEY(SDLK_ESCAPE);assert(!strcmp(screen,"multiview-4")&&focus==803);
    focus=803;go("multiview-escolher");mv_count=3;focus=901;activate(901);assert(!strcmp(screen,"multiview-4")&&focus==803);
    focus=801;activate(801);assert(!strcmp(screen,"multiview-3"));KEY(SDLK_ESCAPE);assert(gv_is(screen));
    mv_count=mc;picker_count=pc2;mode=md;}
#undef KEY
  }
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
