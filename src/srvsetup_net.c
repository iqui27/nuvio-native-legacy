// Personal-server onboarding protocols. Never log response bodies or addresses.
#include "srvsetup.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

static void wipe(void *p, size_t n) { volatile unsigned char *v = p; while (n--) *v++ = 0; }
static int address(const char *s, char *out, size_t n) {
  return s && !strchr(s, '@') && jf_url_normalizar(s, out, n) == JF_OK;
}
static void header(const char *body, const char *key, char *out, size_t n) {
  size_t k = strlen(key); out[0] = 0;
  for (const char *p = body; p && *p; p = strchr(p, '\n')) {
    if (*p == '\n') p++;
    if (!strncasecmp(p, key, k) && p[k] == ':') {
      p += k + 1; while (*p == ' ') p++;
      size_t z = strcspn(p, "\r\n"); if (z >= n) z = n - 1;
      memcpy(out, p, z); out[z] = 0; return;
    }
    if (!strchr(p, '\n')) break;
  }
}
int srv_ler_descoberta(int provider, const char *body, SrvRede *out) {
  SrvRede s = {0};
  if (!body || !out) return 0;
  if (provider == 0) {
    char port[12];
    header(body, "Name", s.nome, sizeof s.nome);
    header(body, "Resource-Identifier", s.id, sizeof s.id);
    header(body, "Port", port, sizeof port);
    char *end; long v = strtol(port, &end, 10);
    if (!s.nome[0] || !s.id[0] || *end || v < 1 || v > 65535) return 0;
    snprintf(s.url, sizeof s.url, "%ld", v); // sender address added by receiver
  } else {
    char raw[512] = "";
    js_texto_raiz(body, "Name", s.nome, sizeof s.nome);
    js_texto_raiz(body, "Id", s.id, sizeof s.id);
    if (!js_texto_raiz(body, "Address", raw, sizeof raw) || !address(raw, s.url, sizeof s.url)) return 0;
  }
  *out = s; return 1;
}
static unsigned long long now(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
int srv_descobrir(int provider, RedeJob *job, SrvRede *out, int max) {
#ifdef __EMSCRIPTEN__
  (void)provider; (void)job; (void)out; (void)max; return 0;
#else
  int fd = socket(AF_INET, SOCK_DGRAM, 0), n = 0, yes = 1;
  if (fd < 0 || !out || max < 1) { if (fd >= 0) close(fd); return 0; }
  setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof yes);
  struct sockaddr_in dst = {0}; dst.sin_family = AF_INET;
  dst.sin_port = htons(provider == 0 ? 32414 : 7359);
  inet_pton(AF_INET, provider == 0 ? "239.0.0.250" : "255.255.255.255", &dst.sin_addr);
  const char *msg = provider == 0 ? "M-SEARCH * HTTP/1.0\r\n\r\n" : provider == 1 ? "who is JellyfinServer?" : "who is EmbyServer?";
  sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&dst, sizeof dst);
  unsigned long long until = now() + 2500;
  while (now() < until && n < max && (!job || rede_job_estado(job) == REDE_OK)) {
    fd_set set; FD_ZERO(&set); FD_SET(fd, &set); struct timeval tv = {0, 100000};
    if (select(fd + 1, &set, NULL, NULL, &tv) <= 0) continue;
    char body[4096]; struct sockaddr_in src; socklen_t len = sizeof src;
    ssize_t z = recvfrom(fd, body, sizeof body - 1, 0, (struct sockaddr *)&src, &len);
    if (z <= 0) continue; body[z] = 0;
    SrvRede s;
    if (!srv_ler_descoberta(provider, body, &s)) continue;
    if (!provider) {
      char ip[INET_ADDRSTRLEN], port[12]; snprintf(port, sizeof port, "%s", s.url);
      inet_ntop(AF_INET, &src.sin_addr, ip, sizeof ip);
      snprintf(s.url, sizeof s.url, "http://%s:%s", ip, port);
    }
    int i; for (i = 0; i < n && strcmp(out[i].url, s.url); i++) {}
    if (i == n) out[n++] = s;
  }
  close(fd); return n;
#endif
}

