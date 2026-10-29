// v2 walkthrough: first/repeat show, auto-advance timing, component lifecycle
// (opens by the real code path, closes, leaves nothing open), Settings replay,
// reduced motion, and the hidden SDL/GL FBO screenshot harness with a real
// catalog so the poster-menu tip has a poster to open on.
#include "../src/ajustes.c"
#include "app.h"
#include "catalogo.h"
#include "central.h"
#include "ctxmenu.h"
#include "dicascontrole.h"
#include "home.h"
#include "ilha.h"
#include "menu.h"
#include "novidades20.h"
#include "novidades_cartao.h"
#include "salvospainel.h"
#include "spotlight.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static void key(SDL_Keycode k, void (*evento)(const SDL_Event *)) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; evento(&e);
}
static void settle(void) { for (int i = 0; i < 40; i++) dicascontrole_atualizar(1.f/60, SDL_GetTicks()); }
static void settings(void) {
  int item = -1;
  ajustes_iniciar();
  for (int i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_DICASCONTROLE) item = i;
  assert(item >= 0 && visivel(item) && focavel(item));
  assert(!strcmp(uxCaminho(AJ_DICASCONTROLE), "Sobre e ajuda"));
  ajPontLista = 1; ajPonteiroItem(item, 0);
  assert(focoOp == AJ_DICASCONTROLE);
  key(SDLK_RETURN, ajustes_evento);
  assert(ajustes_pediu_dicascontrole());
  assert(!ajustes_pediu_dicascontrole());
  dicascontrole_abrir(); assert(dicascontrole_aberto());
}

static int nadaAberto(void) {
  return !spainel_aberto() && !spainel_visivel() && !spot_aberto() && !spot_visivel() &&
         !central_aberta() && !ctx_aberto() && !menu_aberto();
}

static void state(int first) {
  assert(!dicascontrole_ocupado());
  dicascontrole_teste_seco(1);   // sem GL nesta parte: so a maquina de estados
  dicascontrole_atualizar(1, 0); assert(!dicascontrole_aberto());
  // AUTO-SHOW GATE: once per install and per version, only on a quiet Home.
  if (first) {
    for (int i = 0; i < 300; i++) { dicascontrole_home_livre(0, .05f); dicascontrole_atualizar(.05f, 0); }
    assert(!dicascontrole_ocupado());                    // modal/profile picker up: never
    for (int i = 0; i < 20; i++) dicascontrole_home_livre(1, .05f);
    dicascontrole_home_livre(0, .05f);                   // a modal appeared: timer resets
    for (int i = 0; i < 20; i++) dicascontrole_home_livre(1, .05f);
    assert(!dicascontrole_ocupado());
    for (int i = 0; i < 20; i++) dicascontrole_home_livre(1, .05f);
    assert(dicascontrole_aberto());                      // fresh install: shows
    key(SDLK_RIGHT, app_evento);                         // any key dismisses, swallowed
    assert(!dicascontrole_aberto() && dicascontrole_pagina() == 0);
    settle();
    char *s = dados_ler(DICASCONTROLE_ARQUIVO); assert(s && !strcmp(s, "dev" "\n")); free(s);
  } else {
    for (int i = 0; i < 300; i++) dicascontrole_home_livre(1, .05f);
    assert(!dicascontrole_ocupado());                    // same version, 2nd launch: no
    dados_gravar(DICASCONTROLE_ARQUIVO, "0.0.0\n");      // version bump: again
    dicascontrole_home_livre(0, .05f);                   // marker re-read on re-entry
    for (int i = 0; i < 40; i++) dicascontrole_home_livre(1, .05f);
    assert(dicascontrole_aberto());
    key(SDLK_ESCAPE, app_evento); settle(); assert(!dicascontrole_ocupado());
    char *s = dados_ler(DICASCONTROLE_ARQUIVO); assert(s && !strcmp(s, "dev" "\n")); free(s);
  }
  for (int i = 0; i < 3; i++) { dicascontrole_depois_novidades(); settle(); assert(!dicascontrole_ocupado()); }
  settings(); key(SDLK_ESCAPE, app_evento); settle(); assert(!dicascontrole_ocupado());
  // A CHAVE DE CADA DICA E CH+/CH- EM TODA TV: o Amarelo acabou.
  for (int m = 0; m < 4; m++) {
    dicascontrole_teste_modo(m);
    assert(!strcmp(dicascontrole_tecla(0), m == DICAS_TOQUE ? "Relógio na tela" : "CH+"));
    assert(!strcmp(dicascontrole_tecla(1), m == DICAS_TOQUE ? "Buscar" : "CH-"));
  }
  // AVANCA SOZINHO: sem tecla nenhuma, as quatro dicas passam e terminam,
  // gravando a marca — e sem deixar componente aberto no caminho.
  dados_apagar(DICASCONTROLE_ARQUIVO);
  dicascontrole_abrir(); assert(dicascontrole_aberto());
  for (int i = 0; dicascontrole_aberto() && i < 2000; i++) dicascontrole_atualizar(.05f, 0);
  assert(!dicascontrole_aberto()); settle(); assert(!dicascontrole_ocupado());
  char *s = dados_ler(DICASCONTROLE_ARQUIVO); assert(s && !strcmp(s, "dev" "\n")); free(s);
  puts("PASS: dismissal, auto-advance, page bounds, modal routing, Settings replay, marker");
}

