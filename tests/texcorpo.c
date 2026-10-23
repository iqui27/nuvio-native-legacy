// 0 B COMO "SUCESSO" NAO E ARTE (#205, TCL Android).
//
// O caminho medido de imagem (rede_baixar_bin_medido_controle, tex_cache)
// aceitava um 200 com corpo vazio como download: nao ha assinatura para
// recusar, e a falha nunca era guardada — a mesma URL voltava a cada item
// para sempre, fileira inteira sem arte ("[tex] corpo curto (0 B)" no log,
// repetido a cada poucos segundos). Aqui o servidor local responde 200 com
// corpo vazio: o pedido tem de falhar E entrar em recuo (o segundo nao chega
// ao servidor), sem bloquear a vizinha boa.
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/sdlcompat.h"
#include "../src/tex_cache.c"   // sem duble: o caminho medido (md.status) do aparelho

static int batidas(const char *p) {
  FILE *f = fopen(p, "r");
  int n = 0, c;
  if (!f) return 0;
  while ((c = getc(f)) != EOF) n += (c == '\n');
  fclose(f);
  return n;
}

int main(int argc, char **argv) {
  char url[256];
  long n = -1;
  char *b;
  assert(argc == 3);
  snprintf(url, sizeof url, "http://127.0.0.1:%s/vazia200", argv[1]);
  assert(!baixarImagem(url, &n, NULL));   // 200 vazio: falha, nao e arte
  assert(!baixarImagem(url, &n, NULL));   // recuo: o 2o pedido nao refetcha
  assert(batidas(argv[2]) == 1);         // servidor visto UMA vez so
  puts("ok: 200 com corpo vazio falha e entra em recuo (uma batida so)");
  snprintf(url, sizeof url, "http://127.0.0.1:%s/boa.jpg", argv[1]);
  b = baixarImagem(url, &n, NULL);
  assert(b && n == 1024 && !memcmp(b, "\xff\xd8", 2));
  free(b);
  puts("ok: vizinha boa baixa normal, recuo nao vaza");
  return 0;
}