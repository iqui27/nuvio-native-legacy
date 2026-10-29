/* Real renderer proof: populated/sparse facts and emoji in the review body. */
#define NV_TOUCH_UI
#define extras_titulo_original fixtureOriginal
#define extras_idioma_original fixtureLanguage
#define extras_ficha_lancamento fixtureDate
#define extras_ficha_duracao fixtureRuntime
#define seriefrases_n_fatos fixtureFacts
#define seriefrases_fato_rotulo fixtureLabel
#define seriefrases_fato_valor fixtureValue
#define extras_n_comentarios fixtureReviews
#define extras_comentario_texto fixtureReview
#define extras_comentario_usuario fixtureUser
#define extras_comentario_spoiler fixtureSpoiler
#ifndef NV_DETAIL_SOURCE
#define NV_DETAIL_SOURCE "../src/detail.c"
#endif
#include NV_DETAIL_SOURCE
#include "dados.h"
#include <assert.h>
#include <SDL2/SDL_image.h>

static int sparse, previewLines, measurements;
static const char *reviewText;
void fonte_addon_measure(void) { measurements++; }
void fonte_addon_box(int x, int y, int w, int h, int emoji) {
  (void)x; (void)y; (void)w; (void)h; (void)emoji;
}
void fonte_addon_line(const char *text, float y, float w, float h, float limit) {
  (void)text; (void)y; (void)w; (void)h; (void)limit;
  previewLines++;
}
void fonte_addon_visible(const char *text, float y, float h) {
  (void)y; (void)h;
  assert(!strstr(text,"HIDDEN"));
}
const char *fixtureOriginal(void) {
  return sparse ? "L'Âge de fer : une histoire de la Praça Charles de Gaulle, des souvenirs et des rencontres à travers plusieurs générations"
                : "La Bataille de Gaulle : L'Âge de fer";
}
const char *fixtureLanguage(void) { return sparse ? "" : "fr"; }
const char *fixtureDate(void) { return "2026-06-03"; }
int fixtureRuntime(void) { return sparse ? 0 : 160; }
int fixtureFacts(void) { return sparse ? 0 : 1; }
const char *fixtureLabel(int i) { (void)i; return "Filmado em"; }
const char *fixtureValue(int i) { (void)i; return "Praça Charles de Gaulle"; }
int fixtureReviews(void) { return 1; }
const char *fixtureReview(int i) {
  (void)i;
  if (reviewText) return reviewText;
  return "🇫🇷 Great Churchill character. L'Âge de fer, Praça Charles de Gaulle. "
         "A measured performance with a memorable ending. A longer review keeps wrapping "
         "inside the card without covering the next section.";
}
const char *fixtureUser(int i) { (void)i; return "pptt"; }
int fixtureSpoiler(int ep, int i) { (void)ep; (void)i; return 0; }

static int capture(const char *out, int phone, int fields) {
  int width=phone ? 1080 : 1920, height=phone ? 1920 : 1080;
  layout_tela_definir(width,height); layout_modo_definir(phone);
  gfx_tamanho_alvo(width,height); glViewport(0,0,width,height);
  sparse=fields==3;
  LinhaDet rows[NV_DETF_DET_MAXL];
  assert(montarDetalhes(rows,NV_DETF_DET_MAXL)==fields);
  assert(strstr(rows[0].valor,"L'Âge"));
  if (!sparse) assert(!strcmp(rows[5].valor,"Praça Charles de Gaulle"));
  foco.fileira=SEC_DETALHES; foco.coluna=0; nivel=1;
  conteudoSec[SEC_DETALHES]=phone ? 540 : 440; scrollY=0;
  for (int frame=0;frame<12;frame++) {
    txt_novo_quadro(); gfx_novo_quadro();
    glClearColor(13/255.f,13/255.f,13/255.f,1); glClear(GL_COLOR_BUFFER_BIT);
    desenhaComentarios(NV_DETP_X,80,1);
    desenhaRodape(SEC_DETALHES,1);
    char caps[100]; idioma_maiusc_em(ajustes_idioma(),caps,sizeof caps,i18n("Estúdios e produção"));
    txt_tracking(TXT_G20M,caps,243,242,239,NV_DETP_X,
                 conteudoSec[SEC_DETALHES]+alturaSecao(SEC_DETALHES)+80, .6f,3.6f);
  }
  SDL_Surface *shot=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
  unsigned char *pixels=malloc((size_t)width*height*4); assert(shot && pixels);
  glFinish(); glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  int color=0;
  for (int y=0;y<height;y++) {
    unsigned char *line=pixels+(size_t)(height-1-y)*width*4;
    memcpy((char *)shot->pixels+y*shot->pitch,line,(size_t)width*4);
    /* Only review body: excludes avatar, score and all other colored UI. */
    if (y>=212 && y<310) for (int x=120;x<628;x++) {
      unsigned char *p=line+x*4;
      if (abs(p[0]-p[1])>35 || abs(p[1]-p[2])>35) color++;
    }
  }
  char path[1024]; snprintf(path,sizeof path,"%s/%s-%d-fields.png",out,phone ? "phone" : "tv",fields);
  assert(!IMG_SavePNG(shot,path)); printf("PNG %s\n",path);
  free(pixels); SDL_FreeSurface(shot);
  assert(foco.fileira==SEC_DETALHES && foco.coluna==0);
  return color;
}

int main(int argc,char **argv) {
  assert(argc==2 && !SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  IMG_Init(IMG_INIT_PNG);
  SDL_Window *window=SDL_CreateWindow("Detail facts proof",0,0,1920,1920,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  assert(gfx_iniciar() && txt_iniciar("deploy/app",1));
  dados_iniciar("deploy/app/art"); ajustes_iniciar();
  CatItem item={0}; strcpy(item.tipo,"movie"); strcpy(item.imdb,"tt9000011");
  strcpy(item.titulo,"De Gaulle: Résistance"); cat_definir_tudo(&item,1,NULL,0);
  idx=0; aberto=1;
  /* Removing the preview limit must submit hidden lines and fail here. */
  char *longReview=malloc(1024*1024); assert(longReview);
  memset(longReview,'x',1024*1024-1); longReview[1024*1024-1]=0;
  memcpy(longReview,"🇫🇷 first\nsecond\nthird\nHIDDEN\n",sizeof "🇫🇷 first\nsecond\nthird\nHIDDEN\n"-1);
  reviewText=longReview;
  txt_novo_quadro(); gfx_novo_quadro(); previewLines=measurements=0;
  desenhaComentarios(NV_DETP_X,80,1);
  assert(previewLines==3 && measurements<2048);
  memset(longReview,'x',1024*1024-1);
  memcpy(longReview,"🇫🇷 ",9);
  txt_novo_quadro(); previewLines=measurements=0;
  desenhaComentarios(NV_DETP_X,80,1);
  assert(previewLines==3 && measurements<2048);
  reviewText=NULL; free(longReview);
  puts("PASS: long review layout and submission stop at three lines");
  int colors=0;
  for (int phone=0;phone<2;phone++) for (int fields=6;fields>=3;fields-=3)
    colors+=capture(argv[1],phone,fields);
  printf("%s: review body contains color emoji (%d colored pixels)\n",colors>60 ? "PASS" : "FAIL",colors);
  txt_encerrar(); SDL_DestroyWindow(window); SDL_Quit();
  return colors>60 ? 0 : 1;
}
