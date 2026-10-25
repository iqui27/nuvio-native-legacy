// Offline approved episode content, focus and material captures.
#include "text.h"
static int checking, titles, synopses, metadata;
static char capturedMeta[512];
static TxtLinha checkedLine(TxtEstilo style, const char *s, int r, int g, int b, int a);
static float checkedBlock(TxtEstilo style, const char *s, int r, int g, int b,
                          float x, float y, float w, float leading, float a, int lines);
#define txt_linha checkedLine
#define txt_bloco_corta checkedBlock
#define main episode_fixture_main
#include "detail_eps_shot.c"
#undef main
#undef txt_linha
#undef txt_bloco_corta

static TxtLinha checkedLine(TxtEstilo style, const char *s, int r, int g, int b, int a) {
  if (checking && style == TXT_G30M) {
    assert(strstr(s, "4. ") == s && strstr(s, "Fixture episode"));
    titles++;
  }
  if (checking && style == TXT_G22M) {
    snprintf(capturedMeta, sizeof capturedMeta, "%s", s); metadata++;
  }
  return txt_linha(style,s,r,g,b,a);
}
static float checkedBlock(TxtEstilo style, const char *s, int r, int g, int b,
                          float x, float y, float w, float leading, float a, int lines) {
  if (checking) {
    assert(style == TXT_DET_META2 && x == 96 && y == 452);
    assert(w == 1100 && leading == 36 && lines == 3);
    assert(!strcmp(s, "Fixture synopsis")); synopses++;
  }
  return txt_bloco_corta(style,s,r,g,b,x,y,w,leading,a,lines);
}

int main(int argc, char **argv) {
  assert(argc == 3);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  janela=SDL_CreateWindow("Episode information",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(janela && SDL_GL_CreateContext(janela));
  gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app",1));
  tex_iniciar(16); gfx_icones_dir("deploy/app/art");
  ajustes_iniciar(); dados_iniciar("deploy/app/art"); extras_carregar("deploy/app/art");
  for (int layout=0;layout<3;layout++) for (int glass=0;glass<2;glass++) {
    char path[900]; snprintf(path,sizeof path,"%s/ajustes.txt",dados_dir());
    FILE *file=fopen(path,"w"); assert(file);
    fprintf(file,"performanceAutoVersaoLocal 1\nidioma 1\nhomeLayoutLocal %d\nvidroLocal %d\nescalaTextoVersaoLocal 1\nescalaTextoLocal 2\n",layout,glass?0:1);
    fclose(file); ajustes_dir(dados_dir());
    montarCatalogo(); semear(1,T1_N,3); abrir(0,0); nosEpisodios(3);
    parado=1; quadros(32);
    CatEp saved=episodios[3];
    for (int missing=0;missing<4;missing++) {
      episodios[3]=saved;
      strcpy(episodios[3].nome,"Fixture episode");
      strcpy(episodios[3].sinopse,"Fixture synopsis");
      if (missing&1) episodios[3].data[0]=0;
      if (missing&2) episodios[3].duracao[0]=0;
      cat_definir_episodios(0,episodios,T1_N+T2_N+T3_N);
      titles=synopses=metadata=0; checking=1;
      txt_novo_quadro();
      desenhaEpisodio((GfxRect){100,100,epCardW(),epCardH()},3,1,1,10000);
      checking=0;
      assert(titles==1 && synopses==1 && metadata==1);
      assert((strstr(capturedMeta,"·")!=NULL)==!(missing&1));
      if (missing==3) assert(!capturedMeta[0]);
    }
    episodios[3]=saved; cat_definir_episodios(0,episodios,T1_N+T2_N+T3_N);
    quadros(3);
    snprintf(path,sizeof path,"%s/%s-layout%d-glass%d.png",argv[1],argv[2],layout,glass);
    gravar(path); assert(glGetError()==GL_NO_ERROR);
  }
  puts("PASS: episode title, duration/date absence, synopsis bounds and material captures");
  return 0;
}