// ---- shots -----------------------------------------------------------------

static GLuint fbo, tex;
static int width = 1920, height = 1080;

static CatItem itens[4];
static CatFileira filas[2];

static void catalogo(void) {
  const char *nomes[] = { "O Jardim", "Sal e Luz", "Noite de Verão", "Fronteira" };
  for (int i = 0; i < 4; i++) {
    memset(&itens[i], 0, sizeof itens[i]);
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "%s", nomes[i]);
    snprintf(itens[i].genero, sizeof itens[i].genero, "Drama");
    snprintf(itens[i].meta, sizeof itens[i].meta, "2024 · %d min", 92 + i * 7);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "movie");
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt1400000%d", i);
    snprintf(itens[i].poster, sizeof itens[i].poster, "deploy/app/art/poster/%02d.jpg", 12 + i);
    snprintf(itens[i].backdrop, sizeof itens[i].backdrop, "deploy/app/art/03.jpg");
    itens[i].nota = 70 + i;
  }
  memset(filas, 0, sizeof filas);
  snprintf(filas[0].chave, sizeof filas[0].chave, "demo_movie_pop");
  snprintf(filas[0].titulo, sizeof filas[0].titulo, "Popular · Filme");
  snprintf(filas[0].tipo, sizeof filas[0].tipo, "movie");
  filas[0].ini = 0; filas[0].n = 4;
  snprintf(filas[1].chave, sizeof filas[1].chave, "demo_series_alt");
  snprintf(filas[1].titulo, sizeof filas[1].titulo, "Em alta · Série");
  snprintf(filas[1].tipo, sizeof filas[1].tipo, "series");
  filas[1].ini = 1; filas[1].n = 3;
  cat_definir_tudo(itens, 4, filas, 2);
}

static void backdrop(void) {
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H}, 0, .06f,.07f,.1f,1);
  for (int i = 0; i < 7; i++) {
    GfxRect card = {NV_TELA_W*.08f+i*NV_TELA_W*.13f,NV_TELA_H*.25f,NV_TELA_W*.11f,NV_TELA_H*.5f};
    gfx_cor(card, .03f, .16f+i*.018f, .13f, .20f+i*.01f, 1);
  }
}

