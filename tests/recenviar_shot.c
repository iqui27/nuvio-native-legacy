// Visual fixtures only. Renders the real "Recommend to a friend" sheet without
// starting Social/network: the service calls recenviar.c makes are swapped for
// fakes below (same signatures), so the sheet is driven by real key events and
// recenviar_atualizar, step by step, like on the TV.
//
// RE_FONTE lets the same script photograph an older recenviar.c (before/after).
#define recomenda_ativo        fx_ativo
#define recomenda_contatos     fx_contatos
#define recomenda_meu_codigo   fx_codigo
#define recomenda_enviar       fx_enviar
#define recomenda_envio_estado fx_envio_estado
#define recomenda_envio_limpar fx_envio_limpar
#define recomenda_pedir_agora  fx_pedir_agora
#ifndef RE_FONTE
#define RE_FONTE "../src/recenviar.c"
#endif
#include RE_FONTE
#include "nvtmp.h"
#include "vidro_fundo.h"
#include "shot_arte.h"
#include "vidro_ilhas.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static RecContato fxC[REC_CONTATOS_MAX];
static int fxN, fxEstado;
static char fxCod[8] = "uv8scv";
int fx_ativo(void) { return 1; }
int fx_contatos(RecContato *s, int max) {
  int n = fxN < max ? fxN : max;
  memcpy(s, fxC, sizeof(RecContato) * (size_t)n);
  return n;
}
const char *fx_codigo(void) { return fxCod; }
int fx_enviar(const CatItem *c, const char *para, int m, const char *t) {
  (void)c; (void)para; (void)m; (void)t;
  fxEstado = REC_ENVIO_INDO;
  return 1;
}
int fx_envio_estado(void) { return fxEstado; }
void fx_envio_limpar(void) { fxEstado = REC_ENVIO_NADA; }
void fx_pedir_agora(void) {}

static void contato(const char *nome, const char *id, const char *trakt, const char *foto) {
  RecContato *c = &fxC[fxN++];
  memset(c, 0, sizeof *c);
  snprintf(c->nome, sizeof c->nome, "%s", nome);
  snprintf(c->id, sizeof c->id, "%s", id);
  if (trakt) { snprintf(c->ids[0], sizeof c->ids[0], "trakt:%s", trakt); c->nIds = 1; }
  if (foto) snprintf(c->avatar, sizeof c->avatar, "%s", foto);
}
static void tres(void) {
  fxN = 0;
  contato("Ana Souza", "trakt:anasouza", NULL, NULL);
  contato("Amigo #343", "nuvio:7f2c", NULL, NULL);
  contato("Pedro", "nuvio:91ab", NULL, "deploy/app/art/poster/05.jpg");
  contato("Kevin Lima", "nuvio:55de", "kevlima", NULL);
}

