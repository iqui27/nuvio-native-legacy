/* Exercise the real detail drawing and pointer dispatch on a portrait canvas. */
#define NV_TOUCH_UI
#define extras_comentario_texto touchCommentText
#define main detail21_main
#include "detail21.c"
#undef main

const char *touchCommentText(int i) {
  (void)i;
  static char text[8000];
  if (!text[0]) for(int j=0;j<100;j++) strcat(text,"A long review must wrap within the portrait sheet and remain scrollable. ");
  return text;
}

static void frame(int list) {
  ponteiro_quadro(SDL_GetTicks());
  if (list) desenhaLowerList(1);
  else desenhaRodape(SEC_DETALHES,1);
  ponteiro_desenhar();
}
static void finger(Uint32 type,float x,float y) {
  SDL_Event e={0}; e.type=type; e.tfinger.fingerId=1;
  e.tfinger.x=x/NV_TELA_W; e.tfinger.y=y/NV_TELA_H;
  assert(ponteiro_evento(&e,detail_evento));
}
int main(void) {
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  SDL_Window *window=SDL_CreateWindow("Detail touch regression",0,0,540,960,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app",1));
  dados_iniciar("deploy/app/art"); ajustes_iniciar();
  layout_tela_definir(540,960); layout_modo_definir(1);
  ponteiro_iniciar(); ponteiro_teste_toque(1); ponteiro_teste_janela(540,960);
  CatItem item={0}; strcpy(item.tipo,"movie"); strcpy(item.imdb,"tt9000011");
  strcpy(item.titulo,"Portrait fixture");
  cat_definir_tudo(&item,1,NULL,0); idx=0; aberto=1; saindo=0;
  conteudoSec[SEC_DETALHES]=200; scrollY=0;
  frame(0);
  const PonteiroAlvo *targets; int n=ponteiro_teste_lista(&targets);
  assert(n>0);
  for(int i=0;i<n;i++) assert(targets[i].x>=0 && targets[i].x+targets[i].w<=NV_TELA_W);
  quoteCount=20;
  for(int kind=1;kind<=2;kind++) {
    lowerList=kind; lowerCursor=0; lowerEnter=1; lowerScroll=lowerTarget=0;
    foco.fileira=kind==2 ? SEC_FRASES : SEC_COMENTARIOS;
    frame(1); n=ponteiro_teste_lista(&targets);
    assert(n>=2);
    for(int i=0;i<n;i++) assert(targets[i].x>=0 && targets[i].x+targets[i].w<=NV_TELA_W);
    /* Touch row 1: comments open their reader, quotes select their row. */
    PonteiroAlvo row=targets[1];
    finger(SDL_FINGERDOWN,row.x+20,row.y+20); finger(SDL_FINGERUP,row.x+20,row.y+20);
    assert(lowerCursor==1);
    if(kind==1) { assert(sinopse_aberta()); sinopse_fechar(); }
    frame(1);
    finger(SDL_FINGERDOWN,400,500); finger(SDL_FINGERMOTION,400,250); finger(SDL_FINGERUP,400,250);
    assert(lowerScroll>0);
    float saved=lowerScroll; frame(1); assert(lowerScroll==saved);
    /* Last registered target is the explicit close control. */
    n=ponteiro_teste_lista(&targets); PonteiroAlvo close=targets[n-1];
    finger(SDL_FINGERDOWN,close.x+close.w/2,close.y+close.h/2);
    finger(SDL_FINGERUP,close.x+close.w/2,close.y+close.h/2);
    assert(!lowerList && !saindo);
  }
  layout_modo_definir(0); lowerList=2; lowerEnter=1; lowerScroll=lowerTarget=0;
  frame(1); assert(NV_TELA_W==1920 && NV_TELA_H==1080);
  puts("detail portrait: footer bounds, comments/quotes tap, drag, close and TV mode OK");
  return 0;
}
