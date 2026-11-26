// Nuvio Casa, regra pura. Ver casa.h.
#include "casa.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int casa_ip_privado(uint32_t h) {
  return (h >> 24) == 10 || (h >> 20) == 0xAC1 || (h >> 16) == 0xC0A8 || (h >> 24) == 127 ||
         (h >> 22) == (0x64400000u >> 22) || (h >> 16) == 0xA9FE;
}

static int aparar(const char *m, int len) {
  while (len > 0 && (m[len - 1] == '\n' || m[len - 1] == '\r' || m[len - 1] == ' ' || m[len - 1] == 0)) len--;
  return len;
}

int casa_e_pergunta(const char *msg, int len) {
  int n = (int)strlen(CASA_PERGUNTA);
  if (!msg || len < n) return 0;
  return aparar(msg, len) == n && !memcmp(msg, CASA_PERGUNTA, (size_t)n);
}

static int tokenValido(const char *t) {
  int i = 0;
  if (!t) return 0;
  for (; t[i]; i++)
    if (i >= 32 || !isxdigit((unsigned char)t[i])) return 0;
  return i >= 16;
}

static int nomeOk(unsigned char c) {
  return c >= 0x80 || isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.';
}

int casa_montar_resposta(int porta, const char *token, const char *nome, int p2p,
                         char *out, unsigned n) {
  char limpo[48];
  unsigned k = 0;
  int e;
  if (!out || porta < 1 || porta > 65535 || !tokenValido(token)) return 0;
  for (const char *p = nome ? nome : ""; *p && k + 1 < sizeof limpo; p++)
    if (nomeOk((unsigned char)*p)) limpo[k++] = *p;
  while (k && limpo[k - 1] == ' ') k--;
  limpo[k] = 0;
  e = snprintf(out, n, "NUVIO-CASA/1 ! porta=%d token=%s cap=%s nome=%s", porta, token,
               p2p ? "p2p" : "-", k ? limpo : "Nuvio");
  return e > 0 && (unsigned)e < n ? e : 0;
}

// Valor de "chave=" ate o proximo espaco (o nome, ultimo campo, vai ate o fim).
static int campo(const char *m, const char *chave, int ateFim, char *out, unsigned n) {
  size_t L = strlen(chave);
  for (const char *p = m; (p = strstr(p, chave)); p += L) {
    unsigned k = 0;
    if (p != m && p[-1] != ' ') continue;
    p += L;
    while (*p && (ateFim || *p != ' ') && k + 1 < n) out[k++] = *p++;
    out[k] = 0;
    return 1;
  }
  return 0;
}

