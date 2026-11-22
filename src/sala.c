// A regra da Sessao. Ver sala.h.
#include "sala.h"
#include <stdio.h>
#include <string.h>

void sala_iniciar(Sala *s, int nCartas) {
  memset(s, 0, sizeof *s);
  if (nCartas < 0) nCartas = 0;
  if (nCartas > SALA_CARTAS_MAX) nCartas = SALA_CARTAS_MAX;
  s->nCartas = nCartas;
  s->vence = -1;
}

int sala_comecar(Sala *s) {
  if (s->fase != SALA_ENTRANDO || s->nGente < 1 || s->nCartas < 1) return 0;
  s->fase = SALA_VOTANDO;
  return 1;
}

int sala_votos(const Sala *s, int p) {
  int c, k = 0;
  if (p < 0 || p >= s->nGente) return 0;
  for (c = 0; c < s->nCartas; c++) k += (s->g[p].visto >> c) & 1u;
  return k;
}

int sala_sins(const Sala *s, int c) {
  int p, k = 0;
  if (c < 0 || c >= s->nCartas) return 0;
  for (p = 0; p < s->nGente; p++) k += (s->g[p].sim >> c) & 1u;
  return k;
}

void sala_encerrar(Sala *s) {
  int c, melhor = -1, mais = 0;
  if (s->fase != SALA_VOTANDO) return;
  for (c = 0; c < s->nCartas; c++) {
    int k = sala_sins(s, c);
    if (k > mais) { mais = k; melhor = c; }
  }
  s->vence = melhor;
  s->unanime = melhor >= 0 && mais == s->nGente;
  s->fase = melhor >= 0 ? SALA_DECIDIDO : SALA_SEM_ACORDO;
}

static int hexMin(const char *p, size_t n) {
  size_t i;
  for (i = 0; i < n; i++)
    if (!((p[i] >= '0' && p[i] <= '9') || (p[i] >= 'a' && p[i] <= 'f'))) return 0;
  return 1;
}

// Nome de visitante: sem controles, espacos das pontas fora, no maximo
// SALA_NOME_MAX bytes sem partir um caractere UTF-8; UTF-8 invalido vira "?".
static void nomeLimpo(char *dst, const char *src) {
  const unsigned char *p = (const unsigned char *)src;
  size_t n = 0;
  while (*p == ' ') p++;
  while (*p) {
    size_t t = *p < 0x80 ? 1 : (*p >> 5) == 6 ? 2 : (*p >> 4) == 14 ? 3 : (*p >> 3) == 30 ? 4 : 0, i;
    int ok = t > 0;
    for (i = 1; ok && i < t; i++) if ((p[i] & 0xC0) != 0x80) ok = 0;
    if (!ok) { if (n + 1 > SALA_NOME_MAX) break; dst[n++] = '?'; p++; continue; }
    if (*p < 0x20 || *p == 0x7F) { p++; continue; }
    if (n + t > SALA_NOME_MAX) break;
    memcpy(dst + n, p, t); n += t; p += t;
  }
  while (n && dst[n - 1] == ' ') n--;
  dst[n] = 0;
  if (!n) { dst[0] = '?'; dst[1] = 0; }
}

static int achar(const Sala *s, const char *chave) {
  int p;
  for (p = 0; p < s->nGente; p++)
    if (!memcmp(s->g[p].pub, chave, 4)) return !memcmp(s->g[p].seg, chave + 4, 8) ? p : -2;
  return -1;
}

static void conferir(Sala *s, int c) {
  int p, todosViram = 1;
  if (sala_sins(s, c) == s->nGente) {
    s->vence = c; s->unanime = 1; s->fase = SALA_DECIDIDO;
    return;
  }
  for (p = 0; p < s->nGente; p++) if (sala_votos(s, p) < s->nCartas) todosViram = 0;
  if (todosViram) sala_encerrar(s);
}

int sala_mensagem(Sala *s, const char *m) {
  int p;
  if (!m || strlen(m) < 15 || (m[0] != 'j' && m[0] != 'v') || m[1] != ':' ||
      !hexMin(m + 2, 12) || m[14] != ':') return SALA_INVALIDO;
  p = achar(s, m + 2);
  if (p == -2) return SALA_ESTRANHO;   // id publico de outra pessoa
  if (m[0] == 'j') {
    if (p >= 0) return SALA_IGUAL;
    if (s->fase != SALA_ENTRANDO && s->fase != SALA_VOTANDO) return SALA_FORA_DE_HORA;
    if (s->nGente >= SALA_GENTE_MAX) return SALA_CHEIA;
    p = s->nGente++;
    memset(&s->g[p], 0, sizeof s->g[p]);
    memcpy(s->g[p].pub, m + 2, 4);
    memcpy(s->g[p].seg, m + 6, 8);
    nomeLimpo(s->g[p].nome, m + 15);
    return SALA_OK;
  }
  {
    const char *q = m + 15;
    int c = 0, nd = 0;
    if (p < 0) return SALA_ESTRANHO;
    while (*q >= '0' && *q <= '9' && nd < 3) { c = c * 10 + (*q - '0'); q++; nd++; }
    if (!nd || q[0] != ':' || (q[1] != '0' && q[1] != '1') || q[2]) return SALA_INVALIDO;
    if (c >= s->nCartas) return SALA_INVALIDO;
    if (s->fase != SALA_VOTANDO) return SALA_FORA_DE_HORA;
    if ((s->g[p].visto >> c) & 1u) return SALA_IGUAL;
    s->g[p].visto |= 1u << c;
    if (q[1] == '1') s->g[p].sim |= 1u << c;
    conferir(s, c);
    return SALA_OK;
  }
}

size_t sala_json_texto(char *dst, size_t n, const char *s) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  size_t k = 0;
  for (; *p; p++) {
    if (*p < 0x20 || *p == '"' || *p == '\\' || *p == '<' || *p == '>' || *p == '&' || *p == 0x7F) {
      if (k + 7 > n) return 0;
      k += (size_t)snprintf(dst + k, n - k, "\\u%04x", *p);
    } else {
      if (k + 2 > n) return 0;
      dst[k++] = (char)*p;
    }
  }
  if (k >= n) return 0;
  dst[k] = 0;
  return k;
}

size_t sala_estado_json(const Sala *s, char *dst, size_t n) {
  int p, r;
  size_t k;
  r = snprintf(dst, n, "{\"f\":%d,\"n\":%d,\"v\":%d,\"u\":%d,\"g\":[", s->fase, s->nCartas, s->vence, s->unanime);
  if (r < 0 || (size_t)r >= n) return 0;
  k = (size_t)r;
  for (p = 0; p < s->nGente; p++) {
    char nome[SALA_NOME_MAX * 6 + 1];
    if (!sala_json_texto(nome, sizeof nome, s->g[p].nome)) return 0;
    r = snprintf(dst + k, n - k, "%s{\"i\":\"%s\",\"n\":\"%s\",\"k\":%d}", p ? "," : "", s->g[p].pub, nome, sala_votos(s, p));
    if (r < 0 || (size_t)r >= n - k) return 0;
    k += (size_t)r;
  }
  if (k + 3 > n) return 0;
  memcpy(dst + k, "]}", 3);
  return k + 2;
}
