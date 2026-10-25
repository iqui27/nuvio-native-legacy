// Render the reviewed states with real fonts; reject missing or clipped guidance.
#include "../src/text.h"
static TxtLinha checked_line(TxtEstilo, const char *, int, int, int, int, float);
static TxtLinha checked_full(TxtEstilo, const char *, int, int, int, int);
static float checked_block(TxtEstilo, const char *, int, int, int, float, float, float, float, float, int);
#define txt_linha checked_full
#define txt_linha_corta checked_line
#define txt_bloco checked_block
#include "../src/ajustes.c"
#undef txt_linha
#undef txt_linha_corta
#undef txt_bloco
#include "nvtmp.h"
#include <assert.h>
#include <SDL2/SDL_image.h>

static const char *wanted[4];
static int seen[4], clipped[4], failures;
static void observe(TxtEstilo style, const char *s, float width, int lines) {
  for (int i=0;i<4;i++) if(wanted[i] && strstr(s,wanted[i])) {
    seen[i]++;
    if(txt_largura(style,s)>width*lines) clipped[i]++;
  }
}
static TxtLinha checked_full(TxtEstilo e,const char *s,int r,int g,int b,int a) {
  observe(e,s,1920,1);return txt_linha(e,s,r,g,b,a);
}
static TxtLinha checked_line(TxtEstilo e,const char *s,int r,int g,int b,int a,float w) {
  observe(e,s,w,1); return txt_linha_corta(e,s,r,g,b,a,w);
}
static float checked_block(TxtEstilo e,const char *s,int r,int g,int b,float x,float y,float w,float lh,float a,int lines) {
  observe(e,s,w,lines); return txt_bloco(e,s,r,g,b,x,y,w,lh,a,lines);
}
static void expect(int ok,const char *message) {
  printf("%s: %s\n",ok?"PASS":"FAIL",message); failures+=!ok;
}
static void frame(SDL_Window *win,const char *dir,const char *name) {
  for(int i=0;i<60;i++) {
    SDL_PumpEvents();txt_novo_quadro();gfx_novo_quadro();tex_novo_quadro();tex_bombear(6);
    glClearColor(.025f,.025f,.03f,1);glClear(GL_COLOR_BUFFER_BIT);
    ajustes_atualizar(1.0f/60,SDL_GetTicks());ajustes_desenhar(SDL_GetTicks());
    SDL_GL_SwapWindow(win);SDL_Delay(16);
  }
  if(dir) {
    unsigned char *pixels=malloc(1920*1080*4);
    glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
    assert(pixels && s);
    for(int y=0;y<1080;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
    char path[1024];snprintf(path,sizeof path,"%s/%s.png",dir,name);
    assert(IMG_SavePNG(s,path)==0); SDL_FreeSurface(s);free(pixels);
  }
  SDL_GL_SwapWindow(win);
}
static void watch(const char *a,const char *b,const char *c,const char *d) {
  wanted[0]=a;wanted[1]=b;wanted[2]=c;wanted[3]=d;
  memset(seen,0,sizeof seen);memset(clipped,0,sizeof clipped);
}
int main(int argc,char **argv) {
  char dir[1024];snprintf(dir,sizeof dir,"%s/nuvio-v4-review-XXXXXX",nvt_dir());
  assert(mkdtemp(dir));assert(setenv("NUVIO_DADOS",dir,1)==0);dados_iniciar(dir);
  snprintf(dirAjustes,sizeof dirAjustes,"%s",dir);
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *win=SDL_CreateWindow("Settings review",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app",1));tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");ajustes_recursos("deploy/app/art");ajustes_iniciar();
  const char *output=argc>1?argv[1]:NULL;
  assert(ajustes_teste_quadro("v4-inicio-simples"));
  ajustes_teste_fonte_interface(TXT_FAMILIA_MONTSERRAT);
  // Exactly three profile changes, like the approved home fixture.
  memcpy(valor,valorPadrao,sizeof valor);valor[AJ_IDIOMA]=IDIOMA_PT+1;aj4Escopo=0;
  valor[AJ_AVANCADAS]=1;valor[AJ_QUALIDADE]=2;valor[AJ_AUD_LINGUA]=1;valor[AJ_LEG_LINGUA]=2;
  watch("4 tarefas do dia a dia","Diferente do padrão · 3","Perfil ativo: Henrique",NULL);
  frame(win,output,"home");
  expect(seen[0]&&!clipped[0],"home shows all four tasks");
  expect(seen[1]&&!clipped[1],"home shows live profile difference count");
  expect(seen[2]&&!clipped[2],"home identifies active profile below presets");
  valor[AJ_QUALIDADE]=valorPadrao[AJ_QUALIDADE];
  watch("Diferente do padrão · 2",NULL,NULL,NULL);frame(win,NULL,NULL);
  expect(seen[0]&&!clipped[0],"difference count follows saved values");
  // More groups proof starts on Image and sound, rather than P2P.
  assert(ajustes_teste_quadro("v4-secao-grupos"));
  watch("Imagem e som","Mais grupos",NULL,NULL);frame(win,output,"groups");
  expect(seen[0]&&seen[1],"More groups proof keeps matching Image and sound state");
  assert(ajustes_teste_quadro("v4-inicio-simples"));
  aj4Entrar(1);aj4Linha=1;
  watch("Ajustes / Áudio e legendas · Idiomas","NESTA SEÇÃO","Quatro escolhas do dia a dia.","Mude para Avançado para ver o catálogo completo.");
  frame(win,output,"languages");
  expect(seen[0]&&seen[1]&&seen[2]&&!clipped[2]&&seen[3]&&!clipped[3],"Simple languages restores breadcrumb, sidebar and Advanced guidance");
  char hour[48];ajCenaHora(hour,sizeof hour);
  watch("Sala",hour,NULL,NULL);frame(win,NULL,NULL);
  expect(seen[0]&&seen[1]&&!clipped[0]&&!clipped[1],"shared header shows room context and current formatted clock");
  valor[AJ_CW_LIGADO]=0;valor[AJ_CW_THUMB]=1;valor[AJ_CW_BLUR_PROX]=1;
  focarOpcao(AJ_CW_BLUR_PROX);
  watch("Precisa ligar \"Miniatura do episódio\"","OK liga as duas",NULL,NULL);
  frame(win,output,"episode-dependency");
  expect(seen[0]&&!clipped[0]&&seen[1]&&!clipped[1],"episode dependency and OK action fit completely");
  int pageRow=aj4Linha;
  watch("Itens anteriores","Próximos itens",NULL,NULL);frame(win,NULL,NULL);
  expect(seen[0]&&seen[1]&&!clipped[0]&&!clipped[1],"middle page visibly offers previous and next items");
  if(seen[0]&&seen[1]) {
    aj4Acao=0;aj4Linha=7;aj4Evento(SDLK_DOWN);
    expect(aj4Acao==2,"Down from last visible row focuses Previous items");
    aj4PontAtivar(3,2);
    expect(aj4Linha==0 && !aj4Acao && !aj4Review,"pointer Previous items opens first page without reset");
    watch("Itens anteriores","Próximos itens",NULL,NULL);frame(win,NULL,NULL);
    expect(!seen[0]&&seen[1],"first page only offers next items");
    aj4PontAtivar(3,3);
    expect(aj4Linha==4 && !aj4Acao && !aj4Review,"pointer Next items opens middle page without reset");
    aj4Linha=7;aj4Evento(SDLK_DOWN);aj4Evento(SDLK_RIGHT);aj4Evento(SDLK_RETURN);
    expect(aj4Linha==8 && !aj4Acao && !aj4Review,"D-pad Next items opens final partial page");
    watch("Itens anteriores","Próximos itens",NULL,NULL);frame(win,NULL,NULL);
    expect(seen[0]&&!seen[1],"final partial page only offers previous items");
    aj4PontAtivar(3,4);
    expect(aj4Escopo==1 && !aj4Review,"scope chip toggles reset scope without resetting");
    aj4Escopo=0;aj4Acao=0;
  }
  aj4Linha=pageRow;aj4Acao=aj4Review=0;
  valor[AJ_SEEKR_LIGADO]=1;valor[AJ_SEEKR_FITA]=1;
  snprintf(seekrChave,sizeof seekrChave,"offline-test-key");focarOpcao(AJ_SEEKR_FITA);
  watch("Precisa ligar \"Miniaturas na barra de tempo\"","OK liga as duas",NULL,NULL);
  frame(win,output,"strip-dependency");
  expect(seen[0]&&!clipped[0]&&seen[1]&&!clipped[1],"thumbnail strip prerequisite and OK action fit completely");
  expect(!lig(AJ_SEEKR_LIGADO)&&!lig(AJ_SEEKR_FITA),"rendering leaves both dependency values unchanged");
  aj4Evento(SDLK_RETURN);
  expect(lig(AJ_SEEKR_LIGADO)&&lig(AJ_SEEKR_FITA),"OK enables thumbnail strip and prerequisite");
  frame(win,output,"strip-enabled");
  txt_encerrar();SDL_DestroyWindow(win);SDL_Quit();return failures?1:0;
}