static int request(const char *url, const char *body, const char *const *headers, RedeJob *job, int personal, RedeResposta *r) {
  RedePedido p = {0}; p.url = url; p.metodo = body ? "POST" : "GET";
  p.corpo = body; p.n_corpo = body ? strlen(body) : 0; p.cabecalhos = headers;
  p.prazo_ms = 10000; p.max_bytes = 512 * 1024; p.job = job;
  p.seguir = 1; p.mesmo_host = 1; p.tls_pessoal = personal;
  memset(r, 0, sizeof *r); rede_pedir(&p, r);
  int e = r->erro == REDE_CANCELADO || r->erro == REDE_GERACAO ? JF_ERR_CANCELADO :
    r->erro != REDE_OK ? JF_ERR_REDE : r->status == 401 || r->status == 403 ? JF_ERR_AUTH :
    r->status < 200 || r->status >= 300 ? JF_ERR_HTTP : JF_OK;
  if (e) { if (r->corpo) wipe(r->corpo, r->n_corpo); rede_resposta_limpar(r); }
  return e;
}
int srv_usuarios(const JfConta *c, RedeJob *job, SrvUsuario *out, int max) {
  char url[640], auth[512]; RedeResposta r; int n = 0;
  jf_cabecalho_auth(c, auth, sizeof auth);
  const char *headers[] = { "Accept: application/json", auth, NULL };
  snprintf(url, sizeof url, "%s%s/Users/Public", c->base, jf_prefixo(c));
  int e = request(url, NULL, headers, job, 1, &r); wipe(auth, sizeof auth);
  if (e) return e;
  for (const char *p = js_raiz_array(r.corpo); p && n < max;) {
    const char *end = js_fim(p); if (!end) break;
    SrvUsuario u = {0};
    js_texto(p, end, "Name", u.nome, sizeof u.nome); js_texto(p, end, "Id", u.id, sizeof u.id);
    char id[40];
    if (u.nome[0] && jfid_hex_limpo(u.id, id, 32)) {
      snprintf(u.imagem, sizeof u.imagem, "%s%s/Users/%s/Images/Primary?maxWidth=128", c->base, jf_prefixo(c), u.id);
      out[n++] = u;
    }
    p = js_prox(end);
  }
  rede_resposta_limpar(&r); return n;
}
int em_connect_pedir(const char *path, const char *body, const char *token, RedeJob *job, RedeResposta *r) {
  char url[1024], auth[256];
  snprintf(url, sizeof url, "https://connect.emby.media/service/%s", path);
  snprintf(auth, sizeof auth, "X-Connect-UserToken: %s", token ? token : "");
  const char *h[] = {"Content-Type: application/json", "Accept: application/json", "X-Application: Nuvio/2.1", auth, NULL};
  int e = request(url, body, h, job, 0, r); wipe(auth, sizeof auth); return e;
}
int em_connect_ler_conta(const char *body, char *token, size_t nt, char *user, size_t nu) {
  token[0] = user[0] = 0;
  if (body) {
    if (!js_texto_raiz(body, "ConnectAccessToken", token, nt)) js_texto_raiz(body, "AccessToken", token, nt);
    if (!js_texto_raiz(body, "ConnectUserId", user, nu) && !js_texto_raiz(body, "UserId", user, nu)) {
      char object[1024];
      if (js_bruto(body, NULL, "User", object, sizeof object)) js_texto_raiz(object, "Id", user, nu);
    }
  }
  if (!token[0] || !user[0] || strpbrk(token, "\r\n") || strspn(user, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != strlen(user)) {
    wipe(token, nt); wipe(user, nu); return 0;
  }
  return 1;
}
int em_connect_servidores(const char *token, const char *user, RedeJob *job, EmConnectServidor *out, int max) {
  char path[256]; RedeResposta r; int n = 0;
  if (!user || strspn(user, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != strlen(user)) return JF_ERR_FORMATO;
  snprintf(path, sizeof path, "servers?userId=%s", user);
  int e = em_connect_pedir(path, NULL, token, job, &r); if (e) return e;
  for (const char *p = js_raiz_array(r.corpo); p && n < max;) {
    const char *end = js_fim(p); if (!end) break;
    EmConnectServidor s = {0};
    js_texto(p, end, "Name", s.nome, sizeof s.nome); js_texto(p, end, "SystemId", s.id, sizeof s.id);
    js_texto(p, end, "LocalAddress", s.local, sizeof s.local); js_texto(p, end, "Url", s.remoto, sizeof s.remoto);
    js_texto(p, end, "AccessKey", s.chave, sizeof s.chave);
    if (s.chave[0] && !strpbrk(s.chave, "\r\n") && s.id[0]) out[n++] = s;
    p = js_prox(end);
  }
  if (r.corpo) wipe(r.corpo, r.n_corpo); rede_resposta_limpar(&r); return n;
}
int em_connect_trocar(JfConta *c, const EmConnectServidor *s, const char *user, RedeJob *job) {
  int e = JF_ERR_REDE;
  if (!c || !s || !user || !*user || strspn(user, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != strlen(user)) return JF_ERR_ENTRADA;
  for (int i = 0; i < 2; i++) {
    char base[512], url[1024], auth[512], key[160]; RedeResposta r;
    if (!address(i ? s->remoto : s->local, base, sizeof base)) continue;
    JfConta candidate = *c; candidate.tipo = JF_TIPO_EMBY;
    snprintf(candidate.base, sizeof candidate.base, "%s", base);
    e = jf_publico(base, job, &candidate);
    if (e || strcmp(candidate.servidorId, s->id)) { if (!e) e = JF_ERR_OUTRO_SERVIDOR; wipe(&candidate, sizeof candidate); continue; }
    jf_cabecalho_auth(&candidate, auth, sizeof auth);
    snprintf(key, sizeof key, "X-Emby-Token: %s", s->chave);
    const char *h[] = {"Accept: application/json", auth, key, NULL};
    snprintf(url, sizeof url, "%s/emby/Connect/Exchange?format=json&ConnectUserId=%s", candidate.base, user);
    e = request(url, NULL, h, job, 1, &r); wipe(auth, sizeof auth); wipe(key, sizeof key);
    if (e) { wipe(&candidate, sizeof candidate); continue; }
    int ok = js_texto_raiz(r.corpo, "AccessToken", candidate.token, sizeof candidate.token) &&
      js_texto_raiz(r.corpo, "LocalUserId", candidate.usuarioId, sizeof candidate.usuarioId);
    if (r.corpo) wipe(r.corpo, r.n_corpo); rede_resposta_limpar(&r);
    if (ok && candidate.token[0] && candidate.usuarioId[0]) { *c = candidate; wipe(&candidate, sizeof candidate); return JF_OK; }
    wipe(&candidate, sizeof candidate); e = JF_ERR_FORMATO;
  }
  return e;
}
