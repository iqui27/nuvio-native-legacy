// SEEKR: o recorte do quadro dentro da folha (decode + copia das linhas), sem
// rede. A "folha" e tests/amostra.jpg (640x360 = 2x2 quadros de 320x180); o
// download e trocado por leitura do arquivo. Confere que o recorte de (320,180)
// e EXATAMENTE o quadrante de baixo a direita da imagem decodificada.
//
//   bash tests/seekr_recorte.sh
#include <stdio.h>
int perftv_enabled, perftv_uploads;
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/rede.h"
static char *lerArquivo(const char *url, int s, long *n);
#include "../src/seekr.c"
void gfx_tex_esquecer(GLuint t) { (void)t; }
// Ajustes nao entra nesta bancada: relogio de 12 h desligado (tests/guia_logos.c).
int ajustes_relogio_12h(void) { return 0; }
// Dubles: este teste nao consulta a API.
char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; if (st) *st = 0; return NULL; }
char *rede_baixar_st_retry(const char *u, int s, const char *const *c, int *st, int *retry) {
  if (retry) *retry = 0; return rede_baixar_st(u, s, c, st); }
char *rede_baixar_bin_medido_controle(const char *u, int s, const char *const *c,
                                     const RedeControle *controle, long *n, RedeMedida *medida) {
  (void)c; (void)controle; memset(medida, 0, sizeof *medida);
  char *bytes = lerArquivo(u, s, n); medida->status = bytes ? 200 : 0; return bytes;
}

static char *lerArquivo(const char *url, int s, long *n) {
  FILE *f = fopen(url, "rb"); char *b; long t;
  (void)s;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t); if (fread(b, 1, (size_t)t, f) != (size_t)t) { free(b); b = NULL; }
  fclose(f); *n = t; return b;
}

int main(void) {
  Recorte *r = calloc(1, sizeof *r);
  SDL_Surface *ref;
  long n; char *b = lerArquivo("tests/amostra.jpg", 0, &n);
  int y, difs = 0;
  ref = decodificar((unsigned char *)b, n); free(b);
  assert(ref && ref->w == 640 && ref->h == 360);
  snprintf(r->url, sizeof r->url, "tests/amostra.jpg");
  r->folha = 0; r->cue = 3; r->x = 320; r->y = 180; r->w = 320; r->h = 180;
  r->g = geracao;
  emVoo = 1;
  recortar(r);                    // sincrono: libera r
  assert(!emVoo);
  assert(prontos[0].px && prontos[0].w == 320 && prontos[0].h == 180 && prontos[0].cue == 3);
  for (y = 0; y < 180; y++)
    difs += memcmp(prontos[0].px + (size_t)y * 320 * 4,
                   (unsigned char *)ref->pixels + (size_t)(180 + y) * ref->pitch + 320 * 4,
                   320 * 4) != 0;
  assert(difs == 0);
  // A folha ficou guardada, decodificada e em JPEG.
  assert(folhaSup && folhaIdx == 0 && jpgBytes && jpgIdx == 0);
  // Ocioso solta a decodificada e mantem o JPEG.
  seekr_ocioso();
  assert(!folhaSup && jpgBytes);
  // Recorte fora da folha: nada pronto, sem estourar (e decodifica do JPEG).
  soltarProntos();
  r = calloc(1, sizeof *r);
  snprintf(r->url, sizeof r->url, "tests/amostra.jpg");
  r->folha = 0; r->x = 400; r->y = 0; r->w = 320; r->h = 180; r->g = geracao; emVoo = 1;
  recortar(r);
  assert(!prontos[0].px && !prontos[1].px && !emVoo);
  // Geracao velha (titulo trocado no meio): descartado.
  r = calloc(1, sizeof *r);
  snprintf(r->url, sizeof r->url, "tests/amostra.jpg");
  r->folha = 0; r->x = 0; r->y = 0; r->w = 320; r->h = 180; r->g = geracao - 1; emVoo = 1;
  recortar(r);
  assert(!prontos[0].px && !prontos[1].px);
  SDL_FreeSurface(ref);
  seekr_desligar();
  printf("seekr_recorte: ok\n");
  return 0;
}
