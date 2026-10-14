#include <assert.h>
#include "../src/filmo.c"
static PonteiroAlvo targets[200];static int count;static PonteiroRolagemFn scroll;
void ponteiro_alvo(float x,float y,float w,float h,PonteiroFn f,PonteiroFn a,int i,int j){targets[count++]=(PonteiroAlvo){.x=x,.y=y,.w=w,.h=h,.focar=f,.ativar=a,.a=i,.b=j};}
void ponteiro_camada(void){count=0;scroll=NULL;}
void ponteiro_rolagem(PonteiroRolagemFn fn){scroll=fn;}
void fd_sobre(void){}
const FdFicha *fd_ficha(void){static FdFicha d;return &d;}
int fm_marcos(const FmCredito *c,int n,FmMarco *m,int max){return 0;}
int fm_capitulos(const FmCredito *c,int n,const FmMarco *m,int nm,FmCap *cap,int max){return 0;}
int main(void){aba=1;zona=4;ni=40;registrarToque();assert(count>=18&&scroll);int found=0;for(int i=0;i<count;i++)if(targets[i].a==4&&targets[i].b==3){targets[i].focar(4,3);found=1;}assert(found&&col==3);PonteiroRolagem e={.fase=PONT_ROL_INICIO,.eixoY=1,.x=500,.y=600};assert(scroll(&e));e.fase=PONT_ROL_MOVER;e.delta=-400;assert(scroll(&e));assert(topo==1);camada=1;registrarToque();assert(count==5);targets[2].focar(targets[2].a,targets[2].b);assert(op==2);puts("filmography touch: PASS");}
