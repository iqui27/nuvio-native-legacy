// Production Filmography targets after scrolling beyond the first viewport.
#include <assert.h>
#include "../src/layout.c"
#include "../src/filmo.c"
#undef ponteiro_alvo
static PonteiroAlvo targets[256];
static int count;
static PonteiroRolagemFn scrollHandler;
void ponteiro_alvo(float x,float y,float w,float h,PonteiroFn focus,PonteiroFn activate,int a,int b) {
  targets[count++]=(PonteiroAlvo){.x=x,.y=y,.w=w,.h=h,.focar=focus,.ativar=activate,.a=a,.b=b};
}
void ponteiro_alvo_faixa(float x,float y,float w,float h,float lo,float hi,PonteiroFn focus,PonteiroFn activate,int a,int b) {
  float bottom=fminf(y+h,hi); y=fmaxf(y,lo);
  if(bottom>y) ponteiro_alvo(x,y,w,bottom-y,focus,activate,a,b);
}
void ponteiro_rolagem(PonteiroRolagemFn fn) { scrollHandler=fn; }
void ponteiro_camada(void) { count=0; }
const char *i18n(const char *s) { return s; }
float gfx_poster_k(void) { return 1; }
float gfx_escala_txt(void) { return 1; }
float gfx_escala_txt_entrar(void) { return 1; }
float gfx_escala_tela_txt(void) { return 1; }
void gfx_escala_sair(float scale) { (void)scale; }
int txt_largura(TxtEstilo style,const char *s) { return 100; }
void ajustes_area_conteudo(float left,float right,float *x,float *w) {
  if(x)*x=396; if(w)*w=nv_layout_w-396-right;
}
const FdFicha *fd_ficha(void) { static FdFicha data; return &data; }
int fm_marcos(const FmCredito *c,int n,FmMarco *m,int max) { return 0; }
int fm_capitulos(const FmCredito *c,int n,const FmMarco *m,int nm,FmCap *cap,int max) { return 0; }
void fd_enriquecer(int index) { assert(index>=0&&index<ni); }
int main(void) {
  layout_janela_definir(1920,1080);
  aba=1;zona=4;compactoT=1;ni=100;
  for(int i=0;i<ni;i++)indices[i]=i;
  desktopClipTop=py(compacta(CONTEUDO_Y,230))+100;
  registrarToque(); assert(scrollHandler);
  int originalEnd=gradeLinhas()*gradeCols();
  PonteiroRolagem e={.fase=PONT_ROL_INICIO,.eixoY=1,.x=800,.y=700};
  assert(scrollHandler(&e));
  e.fase=PONT_ROL_MOVER;e.delta=-gradePasso()*4-75;
  assert(scrollHandler(&e)); assert(topo==0&&scroll>gradePasso()*4);
  count=navigationCount=0;registrarToque();
  int hit=-1;
  for(int i=0;i<count;i++)if(targets[i].a==4&&targets[i].b>=originalEnd&&targets[i].h>100){hit=i;break;}
  assert(hit>=0);
  PonteiroAlvo *target=&targets[hit];float before=scroll;
  target->focar(target->a,target->b);
  assert(zona==4&&col==target->b&&scroll==before);
  assert(target->ativar);target->ativar(target->a,target->b);
  assert(camada==1&&selecionado==indices[col]&&scroll==before);
  puts("PASS: Filmography free scroll beyond viewport, clipped hover and card activation");
}
