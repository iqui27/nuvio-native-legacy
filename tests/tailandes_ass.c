// #369 item 2: o libass recebe o NotoSansThai pela mesma leitura de pasta que o
// app usa (assrender_ler_pasta_fontes) e desenha o tailandes de uma trilha ASS
// com ele, em vez do .notdef da Inter. Sem provedor de sistema (Samsung/Android
// sem fontconfig). Uso: tailandes_ass <pasta de fontes>
#include "../src/assrender.h"
#include <ass/ass.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ASS_Library *lib;
static void cb(const char *nome, const void *d, size_t n, void *u) {
  (void)u; ass_add_font(lib, nome, (char *)d, (int)n);
}
static const char *DOC =
  "[Script Info]\nScriptType: v4.00+\nPlayResX: 1280\nPlayResY: 720\n\n"
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
  "Style: Default,Inter Display,48,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,2,0,2,20,20,20,1\n\n"
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
  "Dialogue: 0,0:00:00.00,0:00:09.00,Default,,0,0,0,,\xe0\xb8\xaa\xe0\xb8\xa7\xe0\xb8\xb1\xe0\xb8\xaa\xe0\xb8\x94\xe0\xb8\xb5\xe0\xb8\x84\xe0\xb8\xa3\xe0\xb8\xb1\xe0\xb8\x9a \xe0\xb8\xa0\xe0\xb8\xb2\xe0\xb8\xa9\xe0\xb8\xb2\xe0\xb9\x84\xe0\xb8\x97\xe0\xb8\xa2\n";

/* modo 0: sem a fonte tailandesa; 1: com a fonte, texto como veio (libass sem
 * fallback por glifo); 2: com a fonte e as corridas marcadas (o que o app faz). */
static unsigned long area(int modo, const char *pasta) {
  int comThai = modo > 0;
  ASS_Renderer *r; ASS_Track *t; ASS_Image *im; int mudou = 0; unsigned long a = 0;
  char c[700];
  lib = ass_library_init(); assert(lib);
  snprintf(c, sizeof c, "%s", pasta);
  { int n = assrender_ler_pasta_fontes(c, cb, NULL, NULL); assert(n > 0); }
  if (!comThai) { ass_clear_fonts(lib);
    { FILE *f; char *b; long n; snprintf(c, sizeof c, "%s/InterDisplay-Regular.ttf", pasta);
      f = fopen(c, "rb"); assert(f); fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
      b = malloc((size_t)n); assert(fread(b, 1, (size_t)n, f) == (size_t)n); fclose(f);
      ass_add_font(lib, "InterDisplay-Regular.ttf", b, (int)n); free(b); } }
  r = ass_renderer_init(lib); assert(r);
  ass_set_frame_size(r, 1280, 720); ass_set_storage_size(r, 1280, 720);
  ass_set_shaper(r, ASS_SHAPING_COMPLEX);
  ass_set_fonts(r, NULL, "Inter Display", ASS_FONTPROVIDER_NONE, NULL, 1);
  t = ass_read_memory(lib, (char *)DOC, strlen(DOC), "UTF-8"); assert(t);
  if (modo == 2) for (int i = 0; i < t->n_events; i++) {
    char *novo = assrender_marcar_tailandes(t->events[i].Text);
    if (novo) { free(t->events[i].Text); t->events[i].Text = novo; }
  }
  for (im = ass_render_frame(r, t, 1000, &mudou); im; im = im->next)
    a += (unsigned long)im->w * (unsigned long)im->h;
  ass_free_track(t); ass_renderer_done(r); ass_library_done(lib);
  return a;
}

int main(int argc, char **argv) {
  unsigned long a0, a1, a2;
  assert(argc == 2);
  { char c[700]; FILE *f; snprintf(c, sizeof c, "%s/NotoSansThai-Regular.ttf", argv[1]);
    f = fopen(c, "rb"); if (!f) { printf("FALHOU: %s nao existe\n", c); return 1; } fclose(f); }
  { char *m = assrender_marcar_tailandes("abc"); assert(m == NULL); }
  { char *m = assrender_marcar_tailandes("{\\an8}\xe0\xb8\x81 x"); assert(m && strstr(m, "{\\an8}{\\fnNoto Sans Thai}\xe0\xb8\x81{\\fn} x")); free(m); }
  a0 = area(0, argv[1]); a1 = area(1, argv[1]); a2 = area(2, argv[1]);
  printf("tailandes_ass: area sem fonte=%lu, com fonte sem marca=%lu, com fonte e marca=%lu\n", a0, a1, a2);
  if (a2 == a1 || a2 == a0 || a2 == 0) { puts("FALHOU: o libass nao desenhou o tailandes com o NotoSansThai"); return 1; }
  puts("tailandes_ass: PASS");
  return 0;
}
