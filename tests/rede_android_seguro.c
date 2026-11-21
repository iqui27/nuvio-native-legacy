// Reserva segura do download verificado (F94): com a libcurl fora do ar, o
// download de artefato de release (rede_baixar_bin_seguro) segue a MESMA
// regra do caminho libcurl — salto a salto, so https, so host oficial, ate 5
// saltos — em vez do follow automatico do HttpURLConnection. Na base o
// download caia na reserva comum: o stub do transporte seguro nunca era
// chamado e a lista nao era conferida.
//
//   bash tests/rede_android_seguro.sh [--sanitize]
#define NV_ANDROID 1
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/rede.c"

#define MAX_SALTOS 10
// Roteiro do transporte seguro: pares status/corpo consumidos na ordem. No
// 3xx o "corpo" e o Location ABSOLUTO resolvido — o formato que o Java devolve
// ao C (a resolucao de relativo e do lado Kotlin, coberta pelo QA no aparelho).
static int scriptSt[MAX_SALTOS];
static const char *scriptCorpo[MAX_SALTOS];
static int scriptN, seguroCalls, androidCalls;
static char urlsVistas[MAX_SALTOS][2200];

char *android_http_seguro(const char *v, const char *u, const char *c, const char *b,
                          int p, int *st, int *retryAfter, long *n, char *erro, size_t ne) {
  int i = seguroCalls++;
  (void)c; (void)b; (void)p;
  assert(i < scriptN);
  assert(v && !strcmp(v, "GET"));
  snprintf(urlsVistas[i], sizeof urlsVistas[0], "%s", u);
  if (retryAfter) *retryAfter = 0;
  *st = scriptSt[i];
  if (*st <= 0) { snprintf(erro, ne, "transporte caiu"); return NULL; }
  *n = (long)strlen(scriptCorpo[i]);
  return strdup(scriptCorpo[i]);
}
// Reserva comum: o Java segue o 302 sozinho — basta devolver o que o roteiro
// der; contar as chamadas ja prova que o caminho verificado nao passa por ela.
char *android_http(const char *v, const char *u, const char *c, const char *b,
                   int p, int *st, int *retryAfter, long *n, char *erro, size_t ne) {
  (void)v; (void)u; (void)c; (void)b; (void)p; (void)erro; (void)ne;
  androidCalls++;
  if (retryAfter) *retryAfter = 0;
  *st = 200; *n = 2;
  return strdup("{}");
}
void android_etapa(const char *s) { (void)s; }
void android_quadro(void) {}

static void roteiro(int n, const int *sts, const char **corpos) {
  int i;
  scriptN = n;
  seguroCalls = androidCalls = 0;
  for (i = 0; i < n; i++) { scriptSt[i] = sts[i]; scriptCorpo[i] = corpos[i]; }
}

