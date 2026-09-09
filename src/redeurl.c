#include "rede.h"
#include <stdio.h>
#include <string.h>

// SO ESTA FUNCAO, e num arquivo proprio de proposito.
//
// Ela e a redacao de credencial dos logs (ver rede.h) e nao toca em rede
// nenhuma: nao ha socket, nao ha curl, nao ha dlopen, nao ha estado. O resto de
// rede.c tem tudo isso.
//
// O QUE ISSO CONSERTA: tests/debrid.sh finge o transporte (define os seus
// proprios rede_postar_st e rede_baixar_st) e por isso NAO PODE linkar rede.c —
// os simbolos colidiriam. Enquanto esta funcao morava la, o teste simplesmente
// nao linkava, e a alternativa obvia — um stub no teste — seria a pior das
// saidas: o unico lugar onde debrid.c esconde o segmento /d/<chave>/ do link do
// Real-Debrid passaria a ser codigo que o teste NAO exercita, e uma regressao
// que vazasse a chave no log sairia verde.
//
// Quem linka src/*.c (Mac, ARM, Tizen — os tres fazem glob) nao percebe
// diferenca: e a mesma funcao, no mesmo cabecalho.
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  const char *e, *h;
  unsigned n;
  if (!dst || tam == 0) return "";
  dst[0] = 0;
  if (!url || !*url) return dst;
  e = strstr(url, "://");
  if (!e) { snprintf(dst, tam, "%.*s", (int)tam - 1, url); return dst; }
  h = e + 3;
  while (*h && *h != '/' && *h != '?' && *h != '#') h++;
  n = (unsigned)(h - url);
  if (n >= tam) n = tam - 1;
  memcpy(dst, url, n);
  dst[n] = 0;
  // O "/..." avisa que havia caminho: sem ele, um log com host nu parece um
  // pedido a raiz do servidor, que e uma leitura errada.
  if (*h && n + 4 < tam) { memcpy(dst + n, "/...", 4); dst[n + 4] = 0; }
  return dst;
}
