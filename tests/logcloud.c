/* Send the log to the cloud (adapted from Matis94): the request body is the
 * gzipped snapshot itself (POST /v1/loglink on OUR worker, never an external
 * file host) and the answer is {"ok":1,"url":"https://..."}. */
#include "logcloud.h"
#include "qr.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  const unsigned char gz[] = { 0x1f, 0x8b, 0x08, 0x00, 0x00, 0x41, 0x00 };
  unsigned long n = 0;
  char *c = logcloud_corpo(gz, sizeof gz, "BOUND", &n);
  assert(c && n == sizeof gz);
  assert(!memcmp(c, gz, sizeof gz));            /* binary body kept, NULs included */
  free(c);
  assert(!logcloud_corpo(NULL, 4, "B", &n));
  c = logcloud_corpo(NULL, 0, "B", &n);
  assert(c && n == 0);
  free(c);

  char u[256];
  assert(logcloud_resposta_url("{\"ok\":1,\"url\":\"https://w.example/v1/loglink/ab12\",\"expira_s\":259200}", u, sizeof u)
         && !strcmp(u, "https://w.example/v1/loglink/ab12"));
  assert(logcloud_resposta_url("\n { \"url\" : \"http://w.test/v1/loglink/x\" }\r\n", u, sizeof u)
         && !strcmp(u, "http://w.test/v1/loglink/x"));
  assert(!logcloud_resposta_url("{\"erro\":\"nao autenticado\"}", u, sizeof u) && !u[0]);
  assert(!logcloud_resposta_url("javascript:alert(1)", u, sizeof u));
  assert(!logcloud_resposta_url("{\"url\":\"file:///tmp/x\"}", u, sizeof u));
  assert(!logcloud_resposta_url("{\"url\":\"https://\"}", u, sizeof u));
  assert(!logcloud_resposta_url("{\"url\":\"https://w.example/0123456789abcdef0123456789\"}", u, 16));
  assert(!logcloud_resposta_url(NULL, u, sizeof u));
  /* The card shows the link as QR: one that fits QR_MAX_BYTES is accepted,
   * one byte longer is rejected instead of "ready" without a QR. */
  { char body[512], q[QR_MAX_BYTES + 1]; int k = snprintf(body, sizeof body, "{\"url\":\"https://w.example/v1/loglink/");
    while (k < 8 + QR_MAX_BYTES) body[k++] = 'a';
    snprintf(body + k, sizeof body - (size_t)k, "\"}");
    assert(logcloud_resposta_url(body, q, sizeof q) && strlen(q) == QR_MAX_BYTES);
    memmove(body + k + 1, body + k, strlen(body + k) + 1); body[k] = 'a';
    assert(!logcloud_resposta_url(body, q, sizeof q) && !q[0]); }
  puts("PASS: log to the cloud (request body, worker answer)");
  return 0;
}