int casa_ler_resposta(const char *msg, int len, CasaAjudante *a) {
  char m[256], v[64];
  static const char cab[] = "NUVIO-CASA/1 ! ";
  if (!msg || !a || len <= 0) return 0;
  len = aparar(msg, len);
  if (len < (int)sizeof cab - 1 || len >= (int)sizeof m) return 0;
  memcpy(m, msg, (size_t)len); m[len] = 0;
  for (int i = 0; i < len; i++)
    if ((unsigned char)m[i] < 32) return 0;
  if (strncmp(m, cab, sizeof cab - 1)) return 0;
  if (!campo(m, "porta=", 0, v, sizeof v)) return 0;
  for (const char *p = v; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
  if (!v[0] || strlen(v) > 5) return 0;
  a->porta = atoi(v);
  if (a->porta < 1 || a->porta > 65535) return 0;
  if (!campo(m, "token=", 0, v, sizeof v) || !tokenValido(v)) return 0;
  memcpy(a->token, v, strlen(v) + 1);   // tokenValido: cabe
  a->p2p = campo(m, "cap=", 0, v, sizeof v) && !strcmp(v, "p2p");
  a->nome[0] = 0;
  if (campo(m, "nome=", 1, v, sizeof v)) {
    unsigned k = 0;
    for (const char *p = v; *p && k + 1 < sizeof a->nome; p++)
      if (nomeOk((unsigned char)*p)) a->nome[k++] = *p;
    a->nome[k] = 0;
  }
  if (!a->nome[0]) snprintf(a->nome, sizeof a->nome, "Nuvio");
  return 1;
}

int casa_escolher(const CasaAjudante *l, int n, long agora) {
  int m = -1;
  for (int i = 0; l && i < n; i++) {
    if (!l[i].p2p || !casa_ip_privado(l[i].ip) || l[i].porta < 1) continue;
    if (agora - l[i].visto >= CASA_VALIDADE_S || l[i].visto > agora + 5) continue;
    if (m < 0 || l[i].visto > l[m].visto || (l[i].visto == l[m].visto && l[i].ip < l[m].ip)) m = i;
  }
  return m;
}

int casa_base(const CasaAjudante *a, char *out, unsigned n) {
  int e;
  if (!a || !out || !n || !tokenValido(a->token) || a->porta < 1 || a->porta > 65535 || !casa_ip_privado(a->ip)) return 0;
  e = snprintf(out, n, "http://%u.%u.%u.%u:%d/%s", a->ip >> 24, (a->ip >> 16) & 255,
               (a->ip >> 8) & 255, a->ip & 255, a->porta, a->token);
  return e > 0 && (unsigned)e < n;
}

// Inteiro de "chave=" na consulta; `padrao` se faltar ou nao for numero.
static int consulta(const char *q, const char *chave, int padrao, int min, int max) {
  size_t L = strlen(chave);
  for (const char *p = q; p && *p; ) {
    const char *f = p + strcspn(p, "&");
    if (!strncmp(p, chave, L) && p[L] == '=') {
      const char *v = p + L + 1;
      int neg = *v == '-', d = 0;
      long x = 0;
      for (v += neg; v < f; v++, d++) {
        if (!isdigit((unsigned char)*v) || d > 6) return padrao;
        x = x * 10 + (*v - '0');
      }
      if (!d) return padrao;
      if (neg) x = -x;
      return x < min || x > max ? padrao : (int)x;
    }
    p = *f ? f + 1 : f;
  }
  return padrao;
}

int casa_rota(const char *metodo, const char *alvo, const char *token, CasaPedido *p) {
  size_t T;
  const char *c, *q;
  int get, post, i;
  if (!p) return CASA_ROTA_NADA;
  memset(p, 0, sizeof *p);
  p->idx = -1;
  if (!metodo || !alvo || !tokenValido(token)) return CASA_ROTA_NADA;
  get = !strcmp(metodo, "GET") || !strcmp(metodo, "HEAD");
  post = !strcmp(metodo, "POST");
  T = strlen(token);
  if (alvo[0] != '/' || strncmp(alvo + 1, token, T) || alvo[1 + T] != '/') return CASA_ROTA_NADA;
  c = alvo + 2 + T;
  q = strchr(c, '?');
  if (!strcmp(c, "settings")) return p->rota = !strcmp(metodo, "GET") ? CASA_ROTA_SETTINGS : CASA_ROTA_NADA;
  for (i = 0; i < 40; i++) {
    if (!isxdigit((unsigned char)c[i])) return CASA_ROTA_NADA;
    p->hash[i] = (char)tolower((unsigned char)c[i]);
  }
  if (c[40] != '/') return CASA_ROTA_NADA;
  c += 41;
  if (post && !strncmp(c, "create", 6) && (c[6] == 0 || c[6] == '?')) {
    p->idx = consulta(q ? q + 1 : NULL, "idx", -1, -1, 99999);
    p->temporada = consulta(q ? q + 1 : NULL, "s", 0, 0, 9999);
    p->episodio = consulta(q ? q + 1 : NULL, "e", 0, 0, 99999);
    return p->rota = CASA_ROTA_CRIAR;
  }
  if (get && !q) {
    for (i = 0; c[i]; i++)
      if (!isdigit((unsigned char)c[i]) || i > 5) return CASA_ROTA_NADA;
    if (!i) return CASA_ROTA_NADA;
    p->idx = atoi(c);
    return p->rota = CASA_ROTA_ARQUIVO;
  }
  return CASA_ROTA_NADA;
}

int casa_host_ok(const char *host, int porta) {
  unsigned a, b, c, d, pt;
  char sobra;
  if (!host) return 0;
  while (*host == ' ') host++;
  if (sscanf(host, "%3u.%3u.%3u.%3u:%5u%c", &a, &b, &c, &d, &pt, &sobra) != 5) return 0;
  if (a > 255 || b > 255 || c > 255 || d > 255 || (int)pt != porta) return 0;
  for (const char *p = host; *p; p++)
    if (!isdigit((unsigned char)*p) && *p != '.' && *p != ':') return 0;
  return casa_ip_privado((a << 24) | (b << 16) | (c << 8) | d);
}

int casa_fontes_do_corpo(const char *corpo, char *out, unsigned n) {
  const char *p, *fim;
  unsigned u = 0;
  int k = 0;
  if (out && n) out[0] = 0;
  if (!corpo || !out || !n || !(p = strstr(corpo, "\"sources\""))) return 0;
  if (!(p = strchr(p, '[')) || !(fim = strchr(p, ']'))) return 0;
  while ((p = memchr(p, '"', (size_t)(fim - p)))) {
    const char *f = memchr(p + 1, '"', (size_t)(fim - p - 1));
    size_t L;
    int ok;
    if (!f) break;
    L = (size_t)(f - p - 1);
    ok = L > 4 && L < 300 && (!strncmp(p + 1, "tracker:", 8) || !strncmp(p + 1, "dht:", 4));
    for (size_t i = 0; ok && i < L; i++)
      if ((unsigned char)p[1 + i] < 33 || p[1 + i] == '\\' || p[1 + i] == 127) ok = 0;
    if (ok && u + L + 2 <= n) {
      memcpy(out + u, p + 1, L); u += (unsigned)L;
      out[u++] = '\n'; out[u] = 0;
      k++;
    }
    p = f + 1;
  }
  return k;
}

int casa_json_arquivos(uint64_t tam, char *out, unsigned n) {
  int e = snprintf(out, n, "{\"files\":[{\"path\":\"casa.mkv\",\"name\":\"casa.mkv\",\"length\":%llu}]}",
                   (unsigned long long)(tam ? tam : 1));
  return e > 0 && (unsigned)e < n ? e : 0;
}

int casa_url_do_motor(const char *url, const char **caminho) {
  static const char pre[] = "http://127.0.0.1:";
  long porta = 0;
  const char *p;
  int d = 0;
  if (!url || strncmp(url, pre, sizeof pre - 1)) return 0;
  for (p = url + sizeof pre - 1; isdigit((unsigned char)*p) && d < 6; p++, d++) porta = porta * 10 + (*p - '0');
  if (!d || *p != '/' || porta < 1 || porta > 65535) return 0;
  for (const char *q = p; *q; q++)
    if ((unsigned char)*q <= 32 || *q == 127) return 0;
  if (caminho) *caminho = p;
  return (int)porta;
}
