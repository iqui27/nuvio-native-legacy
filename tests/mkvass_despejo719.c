#include "../src/mkvass.c"
#include <assert.h>

int main(void) {
  const char *doc = "[Script Info]\nTitle: test\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
    "Dialogue: 0,0:00:10.00,0:00:20.00,A\n"
    "Dialogue: 0,0:00:12.00,0:01:20.00,B\n"
    "Dialogue: 0,0:00:55.00,0:00:58.00,C\n";
  Ponto p[3] = {{.tempo=10000,.colhido=1}, {.tempo=12004,.colhido=1}, {.tempo=55000,.colhido=1}};
  Fio f = {0}; size_t antes;
  f.escala = 1000000; f.pontos = p; f.nPontos = f.nColhidos = 3;
  f.corpo = strdup(doc); assert(f.corpo); f.corpoTam = strlen(doc); antes = f.corpoTam; f.janela = 1;
  despejar(&f, 60.0);
  assert(strstr(f.corpo, ",B\n"));
  assert(!strstr(f.corpo, ",A\n"));
  assert(strstr(f.corpo, ",C\n"));
  assert(strstr(f.corpo, ",B\n") < strstr(f.corpo, ",C\n"));
  assert(f.corpoTam < antes && p[1].colhido == 1 && p[0].colhido == 0);
  despejar(&f, 15.0);
  assert(p[1].colhido == 1 && strstr(f.corpo, ",B\n"));
  assert(!strstr(strstr(f.corpo, ",B\n") + 1, ",B\n"));
  despejar(&f, 90.0);
  assert(!strstr(f.corpo, ",B\n") && p[1].colhido == 0);
  free(f.corpo);
  return 0;
}
