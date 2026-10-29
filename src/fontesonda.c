#define _GNU_SOURCE   // strcasestr no glibc (LG/tpk)
#include "fontesonda.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

int fontesonda_erro_conexao(const char *t) {
  if (!t || !*t) return 0;
  return strcasestr(t, "ConnectionFailed") || strcasestr(t, "0xfe6c0026") ||
         strcasestr(t, "connect") || strcasestr(t, "timed out") ||
         strcasestr(t, "timeout") || strcasestr(t, "network") ||
         strcasestr(t, "unreachable") || strcasestr(t, "dns");
}

int fontesonda_causa(const char *t, int *http) {
  const char *h;
  long n = -1;
  int android;
  if (http) *http = 0;
  if (!t || !*t) return FS_CAUSA_NENHUMA;
  if (isdigit((unsigned char)*t)) n = strtol(t, NULL, 10);
  android = strcasestr(t, "player error") != NULL;
  if (strcasestr(t, "server error") && n >= 40400 && n <= 40599) {
    if (http) *http = (int)(n - 40000);
    return FS_CAUSA_HTTP;
  }
  if ((h = strcasestr(t, "http ")) && isdigit((unsigned char)h[5]) && isdigit((unsigned char)h[6]) &&
      isdigit((unsigned char)h[7]) && h[5] >= '4' && h[5] <= '5') {
    if (http) *http = (int)strtol(h + 5, NULL, 10);
    return FS_CAUSA_HTTP;
  }
  if (android && n >= 2004 && n <= 2006) {
    if (http) *http = n == 2005 ? 404 : n == 2006 ? 403 : 0;
    return FS_CAUSA_HTTP;
  }
  if ((android && (n == 2000 || n == 2001 || n == 2002 || n == 2008)) ||
      fontesonda_erro_conexao(t)) return FS_CAUSA_REDE;
  if ((android && n >= 3000 && n < 5000) || strcasestr(t, "unsupported") ||
      strcasestr(t, "codec") || strcasestr(t, "decoder") || strcasestr(t, "format"))
    return FS_CAUSA_FORMATO;
  return FS_CAUSA_NENHUMA;
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

void fontesonda_recusa(const char *url, const char *erroTexto, int sincrona) {
  char host[128]; int http = 0;
  int causa = fontesonda_causa(erroTexto, &http);
  const char *motivos[] = {"backend rejected source (no classified error)",
                          "HTTP refusal", "network/TLS/timeout", "format/decoder"};
  soHost(url, host, sizeof host);
  printf("[fonte] playback failure: reason=%s synchronous=%d http=%d host=%s\n",
         motivos[causa], sincrona, http, host[0] ? host : "-");
  fflush(stdout);
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