static Uint32 relogio = 100000;
extern void ajustes_teste_vidro_env(void);
static void quadros(int n) {
  int q;
  for (q = 0; q < n; q++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro();
    tex_bombear(6); gfx_novo_quadro();
    recenviar_atualizar(1.0f / 60.0f, relogio);
  }
}
static void captura(const char *dir, const char *nome, SDL_Window *win) {
  char path[800];
  int q, y;
  snprintf(path, sizeof path, "%s-%s.png", dir, nome);
  for (q = 0; q < 70; q++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro();
    tex_bombear(6); gfx_novo_quadro();
    recenviar_atualizar(1.0f / 60.0f, relogio);
    glClearColor(.03f,.03f,.035f,1); glClear(GL_COLOR_BUFFER_BIT);
    if (vidroFundoAtivo()) vidroFundoDesenhar(); else shot_arte_desenhar(0);
    recenviar_desenhar(relogio);
    if (q == 69) {
      unsigned char *pix = malloc(1920*1080*4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
      assert(pix && s); glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pix);
      for(y=0;y<1080;y++) memcpy((char*)s->pixels+y*s->pitch,pix+(1079-y)*1920*4,1920*4);
      assert(IMG_SavePNG(s,path)==0); SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("capture: %s (open=%d)\n", path, recenviar_aberto());
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  recenviar_evento(&e);
  quadros(2);
}
static void teclaN(SDL_Keycode k, int n) { while (n-- > 0) tecla(k); }

static CatItem fxItem;
static void abrir(void) {
  fxEstado = REC_ENVIO_NADA;
  assert(recenviar_abrir(&fxItem));
  quadros(3);
}

// NUVIO_VIDRO_ILHA=<dir>: stripes behind the open sheet, PNG + frost check.
static int ilhaTeste(SDL_Window *win) {
  ReGeo g;
  int q, ok;
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  tres(); abrir();
  for (q = 0; q < 70; q++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro();
    tex_bombear(6); gfx_novo_quadro();
    recenviar_atualizar(1.0f / 60.0f, relogio);
    ilhaCena();
    recenviar_desenhar(relogio);
    SDL_GL_SwapWindow(win);
    if (q < 69) continue;
  }
  medir(&g);
  printf("com_fonte=%d\n", gfx_vidro_com_fonte);
  ok = ilhaConfere("recenviar", 960 + (int)(g.w / 2) - 30, 440, 20, 200);
  ilhaPng(getenv("NUVIO_VIDRO_ILHA"), getenv("NUVIO_ILHA_NOME"));
  return ok ? 0 : 1;
}

int main(int argc,char **argv) {
  const char *dir=getenv("NUVIO_DADOS"), *out=argc>1?argv[1]:nvt("/nuvio-send");
  const char *idi=getenv("NUVIO_SHOT_IDIOMA");
  char path[700]; FILE *f; SDL_Window *win; SDL_GLContext gl;
  int i;
  assert(dir && *dir);
  snprintf(path,sizeof path,"%s/ajustes.txt",dir); f=fopen(path,"w"); assert(f);
  fprintf(f,"idioma %d\nselected_theme 2\nvidroLocal %d\n",idi?atoi(idi):0,
          getenv("NUVIO_SHOT_SOLIDO")?1:0); fclose(f);
  ajustes_dir(dir); ajustes_teste_vidro_env();
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0); IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  win=SDL_CreateWindow("Nuvio: Send modal",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(win);
  gl=SDL_GL_CreateContext(win);assert(gl);SDL_GL_SetSwapInterval(0);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  vidroFundoPreparar();
  memset(&fxItem, 0, sizeof fxItem);
  snprintf(fxItem.imdb,sizeof fxItem.imdb,"tt12637874"); snprintf(fxItem.tipo,sizeof fxItem.tipo,"series");
  snprintf(fxItem.titulo,sizeof fxItem.titulo,"Fallout");snprintf(fxItem.meta,sizeof fxItem.meta,"2024 · 56 min");
  snprintf(fxItem.logo,sizeof fxItem.logo,"deploy/app/art/logo/00.png");
  snprintf(fxItem.backdrop,sizeof fxItem.backdrop,"deploy/app/art/00.jpg");
  snprintf(fxItem.poster,sizeof fxItem.poster,"deploy/app/art/poster/00.jpg");
  if (getenv("NUVIO_VIDRO_ILHA")) return ilhaTeste(win);


  // 1-2. Open with friends; then a friend in focus.
  tres(); abrir();
  captura(out,"contacts-open",win);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  captura(out,"contacts-focus",win);
  // 3. Phrase step.
  tecla(SDLK_RETURN);
  captura(out,"message",win);
  // 4. Sending, 5. sent.
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  captura(out,"sending",win);
  fxEstado = REC_ENVIO_OK;
  captura(out,"sent",win);
  // 6. Send failure.
  abrir(); teclaN(SDLK_DOWN,2); tecla(SDLK_RETURN); tecla(SDLK_RETURN);
  fxEstado = REC_ENVIO_FALHA;
  captura(out,"error",win);
  // Back from the error goes one step back.
  tecla(SDLK_AC_BACK);
  captura(out,"error-back",win);
  // 7. No friends yet, 8. service never reached (no code).
  fxN = 0; abrir();
  captura(out,"empty",win);
  fxCod[0] = 0; abrir();
  captura(out,"offline",win);
  snprintf(fxCod, sizeof fxCod, "uv8scv");
  // 9. One friend.
  // (Without a backdrop: the poster fills the 16:9 art, cropped.)
  fxN = 0; contato("Ana Souza", "trakt:anasouza", NULL, NULL);
  fxItem.backdrop[0] = 0; abrir();
  captura(out,"one",win);
  snprintf(fxItem.backdrop,sizeof fxItem.backdrop,"deploy/app/art/00.jpg");
  // 10. Thirty friends, scrolled to the middle.
  { static const char *nomes[] = { "Alice", "Bruno", "Carla", "Diego", "Eduarda", "Felipe",
      "Gabriela", "Heitor", "Isabela", "João", "Karina", "Lucas", "Marina", "Nicolas",
      "Olívia", "Paulo", "Quésia", "Rafael", "Sofia", "Tiago", "Úrsula", "Vitor", "Wesley",
      "Xênia", "Yasmin", "Zeca", "Amanda", "Beatriz", "Caio", "Davi" };
    char id[32];
    fxN = 0;
    for (i = 0; i < 30; i++) { snprintf(id, sizeof id, "nuvio:%02d", i);
      contato(nomes[i], id, i % 3 ? NULL : "fixture", NULL); } }
  abrir(); teclaN(SDLK_DOWN, 14);
  captura(out,"many",win);
  // 11. Long names and nameless accounts.
  fxN = 0;
  contato("Maria Eduarda dos Santos Albuquerque Ferreira de Lima", "nuvio:a1", NULL, NULL);
  contato("Amigo #1024", "nuvio:a2", NULL, NULL);
  contato("Christopher-Alexander Montgomery", "trakt:christopheralexandermontgomery", NULL, NULL);
  contato("Amigo #77", "nuvio:a4", NULL, NULL);
  contato("Bia", "nuvio:a5", "bia_assiste_tudo_que_aparece", NULL);
  abrir(); tecla(SDLK_DOWN);
  captura(out,"long",win);
  // 12. Add a friend, from inside the send flow.
  tres(); abrir(); teclaN(SDLK_DOWN, fxN); tecla(SDLK_RETURN);
  captura(out,"friends-add",win);
  // Remove confirmation on the friends page.
  teclaN(SDLK_DOWN, 3); tecla(SDLK_RETURN);
  captura(out,"friends-remove",win);
  tecla(SDLK_AC_BACK); tecla(SDLK_AC_BACK);
  captura(out,"friends-back",win);
  // 13. Friends page from Social, at 130 % interface size.
  recenviar_abrir_amigos(); quadros(3);
  gfx_escala_ui_definir(1.3f);
  captura(out,"friends-zoom130",win);
  assert(gfx_escala()==1.0f);
  gfx_escala_ui_definir(1.0f);
  tex_encerrar();txt_encerrar();gfx_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(win);SDL_Quit();
  return 0;
}