int main(void) {
  const char *oficial =
    "https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/Nuvio-9.9.9.apk";
  long nb = 0;
  char *r;
  pronto = -1;   // libcurl nunca sobe: toda reserva cai no host (viaAndroid*)

  // Caminho valido: um 302 para o CDN de assets e o binario no segundo salto.
  { int sts[] = { 302, 200 };
    const char *corpos[] = { "https://release-assets.githubusercontent.com/x", "BIN" };
    roteiro(2, sts, corpos); }
  r = rede_baixar_bin_seguro(oficial, 5, &nb);
  assert(r && !strcmp(r, "BIN") && nb == 3);
  assert(seguroCalls == 2 && !strcmp(urlsVistas[0], oficial));
  assert(!strcmp(urlsVistas[1], "https://release-assets.githubusercontent.com/x"));
  assert(!androidCalls);   // a reserva comum nao entra no caminho verificado
  free(r);

  // Recusas: cada 3xx fora da regra para o download com UMA chamada.
  { static const char *const ruins[] = {
      "http://github.com/x",                          // esquema em claro
      "https://cdn.example.test/x",                   // host fora da lista
      "https://github.com.evil.test/x",               // sufixo parecido
      "https://objects.githubusercontent.com.evil/x", // idem no CDN
      "https://user@github.com/x",                    // userinfo
      "https://user:senha@github.com/x",
      "/releases/download/v9.9.9/x",                  // relativo cru
      "https://github.com/final com espaco",          // F107: espaco resolvido
      "https://github.com/final\tcontinuacao",        // F107: dobra costurada
      "", NULL };                                     // 3xx sem Location
    int i;
    for (i = 0; ruins[i]; i++) {
      int sts[] = { 302 };
      const char *corpos[] = { ruins[i] };
      roteiro(1, sts, corpos);
      assert(rede_baixar_bin_seguro(oficial, 5, &nb) == NULL);
      assert(seguroCalls == 1 && !androidCalls);
    }
  }

  // Mais de 5 saltos: mesmo dentro da lista o sexto e recusado.
  { int sts[] = { 302, 302, 302, 302, 302, 302, 200 };
    const char *corpos[] = { "https://github.com/a", "https://github.com/b",
      "https://github.com/c", "https://github.com/d", "https://github.com/e",
      "https://github.com/f", "BIN" };
    roteiro(7, sts, corpos); }
  assert(rede_baixar_bin_seguro(oficial, 5, &nb) == NULL);
  assert(seguroCalls == 6);   // pedido inicial + 5 saltos aceitos

  // Cadeia valida de 5 saltos no limite: github -> objects -> github -> ...
  { int sts[] = { 302, 302, 302, 302, 302, 200 };
    const char *corpos[] = { "https://github.com/a",
      "https://objects.githubusercontent.com/b", "https://github.com:443/c",
      "https://release-assets.githubusercontent.com/d", "https://github.com/e",
      "BIN" };
    roteiro(6, sts, corpos); }
  r = rede_baixar_bin_seguro(oficial, 5, &nb);
  assert(r && !strcmp(r, "BIN") && seguroCalls == 6);
  free(r);

  // A URL inicial nao e https, tem credencial ou esta fora do prefixo: nenhum
  // salto e seguido — o 3xx e devolvido como falha, nao como proximo alvo.
  { int sts[] = { 302 };
    const char *corpos[] = { "https://github.com/x" };
    roteiro(1, sts, corpos); }
  assert(rede_baixar_bin_seguro("http://github.com/x", 5, &nb) == NULL && seguroCalls == 0);
  roteiro(1, (int[]){ 302 }, (const char *[]){ "https://github.com/x" });
  assert(rede_baixar_bin_seguro(
      "https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/x?token=fixture",
      5, &nb) == NULL && seguroCalls == 1);
  roteiro(1, (int[]){ 302 }, (const char *[]){ "https://github.com/x" });
  assert(rede_baixar_bin_seguro("https://cdn.example.test/app.apk", 5, &nb) == NULL &&
         seguroCalls == 1);
  // Fora do prefixo com resposta direta o download continua valendo (TLS
  // verificado pela pilha do sistema) — so o redirect e proibido.
  roteiro(1, (int[]){ 200 }, (const char *[]){ "BIN" });
  r = rede_baixar_bin_seguro("https://cdn.example.test/app.apk", 5, &nb);
  assert(r && !strcmp(r, "BIN") && seguroCalls == 1);
  free(r);

  // Erro de transporte e de HTTP no primeiro salto: falha explicita.
  roteiro(1, (int[]){ 0 }, (const char *[]){ "" });
  assert(rede_baixar_bin_seguro(oficial, 5, &nb) == NULL && seguroCalls == 1);
  roteiro(1, (int[]){ 404 }, (const char *[]){ "no" });
  assert(rede_baixar_bin_seguro(oficial, 5, &nb) == NULL && seguroCalls == 1);

  // O caminho NAO verificado segue igual: pedidos simples vao pela reserva
  // comum, que nao confere lista nenhuma.
  roteiro(0, NULL, NULL);
  { int st = 0;
    r = rede_baixar_st("https://api.example.test/dados", 5, NULL, &st);
    assert(r && st == 200);
    free(r);
    r = rede_baixar_bin("https://cdn.example.test/img.png", 5, &nb);
    assert(r && androidCalls == 2 && !seguroCalls);
    free(r);
  }

  puts("rede_android_seguro: ok");
  return 0;
}