static void frame(void) {
  SDL_PumpEvents(); txt_novo_quadro(); gfx_novo_quadro();
  ponteiro_quadro(SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0,0,width,height);
  glClearColor(.05f,.05f,.06f,1); glClear(GL_COLOR_BUFFER_BIT);
  // Relogio virtual: a mola da ilha mede dt pelo relogio, e o laco roda sem esperar.
  static Uint32 base, quadros;
  if (!base) base = SDL_GetTicks();
  Uint32 ag = base + (quadros++ * 1000u) / 60u;
  // A dica e o caminho real: ela mesma abre e fecha o componente. Aqui so
  // rodamos as atualizacoes que o app rodaria, na mesma ordem.
  dicascontrole_atualizar(1.f/60, ag);
  spainel_atualizar(1.f/60, ag);
  spot_atualizar(1.f/60, ag);
  ctx_atualizar(1.f/60, ag);
  menu_atualizar(1.f/60, ag);
  central_atualizar(1.f/60, ag);
  backdrop();
  // ...e desenhamos as camadas que app_desenhar desenharia.
  if (spainel_visivel()) spainel_desenhar(ag);
  if (ctx_aberto()) ctx_desenhar(ag);
  if (menu_visivel()) menu_desenhar(ag);
  ilha_relogio_visivel(1); ilha_posicionar(1); ilha_desenhar(ag); // central da ilha
  spot_desenhar(ag, 0);
  dicascontrole_desenhar(ag);
  ponteiro_desenhar(); glFinish();
}
static void capture(const char *out, const char *name) {
  char path[900]; snprintf(path,sizeof path,"%s/%s.png",out,name);
  unsigned char *pix=malloc(width*height*4); assert(pix);
  glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pix);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32); assert(s);
  for(int y=0;y<height;y++) memcpy((char*)s->pixels+y*s->pitch,pix+(height-1-y)*width*4,width*4);
  assert(IMG_SavePNG(s,path)==0); SDL_FreeSurface(s); free(pix);
  printf("PNG: %s\n",path);
}

static void finger(Uint32 type, float x, float y) {
  SDL_Event e = {0}; e.type = type; e.tfinger.touchId = 1; e.tfinger.fingerId = 1;
  e.tfinger.x = x/NV_TELA_W; e.tfinger.y = y/NV_TELA_H;
  assert(ponteiro_evento(&e, app_evento));
}
static void touch(void) {
  finger(SDL_FINGERDOWN,40,40); ponteiro_cancelar_toque(); finger(SDL_FINGERUP,40,40);
  dicascontrole_abrir(); for(int i=0;i<40;i++) frame();
  ponteiro_teste_janela(width,height);
  // Ponto 3 escolhe a pagina direto; deslizar para a direita volta uma.
  finger(SDL_FINGERDOWN,NV_TELA_W*.5f+12,NV_TELA_H-92);
  finger(SDL_FINGERUP,NV_TELA_W*.5f+12,NV_TELA_H-92);
  assert(dicascontrole_pagina()==2);
  for(int i=0;i<40;i++) frame();
  finger(SDL_FINGERDOWN,300,900); finger(SDL_FINGERMOTION,600,900); finger(SDL_FINGERUP,600,900);
  assert(dicascontrole_pagina()==1);
  for(int i=0;i<40;i++) frame();
  // O botao Concluir segue tappable em qualquer orientacao.
  finger(SDL_FINGERDOWN,NV_TELA_W*.5f,NV_TELA_H-50);
  finger(SDL_FINGERUP,NV_TELA_W*.5f,NV_TELA_H-50);
  assert(!dicascontrole_aberto()); settle();
  puts("PASS: page dots, horizontal swipe and Done use real touch dispatch");
}

