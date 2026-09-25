// FOLHA DE CONTATO da cor viva (tests/corviva_folha_shot.sh): cada titulo com o fundo, o logo e as cores que
// corviva_extrair tirou de cada um. Uso:
//   cv-folha saida.png titulo1 fundo1.jpg logo1.png [titulo2 fundo2 logo2 ...]
// logo pode ser "-".
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include "corviva.h"
char *dados_ler(const char *n) { (void)n; return 0; }
int dados_gravar_leve(const char *a, const char *b) { (void)a; (void)b; return 1; }

static TTF_Font *fonte, *fonteB;
static Uint32 cor(SDL_Surface *s, const float c[3]) {
  return SDL_MapRGB(s->format, (Uint8)(c[0] * 255 + .5f), (Uint8)(c[1] * 255 + .5f), (Uint8)(c[2] * 255 + .5f));
}
static void caixa(SDL_Surface *s, int x, int y, int w, int h, const float c[3]) {
  SDL_Rect r = { x, y, w, h }; SDL_FillRect(s, &r, cor(s, c));
}
static void texto(SDL_Surface *s, TTF_Font *f, const char *t, int x, int y, Uint8 v) {
  SDL_Color c = { v, v, v, 255 };
  SDL_Surface *g = TTF_RenderUTF8_Blended(f, t, c);
  SDL_Rect r = { x, y, 0, 0 };
  if (!g) return;
  SDL_BlitSurface(g, NULL, s, &r); SDL_FreeSurface(g);
}
static void degrade(SDL_Surface *s, int x, int y, int w, int h, const float g[3][3], int pilula) {
  int i, j;
  for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
    float tt = ((float)i / w) * 0.8f + ((float)j / h) * 0.2f, c[3];
    int k;
    if (pilula) {
      float r = h * 0.5f, cx = i < r ? r : (i > w - r ? w - r : i), dx = i - cx, dy = j - r;
      if (dx * dx + dy * dy > r * r) continue;
    }
    for (k = 0; k < 3; k++)
      c[k] = tt < 0.5f ? g[0][k] + (g[1][k] - g[0][k]) * (tt / 0.5f)
                       : g[1][k] + (g[2][k] - g[1][k]) * ((tt - 0.5f) / 0.5f);
    ((Uint32 *)((Uint8 *)s->pixels + (y + j) * s->pitch))[x + i] = cor(s, c);
  }
}
static void encaixar(SDL_Surface *dst, SDL_Surface *src, int x, int y, int w, int h) {
  float a = (float)src->w / src->h;
  int ww = w, hh = (int)(w / a);
  if (hh > h) { hh = h; ww = (int)(h * a); }
  { SDL_Rect r = { x + (w - ww) / 2, y + (h - hh) / 2, ww, hh }; SDL_BlitScaled(src, NULL, dst, &r); }
}
static void hex(char *d, const float c[3]) {
  sprintf(d, "#%02x%02x%02x", (int)(c[0] * 255 + .5f), (int)(c[1] * 255 + .5f), (int)(c[2] * 255 + .5f));
}

