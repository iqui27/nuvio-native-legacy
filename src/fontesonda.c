#define _GNU_SOURCE   // strcasestr no glibc (LG/tpk)
#include "fontesonda.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

int fontesonda_erro_conexao(const char *t) {
  if (!t || !*t) return 0;
  return strcasestr(t, "ConnectionFailed") || strcasestr(t, "0xfe6c0026") ||
         strcasestr(t, "connect") || strcasestr(t, "timed out") ||
         strcasestr(t, "timeout") || strcasestr(t, "network") ||
         strcasestr(t, "unreachable") || strcasestr(t, "dns");
}

// Host apenas: sem esquema, usuario, porta, caminho, query ou fragmento.
static void soHost(const char *url, char *h, unsigned tam) {
  const char *p, *fim, *a;
  unsigned n;
  h[0] = 0;
  if (!url) return;
  p = strstr(url, "://");
  p = p ? p + 3 : url;
  for (fim = p; *fim && *fim != '/' && *fim != '?' && *fim != '#'; fim++) {}
  for (a = p; a < fim; a++) if (*a == '@') p = a + 1;
  if (*p == '[') {                       // IPv6 literal
    const char *c = memchr(p, ']', (size_t)(fim - p));
    if (c) fim = c + 1;
  } else {
    const char *c = memchr(p, ':', (size_t)(fim - p));
    if (c) fim = c;
  }
  n = (unsigned)(fim - p);
  if (n > tam - 1) n = tam - 1;
  memcpy(h, p, n); h[n] = 0;
}

const char *fontesonda_linha(char *dst, unsigned tam, const char *url, int status,
                             int redir, const char *mime, unsigned ms) {
  char host[128], tipo[64], red[16];
  unsigned i, k = 0;
  if (!dst || !tam) return "";
  soHost(url, host, sizeof host);
  // O tipo e do servidor: so caracteres de um media type, para nao levar
  // espaco ou quebra de linha para o log.
  for (i = 0; mime && mime[i] && k < sizeof tipo - 1; i++)
    if (isalnum((unsigned char)mime[i]) || strchr("/.+-_", mime[i])) tipo[k++] = mime[i];
  tipo[k] = 0;
  if (redir < 0) snprintf(red, sizeof red, "?"); else snprintf(red, sizeof red, "%d", redir);
  snprintf(dst, tam,
           "[fonte] sonda apos recusa: status=%d redirecionamentos=%s host_final=%s tipo=%s tempo=%u %s",
           status, red, host[0] ? host : "-", tipo[0] ? tipo : "-", ms,
           status >= 200 && status < 300
             ? "o player recusou um link que responde (apontar para player/TLS/formato)"
             : status > 0 ? "o link tambem nao serve pelo nosso cliente"
                          : "sem resposta pelo nosso cliente");
  return dst;
}
