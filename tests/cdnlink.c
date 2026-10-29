// cdnlink.c without the network: which links go through the CDN, what counts
// as a CDN destination, the cache of what the source check resolved, the
// validity (2 h 30) and the resolve with a fake redirect.
#include <stdio.h>
#include <string.h>
#include "cdnlink.h"

static uint64_t session = 1;
static int switchDuringResolve;
uint64_t perfis_sessao_geracao(void) { return session; }
int perfis_sessao_atual(uint64_t g) { return g == session; }
int perfis_sessao_travar(uint64_t g) { return g == session; }
void perfis_sessao_destravar(void) {}

static const char *redirFinal = "https://nexus-176.neur.tb-cdn.st/dl/abc?token=x";
static int redirOk = 1;
int rede_url_final_tipo(const char *url, int segundos, const char *const *cab,
                        char *dst, unsigned tam, int *status, char *mime, unsigned mimeTam, long *corpo) {
  (void)url; (void)segundos; (void)mime; (void)mimeTam; (void)corpo;
  // the add-on headers arrive, the app's own pseudo header does not
  for (int i = 0; cab && cab[i]; i++) if (!strncmp(cab[i], "X-Nuvio-", 8)) return 0;
  if (status) *status = redirOk ? 206 : 0;
  if (!redirOk) { dst[0] = 0; return 0; }
  if (switchDuringResolve) session++;
  snprintf(dst, tam, "%s", redirFinal);
  return 1;
}

static unsigned agora = 1000;
static unsigned relogio(void) { return agora; }
static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }

int main(void) {
  char d[4096], h[128];
  unsigned idade = 0, ms = 0;
  const char *mf = "https://mediafusionfortheweebs.midnightignite.me/streaming_provider/abc/stream?x=1";
  cdnlink_relogio_teste(relogio);

  ok(cdnlink_origem_ok(mf), "add-on https link: yes");
  ok(!cdnlink_origem_ok("http://127.0.0.1:4000/v.ts"), "local converter/proxy: no");
  ok(!cdnlink_origem_ok("http://localhost/x.mkv"), "localhost: no");
  ok(!cdnlink_origem_ok("https://cdn.example/live/index.m3u8?t=1"), "HLS playlist: no");
  ok(!cdnlink_origem_ok("https://cdn.example/manifest.mpd"), "DASH manifest: no");
  ok(!cdnlink_origem_ok("magnet:?xt=urn"), "not http: no");

  ok(cdnlink_aceita(mf, redirFinal), "redirect to the TorBox CDN: accepted");
  ok(!cdnlink_aceita(mf, mf), "same link: nothing to gain");
  ok(!cdnlink_aceita(mf, "https://mediafusionfortheweebs.midnightignite.me/other"), "same host: nothing to gain");
  ok(!cdnlink_aceita(mf, "http://127.0.0.1:8/x"), "redirect to a local address: refused");

  ok(!strcmp(cdnlink_host("https://user:pw@nexus-176.neur.tb-cdn.st:443/dl?t=1", h, sizeof h), "nexus-176.neur.tb-cdn.st"),
     "host for logs: no credentials, port, path or token");

  cdnlink_lembrar(mf, redirFinal, NULL, session);
  ok(cdnlink_recente(mf, NULL, CDNLINK_REUSO_MS, d, sizeof d, &idade) && !strcmp(d, redirFinal) && idade == 0,
     "the source check's link is reused right away");
  agora += 59000;
  ok(cdnlink_recente(mf, NULL, CDNLINK_REUSO_MS, d, sizeof d, &idade) && idade == 59000, "still reused at 59 s");
  agora += 2000;
  ok(!cdnlink_recente(mf, NULL, CDNLINK_REUSO_MS, d, sizeof d, NULL), "not reused after 60 s (resolved again at open)");
  ok(!cdnlink_recente("https://other.test/x", NULL, CDNLINK_REUSO_MS, d, sizeof d, NULL), "other link: nothing");

  for (int i = 0; i < 12; i++) {   // the cache keeps the newest 8
    char o[64]; snprintf(o, sizeof o, "https://addon.test/%d", i);
    agora += 10; cdnlink_lembrar(o, redirFinal, NULL, session);
  }
  ok(cdnlink_recente("https://addon.test/11", NULL, 100000, d, sizeof d, NULL), "newest entry kept");
  ok(!cdnlink_recente("https://addon.test/0", NULL, 100000, d, sizeof d, NULL), "oldest entry dropped");

  ok(cdnlink_validade_ms() == 150u * 60u * 1000u, "validity: 2 h 30");

  cdnlink_zerar();
  ok(cdnlink_resolver(mf, "User-Agent: x\nX-Nuvio-Prebuffer: 1000", 6, d, sizeof d, &ms) == 1,
     "resolve does not send the app's own X-Nuvio- header (the fake refuses it)");
  ok(cdnlink_resolver(mf, "User-Agent: x", 6, d, sizeof d, &ms) && !strcmp(d, redirFinal), "resolve: the CDN address");
  ok(cdnlink_recente(mf, "User-Agent: x", CDNLINK_REUSO_MS, d, sizeof d, NULL), "resolve: remembered for the next open");
  ok(!cdnlink_recente(mf, "Authorization: B", CDNLINK_REUSO_MS, d, sizeof d, NULL), "different headers never reuse signed links");
  session++;
  ok(!cdnlink_recente(mf, "User-Agent: x", CDNLINK_REUSO_MS, d, sizeof d, NULL), "new account never reuses signed links");
  cdnlink_lembrar(mf, redirFinal, "User-Agent: x", session - 1);
  ok(!cdnlink_recente(mf, "User-Agent: x", CDNLINK_REUSO_MS, d, sizeof d, NULL), "stale source-check worker cannot publish");
  switchDuringResolve = 1;
  ok(!cdnlink_resolver(mf, "User-Agent: x", 6, d, sizeof d, &ms) && !d[0], "stale resolver result rejected");
  ok(!cdnlink_recente(mf, "User-Agent: x", CDNLINK_REUSO_MS, d, sizeof d, NULL), "stale resolver cannot populate new session cache");
  switchDuringResolve = 0;
  redirOk = 0;
  ok(!cdnlink_resolver("https://addon.test/z", NULL, 6, d, sizeof d, &ms) && !d[0], "resolve fails: nothing");
  redirOk = 1; redirFinal = "https://addon.test/z2";
  ok(!cdnlink_resolver("https://addon.test/z", NULL, 6, d, sizeof d, &ms), "no redirect to another host: nothing");

  if (falhas) { printf("cdnlink: %d falha(s)\n", falhas); return 1; }
  printf("cdnlink: ok\n");
  return 0;
}
