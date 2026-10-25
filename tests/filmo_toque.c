#include <assert.h>
#include "../src/filmo.c"
static PonteiroAlvo targets[200];static int count;static PonteiroRolagemFn rolagem;
void ponteiro_alvo(float x,float y,float w,float h,PonteiroFn f,PonteiroFn a,int i,int j){targets[count++]=(PonteiroAlvo){.x=x,.y=y,.w=w,.h=h,.focar=f,.ativar=a,.a=i,.b=j};}
void ponteiro_camada(void){count=0;rolagem=NULL;}
void ponteiro_rolagem(PonteiroRolagemFn fn){rolagem=fn;}
const char *i18n(const char *s){return s;}
float gfx_poster_k(void){return 1;}int txt_largura(TxtEstilo e,const char *s){return 100;}float gfx_escala_txt_entrar(void){return 1;}float gfx_escala_tela_txt(void){return 1;}void gfx_escala_sair(float k){}void ajustes_area_conteudo(float a,float b,float *x,float *w){if(x)*x=a;if(w)*w=1920-a-b;}
float nv_layout_h=1080,nv_layout_w=1920;
void fd_sobre(void){}
const FdFicha *fd_ficha(void){static FdFicha d;return &d;}
int fm_marcos(const FmCredito *c,int n,FmMarco *m,int max){return 0;}
int fm_capitulos(const FmCredito *c,int n,const FmMarco *m,int nm,FmCap *cap,int max){return 0;}
int main(void){aba=1;zona=4;ni=40;registrarToque();assert(count>=18&&rolagem);int found=0;for(int i=0;i<count;i++)if(targets[i].a==4&&targets[i].b==3){targets[i].focar(4,3);found=1;}assert(found&&col==3);PonteiroRolagem e={.fase=PONT_ROL_INICIO,.eixoY=1,.x=500,.y=600};assert(rolagem(&e));e.fase=PONT_ROL_MOVER;e.delta=-400;assert(rolagem(&e));assert(topo==1);camada=1;registrarToque();assert(count==0);count=0;alvosFolha(1,0,0);assert(count==4);targets[2].focar(targets[2].a,targets[2].b);assert(op==2);
/* Sheet targets ride the island content offset: a shifted corpo moves every target by the same dx/dy. */
count=0;alvosFolha(1,40,-20);assert(count==4);
{GfxRect r=area((GfxRect){1130,FOLHA_Y,690,60});assert(fabsf(targets[0].x-(r.x+40))<.01f&&fabsf(targets[0].y-(r.y-20))<.01f&&fabsf(targets[0].w-r.w)<.01f);}
camada=1;registrarToque();assert(count==0);
/* "Desfazer Já vi" only on a manual mark; "Já vi" hides once watched. */
{FdFicha *d=(FdFicha*)fd_ficha();int o[5];selecionado=0;d->st[0].estado=FM_VISTO;d->st[0].manual=1;d->st[0].como=FM_COMO_QUIZ;assert(opcoes(o)==4&&o[1]==2&&o[2]==3);d->st[0]=(FmEstado){0};}
/* Compact layout rides a spring: mid targets sit between anchors. */
camada=0;aba=1;zona=2;compactoT=.5f;registrarToque();
{int ok=0;for(int i=0;i<count;i++)if(targets[i].a==2&&targets[i].b==0){assert(fabsf(targets[i].y-py(compacta(CONTEUDO_Y,230)))<.01f&&targets[i].y>py(230)&&targets[i].y<py(CONTEUDO_Y));ok=1;}assert(ok);}
puts("filmography touch: PASS");}
