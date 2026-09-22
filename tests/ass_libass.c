#include <ass/ass.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { int n, minx, maxx, miny, maxy; unsigned long area; } Quadro;

static Quadro quadro(ASS_Renderer *r, ASS_Track *t, long long ms) {
  ASS_Image *im; int mudou = 0; Quadro q = {0, 100000, -100000, 100000, -100000, 0};
  im = ass_render_frame(r, t, ms, &mudou);
  for (; im; im = im->next) {
    q.n++;
    if (im->dst_x < q.minx) q.minx = im->dst_x;
    if (im->dst_x + im->w > q.maxx) q.maxx = im->dst_x + im->w;
    if (im->dst_y < q.miny) q.miny = im->dst_y;
    if (im->dst_y + im->h > q.maxy) q.maxy = im->dst_y + im->h;
    q.area += (unsigned long)im->w * (unsigned long)im->h;
  }
  return q;
}

int main(int argc, char **argv) {
  ASS_Library *lib; ASS_Renderer *r; ASS_Track *t;
  Quadro a, b, c;
  assert(argc == 2);
  lib = ass_library_init(); assert(lib);
  r = ass_renderer_init(lib); assert(r);
  ass_set_frame_size(r, 1280, 720); ass_set_storage_size(r, 1280, 720);
  ass_set_shaper(r, ASS_SHAPING_COMPLEX);
  ass_set_fonts(r, NULL, "Arial", ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
  t = ass_read_file(lib, argv[1], "UTF-8"); assert(t);
  a = quadro(r, t, 500); b = quadro(r, t, 2500); c = quadro(r, t, 4500);
  /* As seis linhas sobrepostas viram imagens libass; o vetor nao pode ser
   * vazio e a cena em movimento precisa mudar de caixa entre os instantes. */
  assert(a.n > 0 && b.n > 0 && c.n > 0);
  assert(a.area > 0 && b.area > 0 && c.area > 0);
  assert(a.minx != b.minx || a.maxx != b.maxx || a.miny != b.miny || a.maxy != b.maxy);
  printf("ass_libass: t0.5=%d t2.5=%d t4.5=%d area=%lu/%lu/%lu\n",
         a.n, b.n, c.n, a.area, b.area, c.area);
  ass_free_track(t); ass_renderer_done(r); ass_library_done(lib); return 0;
}
