#define epg_faixa_desde fixtureFaixa
#include "../src/guia.c"
#undef epg_faixa_desde
#include "sistexto.h"
#include <assert.h>
#include <SDL2/SDL_image.h>

int fixtureFaixa(int epg,time_t de,time_t ate,int pular,EpgProg *out,int cap) {
  (void)ate;static char titulos[5][3][80];
  int n=0;for(int k=pular;k<3 && n<cap;k++) {
    snprintf(titulos[epg][k],sizeof titulos[epg][k],"Jaguars programme %d/%d",epg,k);
    out[n++]=(EpgProg){.ini=de-300+k*1800,.fim=de+1500+k*1800,.titulo=titulos[epg][k]};
  }return n;
}
static Uint32 agora=1000;
static Uint32 relogioTeste(void) { return agora; }
static void tocar(float x,float y) {
  SDL_Event e={0};e.type=SDL_FINGERDOWN;e.tfinger.touchId=e.tfinger.fingerId=1;
  e.tfinger.x=x/NV_TELA_W;e.tfinger.y=y/NV_TELA_H;
  ponteiro_evento(&e,guia_evento);agora+=60;e.type=SDL_FINGERUP;
  ponteiro_evento(&e,guia_evento);agora+=400;
}
static void quadro(void) {
  buscaAtualizar(1.0f/60,agora);agora+=17;
  ponteiro_teste_janela((int)NV_TELA_W,(int)NV_TELA_H);
  ponteiro_quadro(agora);ponteiro_camada();
  txt_novo_quadro();tex_novo_quadro();tex_bombear(6);gfx_novo_quadro();
  glClearColor(.08,.08,.08,1);glClear(GL_COLOR_BUFFER_BIT);buscaDesenhar(agora);ponteiro_desenhar();
}
static void tecla(SDL_Keycode k) {
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;guia_evento(&e);
}
static void pesquisar(int repetir) {
  focoTopo=1;topoCol=G_TOPO_BUSCAR;tecla(SDLK_RETURN);
  assert(buscaEstado==1 && teclado_aberto());
  if (!repetir) {
    assert(st_ime_abrir(ST_TECLADO,teclado_texto(),TECLADO_MAX));
    st_teste_evento("DJaguars");quadro();
  } else {
    // Repeat the saved query using the real phone modal's Conclude target.
    for(int i=0;i<60;i++)quadro();
    const PonteiroAlvo *v;int n=ponteiro_teste_lista(&v),achou=0;
    for(int i=0;i<n;i++)if(v[i].a==6 && v[i].ativar) {
      tocar(v[i].x+v[i].w*.5f,v[i].y+v[i].h*.5f);achou=1;break;
    }
    assert(achou);quadro();
  }
  assert(buscaEstado==2 && !teclado_aberto() && buscaNC==5);
  tocar(32,70);
  assert(buscaEstado==2 && !pediuCanal);
  for(int i=0;i<60;i++)quadro();
  const PonteiroAlvo *v;int n=ponteiro_teste_lista(&v),alvos=0;
  for(int i=0;i<n;i++)if(v[i].focar==ponteiroBuscaItem)alvos++;
  printf("Jaguars: estado=%d anim=%.3f resultados=%d alvos=%d\n",buscaEstado,buscaAnim,buscaN(),alvos);
  assert(buscaAnim>.99f && alvos>5 && buscaNP==15);
  tocar(32,70);assert(buscaEstado==2 && !pediuCanal);
  // Republishing/reordering must rebuild saved result indices.
  canais[0].nome="Other channel";canaisRevisao++;quadro();assert(buscaNC==4);
  canais[0].nome="US| Jaguars 1";canaisRevisao++;quadro();assert(buscaNC==5);
  // Opening New search must show keyboard controls above the old results.
  ponteiroBuscaItem(-1,0);tecla(SDLK_RETURN);quadro();
  n=ponteiro_teste_lista(&v);
  for(int i=0;i<n;i++)assert(v[i].focar!=ponteiroBuscaItem);
  tecla(SDLK_ESCAPE);quadro();assert(buscaEstado==2);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
#ifdef __APPLE__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
#endif
  SDL_Window *win=SDL_CreateWindow("Live search",0,0,1080,2340,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(win);
  SDL_GLContext gl=SDL_GL_CreateContext(win);assert(gl);
  layout_modo_definir(1);layout_tela_definir(1080,2340);gfx_tamanho_alvo(1080,2340);glViewport(0,0,1080,2340);
  assert(gfx_iniciar() && txt_iniciar("deploy/app",1) && tex_iniciar(64));
  char art[1024];assert(realpath("deploy/app/art",art));gfx_icones_dir(art);
  dados_iniciar(getenv("NUVIO_DADOS"));ajustes_dir("deploy/app/art");
  layout_modo_definir(1);layout_tela_definir(1080,2340);gfx_tamanho_alvo(1080,2340);
  ponteiro_iniciar();ponteiro_teste_toque(1);ponteiro_teste_relogio(relogioTeste);st_teste_ligar(1);teclado_teste_modos(1,0,0);
  canais=calloc(5,sizeof *canais);assert(canais);nCanais=capCanais=5;
  cats=calloc(1,sizeof *cats);assert(cats);nCats=capCats=1;
  cats[0]=(GCat){.nome="Sports",.fonte=0};nLtvF=1;ltvF[0].tipo=LTV_ADDON;ltvF[0].base="";strcpy(ltvF[0].chave,"fixture");
  for(int i=0;i<5;i++) {
    char id[80],nome[80];snprintf(id,sizeof id,"fixture:%d",i);snprintf(nome,sizeof nome,"US| Jaguars %d",i+1);
    canais[i]=(GCanal){.id=ltv_dup(&arena,id),.nome=ltv_dup(&arena,nome),.epg=i,.cat=0,.fonte=0,.numero=i+1};
  }
  aberta=1;entrada=1;
  for(int i=0;i<5;i++) {
    pesquisar(i>0);tecla(SDLK_ESCAPE);assert(!buscaEstado);
    for(int k=0;k<60;k++)quadro();
  }
  tex_encerrar();txt_encerrar();gfx_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();
  puts("PASS: repeat search through real guide/keyboard lifecycle and native results renderer");
}