int main(int argc, char **argv) {
  int n = (argc - 2) / 3, i, H = 150;
  SDL_Surface *folha;
  static const float escuro[3] = { 0.051f, 0.051f, 0.051f }, cinza[3] = { 0.16f, 0.16f, 0.17f };
  SDL_Init(0); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG); TTF_Init();
  fonte = TTF_OpenFont("deploy/app/fonts/InterDisplay-Regular.ttf", 15);
  fonteB = TTF_OpenFont("deploy/app/fonts/InterDisplay-Bold.ttf", 20);
  folha = SDL_CreateRGBSurfaceWithFormat(0, 1500, 40 + n * H, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_FillRect(folha, NULL, cor(folha, escuro));
  texto(folha, fonte, "fundo | logo | ARTE: destaque, degrade, luzes E/D/T/B | LOGO: destaque, degrade | FINAL (cor da logo ligada)", 10, 10, 200);
  printf("%-24s %-9s %-9s %-26s %-9s %-26s\n", "titulo", "arte", "base", "degrade arte", "logo", "degrade logo");
  for (i = 0; i < n; i++) {
    const char *nome = argv[2 + i * 3], *fb = argv[3 + i * 3], *fl = argv[4 + i * 3];
    int y = 40 + i * H;
    CorvivaPaleta pa, pl;
    SDL_Surface *b = IMG_Load(fb), *l = strcmp(fl, "-") ? IMG_Load(fl) : NULL;
    const CorvivaPaleta *fim;
    char h1[16], h2[16], h3[16], g1[3][16], g2[3][16];
    memset(&pl, 0, sizeof pl);
    if (!b) { fprintf(stderr, "sem %s\n", fb); continue; }
    { SDL_Surface *t = SDL_ConvertSurfaceFormat(b, SDL_PIXELFORMAT_ABGR8888, 0);
      corviva_extrair(t->pixels, t->w, t->h, t->pitch, &pa); SDL_FreeSurface(t); }
    if (l) { SDL_Surface *t = SDL_ConvertSurfaceFormat(l, SDL_PIXELFORMAT_ABGR8888, 0);
      corviva_extrair(t->pixels, t->w, t->h, t->pitch, &pl); SDL_FreeSurface(t); }
    encaixar(folha, b, 10, y + 5, 240, 135);
    caixa(folha, 260, y + 5, 230, 135, cinza);
    if (l) encaixar(folha, l, 265, y + 10, 220, 125);
    // ARTE
    caixa(folha, 505, y + 10, 60, 60, pa.ok ? pa.acento : escuro);
    degrade(folha, 575, y + 10, 150, 60, (const float (*)[3])pa.grad, 0);
    { int k; for (k = 0; k < 4; k++) caixa(folha, 505 + k * 56, y + 80, 50, 26, pa.regiao[k]); }
    caixa(folha, 505, y + 112, 220, 22, pa.base);
    // LOGO
    if (l) {
      caixa(folha, 745, y + 10, 60, 60, pl.ok ? pl.acento : escuro);
      degrade(folha, 815, y + 10, 150, 60, (const float (*)[3])pl.grad, 0);
      texto(folha, fonte, pl.ok ? (pl.transparente ? "logo (transp.)" : "logo") : "logo sem cor", 745, y + 80, 170);
    }
    // FINAL
    fim = (l && pl.ok) ? &pl : &pa;
    caixa(folha, 985, y + 5, 500, 135, pa.base);
    if (fim->ok) {
      degrade(folha, 1005, y + 25, 200, 64, (const float (*)[3])fim->grad, 1);
      { SDL_Rect r = { 1215, y + 25, 200, 64 };
        SDL_FillRect(folha, &r, cor(folha, fim->acento)); }
      texto(folha, fonteB, "Reproduzir", 1050, y + 43, 255);
      texto(folha, fonteB, "Reproduzir", 1262, y + 43, 255);
    } else texto(folha, fonteB, "(padrao: branco)", 1010, y + 43, 220);
    texto(folha, fonte, nome, 1005, y + 104, 230);
    hex(h1, pa.acento); hex(h2, pa.base); hex(h3, pl.acento);
    { int k; for (k = 0; k < 3; k++) { hex(g1[k], pa.grad[k]); hex(g2[k], pl.grad[k]); } }
    printf("%-24s %-9s %-9s %s %s %s %-9s %s %s %s%s\n", nome, pa.ok ? h1 : "(sem)", h2,
           g1[0], g1[1], g1[2], (l && pl.ok) ? h3 : "(sem)", g2[0], g2[1], g2[2],
           l ? "" : "");
    SDL_FreeSurface(b); if (l) SDL_FreeSurface(l);
  }
  IMG_SavePNG(folha, argv[1]);
  printf("folha: %s\n", argv[1]);
  return 0;
}
