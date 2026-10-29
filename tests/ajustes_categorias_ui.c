// Actual Settings renderer and pointer dispatch, with synthetic category data.
#include <string.h>
#include "guia.h"
static GuiaCategoria fixture[12];
static int saves;
static int ler(GuiaCategoria *out,int max) { int n=out&&max<12?max:12;if(out)memcpy(out,fixture,(size_t)n*sizeof *out);return n; }
static int salvar(const GuiaCategoria *out,int n) { for(int k=0;k<n;k++)for(int i=0;i<12;i++)if(fixture[i].chave==out[k].chave)fixture[i].oculta=out[k].oculta;if(n)saves++;return 1; }
#define guia_categorias ler
#define guia_categorias_definir salvar
#include "../src/ajustes.c"
#undef guia_categorias
#undef guia_categorias_definir
#include <assert.h>
#include <SDL2/SDL_image.h>
static Uint32 clockTest=1000;
static Uint32 agora(void) { return clockTest; }
static void quadro(void) {
  ponteiro_teste_janela((int)NV_TELA_W,(int)NV_TELA_H);
  ponteiro_quadro(clockTest);ponteiro_camada();ajPontEditor=1;
  txt_novo_quadro();tex_novo_quadro();tex_bombear(6);gfx_novo_quadro();
  glClearColor(.065f,.067f,.085f,1);glClear(GL_COLOR_BUFFER_BIT);
  AJ_ESCALA_INI();aj2EditorEscolha(aj2EditorR());AJ_ESCALA_FIM();
  ponteiro_desenhar();clockTest+=100;SDL_Delay(2);
}
static void clicar(PonteiroFn fn,int a) {
  const PonteiroAlvo *v;int n=ponteiro_teste_lista(&v),achou=0;
  for(int i=0;i<n;i++)if(v[i].ativar==fn && (fn!=ajIptvToqueEscolher||v[i].a==a)) {
    float x=v[i].x+v[i].w*.5f,y=v[i].y+v[i].h*.5f;
    assert(ponteiro_achar(v,n,x,y)==i);
    SDL_Event e={0};e.type=SDL_FINGERDOWN;e.tfinger.touchId=e.tfinger.fingerId=1;
    e.tfinger.x=x/NV_TELA_W;e.tfinger.y=y/NV_TELA_H;
    ponteiro_evento(&e,ajustes_evento);e.type=SDL_FINGERUP;clockTest+=60;
    ponteiro_evento(&e,ajustes_evento);clockTest+=400;achou=1;break;
  }
  assert(achou);
}
static void imagem(const char *dir,int w,int h,int scale) {
  char file[1024];snprintf(file,sizeof file,"%s/category-%dx%d-%d.png",dir,w,h,scale);
  unsigned char *pix=malloc((size_t)w*h*4);assert(pix);
  glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pix);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);assert(s);
  for(int y=0;y<h;y++)memcpy((char*)s->pixels+y*s->pitch,pix+(size_t)(h-y-1)*w*4,(size_t)w*4);
  assert(!IMG_SavePNG(s,file));SDL_FreeSurface(s);free(pix);
}
int main(int argc,char **argv) {
  setvbuf(stdout,NULL,_IONBF,0);puts("UI fixture: SDL initialization");
  assert(argc==2 && getenv("NUVIO_DADOS"));assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
#ifdef __APPLE__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
#endif
  puts("UI fixture: create GL context");
  SDL_Window *win=SDL_CreateWindow("Category Settings",0,0,1080,1920,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(win);
  SDL_GLContext gl=SDL_GL_CreateContext(win);assert(gl);puts("UI fixture: gfx/fonts");gfx_tamanho_alvo(1080,1920);assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app",1));assert(tex_iniciar(64));
  char art[1024];assert(realpath("deploy/app/art",art));gfx_icones_dir(art);ajustes_recursos(art);
  dados_iniciar(getenv("NUVIO_DADOS"));ajustes_iniciar();layout_modo_definir(1);layout_tela_definir(1080,1920);
  for(int i=0;i<12;i++) { fixture[i].chave=i+1;fixture[i].sessao=nv_perfil_sessao_ler();
    snprintf(fixture[i].nome,sizeof fixture[i].nome,"US| 24/7 ACTION/ADVENTURE RAW 60fps - Category %d",i+1);
    snprintf(fixture[i].conta,sizeof fixture[i].conta,"M3U %d",i%2+1);
  }
  for(int mode=0;mode<2;mode++)for(int scale=100;scale<=150;scale+=50) {
    printf("UI fixture: mode=%d scale=%d\n",mode,scale);
    int w=mode?1920:1080,h=mode?1080:1920;
    SDL_SetWindowSize(win,w,h);SDL_PumpEvents();SDL_GL_SwapWindow(win);
    layout_modo_definir(!mode);layout_tela_definir(w,h);
    gfx_tamanho_alvo(w,h);glViewport(0,0,w,h);ajEscalaTestePct=scale;
    uxAbrirEditor(AJ_GUIA_CATEGORIAS);ponteiro_iniciar();ponteiro_teste_toque(1);ponteiro_teste_relogio(agora);
    for(int i=0;i<12;i++)quadro();
    const PonteiroAlvo *v;int n=ponteiro_teste_lista(&v),rows=0,buttons=0;
    float fimLista=0,inicioBotoes=100000;
    for(int i=0;i<n;i++) {
      if(v[i].ativar==ajIptvToqueEscolher) { rows++;if(v[i].y+v[i].h>fimLista)fimLista=v[i].y+v[i].h; }
      if(v[i].ativar==ajToqueConfirmar || v[i].ativar==ajToqueCancelar) { buttons++;if(v[i].y<inicioBotoes)inicioBotoes=v[i].y; }
    }
    assert(rows>0 && buttons==2 && fimLista<inicioBotoes);
    clicar(ajIptvToqueEscolher,0);assert(uxCategorias[0].oculta && !fixture[0].oculta && !saves);
    for(int i=0;i<8;i++)quadro();imagem(argv[1],w,h,scale);clicar(ajToqueConfirmar,0);assert(!uxEditor && fixture[0].oculta && saves==1);
    uxAbrirEditor(AJ_GUIA_CATEGORIAS);for(int i=0;i<8;i++)quadro();
    clicar(ajIptvToqueEscolher,0);assert(!uxCategorias[0].oculta);quadro();clicar(ajToqueCancelar,0);
    assert(!uxEditor && fixture[0].oculta && saves==1);fixture[0].oculta=0;saves=0;
  }
  tex_encerrar();txt_encerrar();gfx_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();
  puts("PASS: actual renderer/pointer targets in portrait and TV layout, 100/150% Settings scale; row/Confirm/Cancel and footer separation");
}
