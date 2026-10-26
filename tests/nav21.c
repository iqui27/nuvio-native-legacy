// Exercise production event routing and return ownership without login or network.
#include "../src/app.c"
#include "shot_arte.h"
#include "socialvis.h"
#include <assert.h>
#include "j4_proof.h"
#include <sys/stat.h>

static int failures;
static void check(const char *name, int ok) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
  failures += !ok;
}
static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  if (k==SDLK_s) e.key.keysym.scancode=NV_SCANCODE_BLUE;
  app_evento(&e); e.type = SDL_KEYUP; app_evento(&e);
}
static void step(int n) {
  for (int i=0;i<n;i++) {
    spainel_atualizar(.01f, SDL_GetTicks());
    detail_atualizar(.01f, SDL_GetTicks());
    voltarAoPainel();
  }
}
static void draw(void) {
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(12); gfx_novo_quadro();
  glClear(GL_COLOR_BUFFER_BIT);
  shot_arte_desenhar(0);
  spainel_desenhar(SDL_GetTicks());
}
static void settle(void) {
  for (int i=0;i<100;i++) { step(1); draw(); }
}
static void save(const char *dir, const char *name) {
  char path[1024]; snprintf(path,sizeof path,"%s/%s.png",dir,name);
  draw(); j4_save(path); printf("PNG: %s\n",path);
}
int main(void) {
  const char *out = "/Volumes/ExternalSSD/tmp/rev/diretor21/redesign/social/fixed";
  mkdir(out,0755);
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  dados_iniciar(getenv("NUVIO_DADOS")); ajustes_iniciar();
  j4_settings(0,0,2,0); ajustes_definir_vidro(1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *win=SDL_CreateWindow("Social navigation",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1));
  gfx_escala_ui_definir(1); gfx_escala_txt_definir(1);
  tex_iniciar(96);
  char *art = realpath("deploy/app/art", NULL); assert(art);
  gfx_icones_dir(art);
  char icon[1024]; snprintf(icon, sizeof icon, "%s/icones/aj_pencil.png", art);
  tex_obter_larg(icon, 26); free(art);
  GLuint fb,texture; glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glViewport(0,0,1920,1080);
  recomenda_iniciar(); salvos_iniciar();
  assert(home_iniciar("deploy/app/art"));
  static CatItem items[4]; static CatFileira row; static SvEvento feed[4];
  const char *names[]={"Gustavo","Marina","Carolina","Rafael"};
  const char *titles[]={"Fallout","Widow's Bay","The Prestige","One Battle After Another"};
  for(int i=0;i<4;i++) {
    snprintf(items[i].imdb,sizeof items[i].imdb,"tt900000%d",i);
    snprintf(items[i].tipo,sizeof items[i].tipo,"movie");
    items[i].nElenco=1; snprintf(items[i].elenco[0].nome,sizeof items[i].elenco[0].nome,"Actor");
    snprintf(items[i].logo,sizeof items[i].logo,"deploy/app/art/logo/00.png");
    snprintf(items[i].titulo,sizeof items[i].titulo,"%s",titles[i]);
    snprintf(items[i].poster,sizeof items[i].poster,"deploy/app/art/poster/%02d.jpg",i);
    snprintf(items[i].backdrop,sizeof items[i].backdrop,"deploy/app/art/%02d.jpg",i);
    snprintf(feed[i].imdb,sizeof feed[i].imdb,"%s",items[i].imdb);
    snprintf(feed[i].titulo,sizeof feed[i].titulo,"%s",titles[i]);
    snprintf(feed[i].tipo,sizeof feed[i].tipo,"movie");
    snprintf(feed[i].pessoaId,sizeof feed[i].pessoaId,"nuvio:%s",names[i]);
    snprintf(feed[i].pessoaNome,sizeof feed[i].pessoaNome,"%s",names[i]);
    snprintf(feed[i].pessoaAvatar,sizeof feed[i].pessoaAvatar,"deploy/app/art/elenco/09_0.jpg");
    snprintf(feed[i].poster,sizeof feed[i].poster,"%s",items[i].poster);
    snprintf(feed[i].arte,sizeof feed[i].arte,"%s",items[i].backdrop);
    feed[i].acao=i==1?SV_AGORA:i==2?SV_AVALIOU:i==3?SV_FIM:SV_MANDOU;
    feed[i].nota=i==2?90:0;feed[i].quando=time(NULL)-(i<2?i*900:86400*(i-1));
    tex_obter(feed[i].poster);tex_obter(feed[i].pessoaAvatar);
  }
  snprintf(row.chave,sizeof row.chave,"pop_movie"); row.n=4;
  cat_definir_tudo(items,4,&row,1); socialvis_definir_feed(feed,4); socialvis_atualizar();
  j4_art_ready();
  SDL_Delay(410); key(SDLK_s);
  check("blue key reaches production Social panel",spainel_aberto());
  step(100);
  for(int i=0;i<12;i++)key(SDLK_UP);
  key(SDLK_RIGHT);
  check("D-pad reaches Activity",spainel_aba_atual()==1);
  key(SDLK_DOWN);check("D-pad reaches first activity",spainel_foco_indice()==0);
  key(SDLK_DOWN);check("D-pad reaches second activity",spainel_foco_indice()==1);
  settle();save(out,"social-100-glass");
  unsigned char glass[4],solid[4];glReadPixels(1750,800,1,1,GL_RGBA,GL_UNSIGNED_BYTE,glass);
  ajustes_definir_vidro(0);settle();save(out,"social-100-solid");
  glReadPixels(1750,800,1,1,GL_RGBA,GL_UNSIGNED_BYTE,solid);
  check("glass retains backdrop unlike solid",abs(glass[0]-solid[0])+abs(glass[1]-solid[1])+abs(glass[2]-solid[2])>6);
  ajustes_definir_vidro(1);
  gfx_escala_txt_definir(.85f);settle();save(out,"social-85-glass");
  gfx_escala_txt_definir(1.3f);settle();save(out,"social-130-glass");
  gfx_escala_txt_definir(1);settle();
  int focus=spainel_foco_indice();
  int expected=cat_indice_por_imdb(socialvis_evento(focus)->imdb);
  key(SDLK_RETURN); abrirTituloDoPainel();
  check("OK opens focused title through production route",
        detail_aberto() && detail_indice()==expected && !spainel_aberto());
  step(100); key(SDLK_ESCAPE); step(100);
  check("Back restores panel tab and exact prior row",!detail_aberto() && spainel_aberto() && spainel_aba_atual()==1 && spainel_foco_indice()==focus);
  for(int i=0;i<5;i++)key(SDLK_UP);
  key(SDLK_RIGHT);check("D-pad reaches Friends",spainel_aba_atual()==2);
  key(SDLK_RIGHT);check("D-pad reaches Notices",spainel_aba_atual()==3);
  key(SDLK_RIGHT);check("existing Agenda remains reachable",spainel_aba_atual()==4);
  for(int i=0;i<4;i++)key(SDLK_LEFT);
  check("D-pad returns to Saved",spainel_aba_atual()==0);
  key(SDLK_RIGHT); key(SDLK_DOWN);settle();
  save(out,"social-focus");
  key(SDLK_ESCAPE);step(8);save(out,"social-exit-080");
  step(100);check("Back closes one layer",!spainel_aberto() && !spainel_visivel());
  spainel_abrir_de(48,24,260,64);step(8);
  check("clock island entry remains in motion",spainel_visivel());
  save(out,"social-entry-080");
  step(100);key(SDLK_ESCAPE);step(100);
  check("clock island transition finishes",!spainel_visivel());
  printf("nav21: %d failures\n",failures);
  return failures?1:0;
}
