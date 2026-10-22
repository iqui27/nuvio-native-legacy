// Capitulos do MKV com URL comprida (Android/.tpk). O player guarda ate 4096
// bytes de URL (video_tpk.c urlAtual, video_android.c urlAtual), mas o
// capmkv.c tinha 512 no pedido e no cache e voltava CALADO acima disso: link
// assinado de debrid/CDN (token, expires, signature na query) perdia os
// capitulos sem uma linha no registro.
//
// Reaproveita o arquivo virtual e os dubles de tests/capmkv.c (o main dele
// fica renomeado). Casos:
//   1. URL de ~1500 bytes: o fio lateral le e publica os capitulos;
//   2. a mesma URL de novo: cache, nenhum pedido;
//   3. outra URL com os mesmos 1400 primeiros bytes: NAO e a mesma — pede de
//      novo (a chave compara a URL inteira, nao um prefixo).
//
//   bash tests/capmkv_url_longa.sh
#define main capmkv_main_original
#include "capmkv.c"
#undef main

static void urlLonga(char *d, size_t n, const char *fimToken) {
  size_t k;
  snprintf(d, n, "https://cdn-07.debrid.example.net/dl/0f3c9a/Serie.S01E03.1080p.mkv"
                 "?expires=1760000000&ip=203.0.113.9&token=");
  k = strlen(d);
  while (k < 1400 && k + 1 < n) { d[k] = (char)('a' + (k * 11) % 26); k++; }
  d[k] = 0;
  snprintf(d + k, n - k, "&signature=%s", fimToken);
}

static void esperaGravado(void) {
  for (int i = 0; i < 300 && !nGravado; i++) usleep(10000);
  usleep(50000);
}

int main(void) {
  static const Cap anime[] = { {0, "Prologue"}, {90, "Opening"}, {180, "Part A"},
                               {1260, "Ending"}, {1350, "Next Episode Preview"} };
  char a[1600], b[1600];
  int antes;
  capmkv_espera_inicial_ms = 0;
  urlLonga(a, sizeof a, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
  urlLonga(b, sizeof b, "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
  assert(strlen(a) > 1400 && strlen(a) < 4096 && strncmp(a, b, 1400) == 0 && strcmp(a, b));

  puts("capmkv: URL assinada de ~1500 bytes");
  montar(0, 5LL * 1024 * 1024 * 1024, anime, 5, 0, 1);
  pedidos = 0; chamadas = 0; nGravado = 0;
  capmkv_iniciar(a);
  esperaGravado();
  ok(pedidos > 0, "pediu o cabecalho (nao voltou calado pelo tamanho)");
  ok(nGravado == 3, "capitulos publicados no modulo de intro");
  ok(perto(capmkv_creditos(1400), 1260), "creditos = Ending");

  antes = pedidos;
  capmkv_iniciar(a);
  usleep(100000);
  ok(pedidos == antes, "mesma URL longa de novo: cache, nenhum pedido");
  ok(perto(capmkv_creditos(1400), 1260), "e os creditos voltam do cache");

  antes = pedidos; nGravado = 0;
  capmkv_iniciar(b);
  esperaGravado();
  ok(pedidos > antes, "mesmo prefixo, outra assinatura: pede de novo (chave inteira)");
  ok(nGravado == 3, "e publica os capitulos dela");

  printf("%s\n", falhas ? "FALHOU" : "capmkv_url_longa: ok");
  return falhas ? 1 : 0;
}