// As tres provas por dica: a tecla na prensa, o componente aberto, a legenda.
static void shots(const char *out) {
  assert(SDL_InitSubSystem(SDL_INIT_VIDEO)==0); IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("remote-tips-shot",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl=SDL_GL_CreateContext(w); assert(gl);
  glGenTextures(1,&tex); glGenFramebuffers(1,&fbo);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1));
  ponteiro_iniciar(); gfx_icones_dir("deploy/app/art");
  catalogo();
  static const char *const modes[]={"android-gtv","webos","tizen","touch"};
  for (int m=0;m<4;m++) {
    dicascontrole_teste_modo(m);
    width=m==DICAS_TOQUE?1080:1920; height=m==DICAS_TOQUE?1920:1080;
    layout_modo_definir(m==DICAS_TOQUE); layout_tela_definir(width,height);
    gfx_tamanho_alvo(width,height); gfx_snap_encerrar(); assert(gfx_snap_iniciar(width,height));
    glBindTexture(GL_TEXTURE_2D,tex); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    gfx_tex_esquecer(0);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    if (m!=DICAS_TOQUE) {   // a tecla ACESA e a que a pagina cita, e so ela
      static const char *const quer[]={"CH+","CH-","CH+","OK"};
      valor[AJ_IDIOMA]=IDIOMA_PT+1; dicascontrole_abrir();
      for (int p=0;p<4;p++) {
        if (p) { key(SDLK_RIGHT,dicascontrole_evento); }
        for(int i=0;i<30;i++) frame();
        const char *rot[8]; int on[8]; dicascontrole_teste_gravar(1); frame();
        int n=dicascontrole_teste_teclas(rot,on,8); dicascontrole_teste_gravar(0);
        int acesas=0; assert(n>=3 && dicascontrole_pagina()==p);
        for(int i=0;i<n;i++) if(on[i]) { acesas++; assert(!strcmp(rot[i],quer[p])); }
        assert(acesas==1);
      }
      key(SDLK_ESCAPE,dicascontrole_evento); frame(); settle();
    }
    for (int idi=0;idi<IDIOMA_N;idi++) {
      valor[AJ_IDIOMA]=idi+1; assert(ajustes_idioma()==idi);
      dicascontrole_abrir();
      for (int p=0;p<4;p++) {
        assert(dicascontrole_pagina()==p);   // o avanco sozinho ja entregou a proxima
        int segurar = m==DICAS_TOQUE ? (p==1||p==2) : (p>=2);
        int n = (int)((segurar?6.0f:5.4f)*60);   // mesma duracao de dicascontrole.c
        int iAberto = segurar?174:150;
        // CICLO DE VIDA DO COMPONENTE: abre pelo caminho real, fecha, nada sobra.
        int compAbriu = 0;
        for (int f=0; f<n && dicascontrole_aberto(); f++) {
          frame();
          if (spainel_aberto()||spot_aberto()||central_aberta()||ctx_aberto()||menu_aberto()) compAbriu = 1;
          if (idi!=0 && idi!=IDIOMA_DE) continue;
          char nome[160];
          if (f==72) { snprintf(nome,sizeof nome,"%s-%s-tip-%d-tecla",modes[m],idi==0?"pt":"de",p+1); capture(out,nome); }
          if (f==iAberto) { snprintf(nome,sizeof nome,"%s-%s-tip-%d-aberto",modes[m],idi==0?"pt":"de",p+1); capture(out,nome); }
          if (f==216) { snprintf(nome,sizeof nome,"%s-%s-tip-%d-legenda",modes[m],idi==0?"pt":"de",p+1); capture(out,nome); }
        }
        assert(compAbriu);
        assert(!spainel_aberto() && !spot_aberto() && !central_aberta() && !ctx_aberto() && !menu_aberto());
      }
      assert(!dicascontrole_aberto());   // a ultima dica conclui sozinha
      settle();
    }
  }
  puts("PASS: lit key matches page text in every remote");
  puts("PASS: components open and close cleanly in all modes and languages");
  touch();
  width=1920; height=1080; layout_tela_definir(width,height); gfx_tamanho_alvo(width,height);
  gfx_snap_encerrar(); assert(gfx_snap_iniciar(width,height));
  glBindTexture(GL_TEXTURE_2D,tex); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  gfx_tex_esquecer(0);
  // RECUO DE MOVIMENTO: quadro estatico, nada anima, e o ciclo fecha igual.
  dicascontrole_teste_modo(DICAS_TOQUE); dicascontrole_abrir(); valor[AJ_IDIOMA]=IDIOMA_PT+1; valor[AJ_ANIM]=1;
  for(int i=0;i<12;i++) frame(); capture(out,"touch-reduced-tip-1");
  key(SDLK_RIGHT,dicascontrole_evento); frame();
  assert(dicascontrole_pagina()==1); capture(out,"touch-reduced-tip-2");
  key(SDLK_ESCAPE,dicascontrole_evento); frame(); assert(!dicascontrole_visivel());
  puts("PASS: reduced motion settles immediately");
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w);
}
int main(int argc,char **argv) {
  setbuf(stdout,NULL); setbuf(stderr,NULL);
  assert(argc>=3); assert(SDL_Init(SDL_INIT_TIMER)==0);
  dados_iniciar(argv[1]); snprintf(dirAjustes,sizeof dirAjustes,"%s",argv[1]); ajustes_iniciar();
  if(!strcmp(argv[2],"shot")) { assert(argc==4); shots(argv[3]); }
  else state(!strcmp(argv[2],"first"));
  SDL_Quit(); return 0;
}
