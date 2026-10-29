// Reuse the native Hero A fixture; exercise the actual shared hero action path.
#include "gfx.h"
#include "botoes.h"
#include <assert.h>
static int sheets, focused;
static GfxRect action;
static void frostSheet(GfxRect r, float radius, float alpha) {
  sheets++; action = r;
  gfx_vidro_folha(r, radius, alpha);
}
static int focusSurface(GfxRect r, float focus, float alpha) {
  focused++; action = r;
  return botao_superficie(r, focus, alpha);
}
#define gfx_vidro_folha frostSheet
#define botao_superficie focusSurface
#define main heroFixtureMain
#include "heroisA.c"
#undef main
#undef gfx_vidro_folha
#undef botao_superficie

static int failures;
static void check(int ok, const char *message) {
  if (!ok) { fprintf(stderr, "FAIL %s\n", message); failures++; }
}
static void save(const char *prefix, const char *layout, int glass, int scale, int focus) {
  char path[1200];
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
  unsigned char *pixels = malloc(1920*1080*4); assert(s && pixels);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for (int y=0;y<1080;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
  snprintf(path,sizeof path,"%s-%s-glass%d-s%d-focus%d.png",prefix,layout,glass,scale,focus);
  assert(IMG_SavePNG(s,path)==0); SDL_FreeSurface(s); free(pixels);
  printf("PNG %s\n",path);
}
int main(int argc, char **argv) {
  // --card initializes the real renderer, local artwork and catalog once.
  char *init[] = {argv[0], "--card", NULL};
  assert(heroFixtureMain(2,init)==0);
  const char *names[] = {"moderna","padrao","dinamica","split"};
  for (int layout=0;layout<4;layout++) for (int glass=0;glass<2;glass++)
    for (int scale=85;scale<=100;scale+=15) for (int focus=0;focus<2;focus++) {
      setup(layout==3,glass);
      char path[900]; snprintf(path,sizeof path,"%s/ajustes.txt",getenv("NUVIO_DADOS"));
      FILE *f=fopen(path,"w"); assert(f);
      fprintf(f,"idioma 1\nhomeLayoutLocal %d\nvidroLocal %d\nvidroFoscoLocal 1\nvidroQualidadeLocal 2\nselected_theme 2\ntrailerHero 1\n",
              layout==3 ? 2 : layout,glass ? 0 : 1);
      fclose(f); ajustes_dir(getenv("NUVIO_DADOS"));
      check(ajustes_vidro()==glass,"requested glass state");
      gfx_escala_txt_definir(scale/100.0f); TXT_INI();
      focoHero=focus;
      // Moderna only shows its action while the hero is focused or returning.
      // Freeze that visible return frame to check the material path too.
      scrollY=-empurraHero();
      heroAtual=heroAnterior=0; heroEntra=heroDesliza=1; heroSai=0;
      heroDesejado=-1; heroCinema.v=0; detailProgress=0;
      gfx_vidro_sujar();
      for (int frame=0;frame<48;frame++) {
        tex_bombear(24); tex_novo_quadro(); txt_novo_quadro(); gfx_novo_quadro();
        glClearColor(NV_COR_FUNDO_R,NV_COR_FUNDO_G,NV_COR_FUNDO_B,1);
        glClear(GL_COLOR_BUFFER_BIT);
        sheets=focused=0;
        home_desenhar(1000);
        if (frame==47) {
          int accent=layout==0 || focus;
          int hidden=layout==3 && !focus;
          check(sheets==(!accent && !hidden),"resting hero uses shared frost sheet in both glass states");
          check(focused==(accent && !hidden),"focused hero keeps shared accent fill");
          if (!accent && !hidden) check(gfx_vidro_com_fonte>0,"resting hero samples frost texture");
          check(gfx_vidro_capturas==0 && gfx_vidro_passadas==0,"settled hero reuses cache without blur passes");
          if (argc>1) save(argv[1],names[layout],glass,scale,focus);
          printf("MATERIAL %s glass=%d scale=%d focus=%d sheet=%d accent=%d captures=%d passes=%d\n",
                 names[layout],glass,scale,focus,sheets,focused,gfx_vidro_capturas,gfx_vidro_passadas);
        }
      }
      TXT_FIM();
    }
  return failures ? 1 : 0;
}
