// Parser e protocolo puro da busca por frase do Farol — ver farolbuscaparse.h.
// Puro: sem SDL, sem rede, sem pthread, sem dados.h, sem malloc, sem log, sem
// estado estatico mutavel.
#include "farolbuscaparse.h"
#include "faroltexto.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define J_PROF_MAX 16

// ------------------------------------------------------------------ UTF-8 --

static long lerUtf8(const char **pp, const char *fim) {
  const unsigned char *s = (const unsigned char *)*pp;
  unsigned char c;
  long cp;
  int extra, i;
  if (*pp >= fim) return -1;
  c = s[0];
  if (c < 0x80) { *pp += 1; return c; }
  if (c >= 0xC2 && c <= 0xDF)      { cp = c & 0x1F; extra = 1; }
  else if (c >= 0xE0 && c <= 0xEF) { cp = c & 0x0F; extra = 2; }
  else if (c >= 0xF0 && c <= 0xF4) { cp = c & 0x07; extra = 3; }
  else return -1;
  if (fim - *pp < extra + 1) return -1;
  for (i = 1; i <= extra; i++) {
    if ((s[i] & 0xC0) != 0x80) return -1;
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)) return -1;
  if (cp >= 0xD800 && cp <= 0xDFFF) return -1;
  if (cp > 0x10FFFF) return -1;
  *pp += extra + 1;
  return cp;
}

static size_t escreverUtf8(long cp, char *dst, size_t cap) {
  if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
  if (cp < 0x80) {
    if (cap < 1) return 0;
    dst[0] = (char)cp;
    return 1;
  }
  if (cp < 0x800) {
    if (cap < 2) return 0;
    dst[0] = (char)(0xC0 | (cp >> 6));
    dst[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  }
  if (cp < 0x10000) {
    if (cap < 3) return 0;
    dst[0] = (char)(0xE0 | (cp >> 12));
    dst[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    dst[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  }
  if (cap < 4) return 0;
  dst[0] = (char)(0xF0 | (cp >> 18));
  dst[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  dst[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
  dst[3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

static unsigned char minAscii(unsigned char c) {
  return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

static int contemSemCaixa(const char *s, const char *sub) {
  size_t i, n = strlen(sub);
  if (!s || !n) return 0;
  for (; *s; s++) {
    for (i = 0; i < n; i++) {
      if (!s[i] || minAscii((unsigned char)s[i]) != minAscii((unsigned char)sub[i]))
        break;
    }
    if (i == n) return 1;
  }
  return 0;
}

static int temUrl(const char *s) {
  return contemSemCaixa(s, "://") || contemSemCaixa(s, "www.");
}

static int temControleProibido(const char *s, int permiteQuebra) {
  const unsigned char *p = (const unsigned char *)s;
  for (; *p; p++) {
    if (*p == 0x7f) return 1;
    if (*p < 0x20) {
      if (permiteQuebra && (*p == '\t' || *p == '\n' || *p == '\r')) continue;
      return 1;
    }
  }
  return 0;
}

static int idiomaValido(const char *s) {
  size_t n;
  if (!s || !s[0]) return 0;
  n = strlen(s);
  if (n < 2 || n > 7) return 0;
  if (!(s[0] >= 'a' && s[0] <= 'z') || !(s[1] >= 'a' && s[1] <= 'z')) return 0;
  if (n == 2) return 1;
  if (s[2] != '-' || n < 5) return 0;
  for (size_t i = 3; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return 0;
  }
  return 1;
}

static int imdbValido(const char *s) {
  size_t n = 0;
  if (!s || s[0] != 't' || s[1] != 't') return 0;
  s += 2;
  while (isdigit((unsigned char)*s)) { s++; n++; }
  return *s == '\0' && n >= 1 && n <= 12;
}

static int refHistoricoValida(const char *s) {
  if (!s || !s[0] || strlen(s) > 64) return 0;
  if (imdbValido(s)) return 1;
  if (!strncmp(s, "tmdb:", 5)) {
    const char *p = s + 5;
    long long v = 0;
    size_t n = 0;
    while (isdigit((unsigned char)*p)) {
      if (++n > 10) return 0;
      v = v * 10 + (*p++ - '0');
    }
    return *p == '\0' && n >= 1 && v > 0 && v <= 2000000000LL;
  }
  return 0;
}

static int generoClienteValido(const char *s) {
  size_t n;
  if (!s || !s[0]) return 0;
  n = strlen(s);
  if (n > 40) return 0;
  if (temControleProibido(s, 0) || temUrl(s) || strchr(s, '@')) return 0;
  // Rejeita UTF-8 quebrado
  {
    const char *p = s, *fim = s + n;
    while (p < fim) if (lerUtf8(&p, fim) < 0) return 0;
  }
  return 1;
}

// ------------------------------------------------- normalizacao de frase --

int farolbusca_normalizar_frase(const char *src, char *dst, size_t cap) {
  const char *p, *fim;
  size_t u = 0;
  int cps = 0, emEspaco = 1;
  if (dst && cap) dst[0] = '\0';
  if (!src || !dst || cap < 2) return 0;
  if (temControleProibido(src, 1) || temUrl(src)) return 0;
  fim = src + strlen(src);
  p = src;
  while (p < fim) {
    const char *ini = p;
    long cp = lerUtf8(&p, fim);
    if (cp < 0) { dst[0] = '\0'; return 0; }
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r') {
      if (!emEspaco) emEspaco = 1;
      continue;
    }
    if (u > 0 && emEspaco) {
      if (cps >= FAROL_BUSCA_FRASE_MAX) break;
      if (u + 1 >= cap) { dst[0] = '\0'; return 0; }
      dst[u++] = ' ';
      cps++;
    }
    if (cps >= FAROL_BUSCA_FRASE_MAX) break;
    {
      size_t n = (size_t)(p - ini);
      if (u + n >= cap) { dst[0] = '\0'; return 0; }
      memcpy(dst + u, ini, n);
      u += n;
      cps++;
      emEspaco = 0;
    }
  }
  while (u > 0 && dst[u - 1] == ' ') u--;
  dst[u] = '\0';
  return u > 0;
}

// ------------------------------------------------ escritor JSON de pilha --

typedef struct {
  char  *buf;
  size_t n, cap;
  int    erro;
} Esc;

static void escCru(Esc *e, const char *s, size_t n) {
  if (e->erro) return;
  if (n >= e->cap - e->n) { e->erro = 1; return; }
  memcpy(e->buf + e->n, s, n);
  e->n += n;
  e->buf[e->n] = '\0';
}

static void escLit(Esc *e, const char *s) { escCru(e, s, strlen(s)); }

static void escStr(Esc *e, const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  escLit(e, "\"");
  for (; *p && !e->erro; p++) {
    if (*p == '"' || *p == '\\') {
      char par[2] = { '\\', (char)*p };
      escCru(e, par, 2);
    } else if (*p == '\n') escLit(e, "\\n");
    else if (*p == '\r') escLit(e, "\\r");
    else if (*p == '\t') escLit(e, "\\t");
    else if (*p < 0x20) {
      char h[8];
      snprintf(h, sizeof h, "\\u%04x", (unsigned)*p);
      escLit(e, h);
    } else {
      char ch = (char)*p;
      escCru(e, &ch, 1);
    }
  }
  escLit(e, "\"");
}

static void escListaStr(Esc *e, const char *chave, const char *const *itens,
                        int n, int maxItens, int (*valido)(const char *)) {
  int escritos = 0, i, j;
  const char *usados[FAROL_BUSCA_HIST_MAX];
  if (!itens || n <= 0) return;
  if (maxItens > FAROL_BUSCA_HIST_MAX) maxItens = FAROL_BUSCA_HIST_MAX;
  for (i = 0; i < n && escritos < maxItens; i++) {
    int dup = 0;
    if (!itens[i] || !valido(itens[i])) continue;
    for (j = 0; j < escritos; j++)
      if (!strcmp(usados[j], itens[i])) { dup = 1; break; }
    if (dup) continue;
    if (escritos == 0) {
      escLit(e, ",\"");
      escLit(e, chave);
      escLit(e, "\":[");
    } else {
      escLit(e, ",");
    }
    escStr(e, itens[i]);
    usados[escritos++] = itens[i];
  }
  if (escritos > 0) escLit(e, "]");
}

size_t farolbusca_montar_corpo(const char *frase, const FarolBuscaCtx *ctx,
                               char *dst, size_t cap) {
  char norm[FAROL_BUSCA_FRASE_BYTES];
  size_t lim;
  Esc e;
  if (dst && cap) dst[0] = '\0';
  if (!dst || cap < 16) return 0;
  if (!farolbusca_normalizar_frase(frase, norm, sizeof norm)) return 0;
  lim = cap < (size_t)(FAROL_BUSCA_CORPO_MAX + 1) ? cap : (size_t)(FAROL_BUSCA_CORPO_MAX + 1);
  e.buf = dst; e.n = 0; e.cap = lim; e.erro = 0;
  dst[0] = '\0';

  escLit(&e, "{\"frase\":");
  escStr(&e, norm);

  // Perfil privado: nao manda NADA alem da frase (ADENDO:15).
  if (ctx && ctx->privado) {
    escLit(&e, "}");
    if (e.erro) { dst[0] = '\0'; return 0; }
    return e.n;
  }

  // Sem contexto explicito: recua para consentiu:0 explicito (ressalva farol-29).
  if (!ctx) {
    escLit(&e, ",\"consentiu\":0}");
    if (e.erro) { dst[0] = '\0'; return 0; }
    return e.n;
  }

  if (ctx->idioma && idiomaValido(ctx->idioma)) {
    escLit(&e, ",\"idioma\":");
    escStr(&e, ctx->idioma);
  }
  if (ctx->infantil) escLit(&e, ",\"infantil\":1");

  // Ausencia de consentiu conta como consentimento no servidor (farol-busca.js:265).
  // Perfil infantil ou sem consentimento envia consentiu:0 explicito e nada pessoal.
  if (!ctx->consentiu || ctx->infantil) {
    escLit(&e, ",\"consentiu\":0}");
    if (e.erro) { dst[0] = '\0'; return 0; }
    return e.n;
  }

  escLit(&e, ",\"consentiu\":1");
  escListaStr(&e, "generos", ctx->generos, ctx->nGeneros,
              FAROL_BUSCA_GENEROS_MAX, generoClienteValido);
  escListaStr(&e, "historico", ctx->historico, ctx->nHistorico,
              FAROL_BUSCA_HIST_MAX, refHistoricoValida);
  escListaStr(&e, "vistos", ctx->vistos, ctx->nVistos,
              FAROL_BUSCA_HIST_MAX, refHistoricoValida);
  escLit(&e, "}");

  if (e.erro) { dst[0] = '\0'; return 0; }
  return e.n;
}

// ------------------------------------------------- parser JSON estrito ----

typedef struct {
  const char *p;
  const char *fim;
} JCur;

static void jEspaco(JCur *c) {
  while (c->p < c->fim &&
         (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r'))
    c->p++;
}

static int hexDig(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

static int jHex4(JCur *c, unsigned *out) {
  unsigned v = 0;
  int i;
  if (c->fim - c->p < 4) return 0;
  for (i = 0; i < 4; i++) {
    int d = hexDig(c->p[i]);
    if (d < 0) return 0;
    v = (v << 4) | (unsigned)d;
  }
  c->p += 4;
  *out = v;
  return 1;
}

static int jString(JCur *c, char *dst, size_t cap) {
  size_t u = 0;
  jEspaco(c);
  if (c->p >= c->fim || *c->p != '"') return 0;
  c->p++;
  if (dst && cap) dst[0] = '\0';
  while (c->p < c->fim) {
    unsigned char ch = (unsigned char)*c->p++;
    if (ch == '"') {
      if (dst && cap) dst[u] = '\0';
      return 1;
    }
    if (ch < 0x20) return 0;
    if (ch == '\\') {
      char esc;
      if (c->p >= c->fim) return 0;
      esc = *c->p++;
      if (esc == '"' || esc == '\\' || esc == '/') {
        if (dst && u + 1 < cap) dst[u++] = esc;
      } else if (esc == 'b') { if (dst && u + 1 < cap) dst[u++] = '\b'; }
      else if (esc == 'f')   { if (dst && u + 1 < cap) dst[u++] = '\f'; }
      else if (esc == 'n')   { if (dst && u + 1 < cap) dst[u++] = '\n'; }
      else if (esc == 'r')   { if (dst && u + 1 < cap) dst[u++] = '\r'; }
      else if (esc == 't')   { if (dst && u + 1 < cap) dst[u++] = '\t'; }
      else if (esc == 'u') {
        unsigned u1 = 0;
        long cp;
        char tmp[4];
        size_t nb;
        if (!jHex4(c, &u1)) return 0;
        if (u1 >= 0xD800 && u1 <= 0xDBFF) {
          unsigned u2 = 0;
          if (c->fim - c->p < 6 || c->p[0] != '\\' || c->p[1] != 'u') return 0;
          c->p += 2;
          if (!jHex4(c, &u2) || u2 < 0xDC00 || u2 > 0xDFFF) return 0;
          cp = 0x10000L + (long)(((u1 - 0xD800u) << 10) | (u2 - 0xDC00u));
        } else if (u1 >= 0xDC00 && u1 <= 0xDFFF) {
          return 0;
        } else {
          cp = (long)u1;
        }
        nb = escreverUtf8(cp, tmp, sizeof tmp);
        if (!nb) return 0;
        if (dst && u + nb < cap) { memcpy(dst + u, tmp, nb); u += nb; }
      } else {
        return 0;
      }
    } else {
      if (dst && u + 1 < cap) dst[u++] = (char)ch;
    }
  }
  return 0;
}

static int jLit(JCur *c, const char *lit) {
  size_t n = strlen(lit);
  jEspaco(c);
  if ((size_t)(c->fim - c->p) < n || memcmp(c->p, lit, n) != 0) return 0;
  c->p += n;
  return 1;
}

static int jInt(JCur *c, long long *out) {
  int neg = 0;
  long long v = 0;
  const char *ini;
  jEspaco(c);
  if (c->p >= c->fim) return 0;
  if (*c->p == '-') { neg = 1; c->p++; }
  ini = c->p;
  if (c->p >= c->fim || !isdigit((unsigned char)*c->p)) return 0;
  if (*c->p == '0' && c->p + 1 < c->fim && isdigit((unsigned char)c->p[1]))
    return 0;
  while (c->p < c->fim && isdigit((unsigned char)*c->p)) {
    int d = *c->p++ - '0';
    if (v > (INT64_MAX - d) / 10) return 0;
    v = v * 10 + d;
  }
  if (c->p == ini) return 0;
  if (c->p < c->fim && (*c->p == '.' || *c->p == 'e' || *c->p == 'E')) return 0;
  if (out) *out = neg ? -v : v;
  return 1;
}

static int jPularNumero(JCur *c) {
  jEspaco(c);
  if (c->p >= c->fim) return 0;
  if (*c->p == '-') c->p++;
  if (c->p >= c->fim || !isdigit((unsigned char)*c->p)) return 0;
  if (*c->p == '0') {
    c->p++;
    if (c->p < c->fim && isdigit((unsigned char)*c->p)) return 0;
  } else {
    while (c->p < c->fim && isdigit((unsigned char)*c->p)) c->p++;
  }
  if (c->p < c->fim && *c->p == '.') {
    c->p++;
    if (c->p >= c->fim || !isdigit((unsigned char)*c->p)) return 0;
    while (c->p < c->fim && isdigit((unsigned char)*c->p)) c->p++;
  }
  if (c->p < c->fim && (*c->p == 'e' || *c->p == 'E')) {
    c->p++;
    if (c->p < c->fim && (*c->p == '+' || *c->p == '-')) c->p++;
    if (c->p >= c->fim || !isdigit((unsigned char)*c->p)) return 0;
    while (c->p < c->fim && isdigit((unsigned char)*c->p)) c->p++;
  }
  return 1;
}

static int jPularValor(JCur *c, int prof) {
  if (prof > J_PROF_MAX) return 0;
  jEspaco(c);
  if (c->p >= c->fim) return 0;
  if (*c->p == '"') return jString(c, NULL, 0);
  if (*c->p == '{') {
    c->p++;
    jEspaco(c);
    if (c->p < c->fim && *c->p == '}') { c->p++; return 1; }
    for (;;) {
      if (!jString(c, NULL, 0)) return 0;
      jEspaco(c);
      if (c->p >= c->fim || *c->p != ':') return 0;
      c->p++;
      if (!jPularValor(c, prof + 1)) return 0;
      jEspaco(c);
      if (c->p >= c->fim) return 0;
      if (*c->p == '}') { c->p++; return 1; }
      if (*c->p != ',') return 0;
      c->p++;
    }
  }
  if (*c->p == '[') {
    c->p++;
    jEspaco(c);
    if (c->p < c->fim && *c->p == ']') { c->p++; return 1; }
    for (;;) {
      if (!jPularValor(c, prof + 1)) return 0;
      jEspaco(c);
      if (c->p >= c->fim) return 0;
      if (*c->p == ']') { c->p++; return 1; }
      if (*c->p != ',') return 0;
      c->p++;
    }
  }
  if (*c->p == 't') return jLit(c, "true");
  if (*c->p == 'f') return jLit(c, "false");
  if (*c->p == 'n') return jLit(c, "null");
  return jPularNumero(c);
}

static int jSlugGeneroValido(const char *s) {
  size_t i, n;
  if (!s || !s[0]) return 0;
  n = strlen(s);
  if (n >= 24) return 0;
  for (i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)s[i];
    if (!((ch >= 'a' && ch <= 'z') || ch == '_')) return 0;
  }
  return 1;
}

static void normalizarTipoItem(const char *src, char dst[8]) {
  if (!src) { strcpy(dst, "movie"); return; }
  if (!strcmp(src, "series") || !strcmp(src, "serie") || !strcmp(src, "tv"))
    strcpy(dst, "series");
  else
    strcpy(dst, "movie");
}

static void normalizarTipoFiltro(const char *src, char dst[16]) {
  if (!src) { strcpy(dst, "qualquer"); return; }
  if (!strcmp(src, "filme") || !strcmp(src, "movie")) strcpy(dst, "filme");
  else if (!strcmp(src, "serie") || !strcmp(src, "series") || !strcmp(src, "tv"))
    strcpy(dst, "serie");
  else strcpy(dst, "qualquer");
}

static void copiarStr(char *dst, size_t cap, const char *src) {
  size_t n;
  if (!dst || !cap) return;
  if (!src) { dst[0] = '\0'; return; }
  n = strlen(src);
  if (n >= cap) n = cap - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static int lerUmId(JCur *c, FarolBuscaId *out) {
  memset(out, 0, sizeof *out);
  strcpy(out->tipo, "movie");
  jEspaco(c);
  if (c->p >= c->fim || *c->p != '{') return 0;
  c->p++;
  jEspaco(c);
  if (c->p < c->fim && *c->p == '}') { c->p++; return 1; }
  for (;;) {
    char chave[32];
    if (!jString(c, chave, sizeof chave)) return 0;
    jEspaco(c);
    if (c->p >= c->fim || *c->p != ':') return 0;
    c->p++;
    jEspaco(c);
    if (!strcmp(chave, "tmdb")) {
      long long v = 0;
      if (jLit(c, "null")) {
        out->tmdb = 0;
      } else if (jInt(c, &v)) {
        if (v > 0 && v <= 2000000000LL) out->tmdb = (long)v;
      } else {
        return 0;
      }
    } else if (!strcmp(chave, "imdb")) {
      char buf[32];
      if (jLit(c, "null")) {
        out->imdb[0] = '\0';
      } else if (jString(c, buf, sizeof buf)) {
        if (imdbValido(buf)) copiarStr(out->imdb, sizeof out->imdb, buf);
      } else {
        return 0;
      }
    } else if (!strcmp(chave, "tipo")) {
      char buf[16];
      if (jLit(c, "null")) {
        strcpy(out->tipo, "movie");
      } else if (jString(c, buf, sizeof buf)) {
        normalizarTipoItem(buf, out->tipo);
      } else {
        return 0;
      }
    } else {
      if (!jPularValor(c, 2)) return 0;
    }
    jEspaco(c);
    if (c->p >= c->fim) return 0;
    if (*c->p == '}') { c->p++; return 1; }
    if (*c->p != ',') return 0;
    c->p++;
  }
}

static int lerArrayIds(JCur *c, FarolBuscaResp *out) {
  jEspaco(c);
  if (c->p >= c->fim || *c->p != '[') return 0;
  c->p++;
  jEspaco(c);
  if (c->p < c->fim && *c->p == ']') { c->p++; return 1; }
  for (;;) {
    FarolBuscaId item;
    if (!lerUmId(c, &item)) return 0;
    if ((item.tmdb > 0 || item.imdb[0]) && out->nIds < FAROL_BUSCA_IDS_MAX) {
      int dup = 0, i;
      for (i = 0; i < out->nIds; i++) {
        if (item.imdb[0] && out->ids[i].imdb[0] && !strcmp(item.imdb, out->ids[i].imdb))
          { dup = 1; break; }
        if (item.tmdb > 0 && out->ids[i].tmdb == item.tmdb &&
            !strcmp(item.tipo, out->ids[i].tipo))
          { dup = 1; break; }
      }
      if (!dup) out->ids[out->nIds++] = item;
    }
    jEspaco(c);
    if (c->p >= c->fim) return 0;
    if (*c->p == ']') { c->p++; return 1; }
    if (*c->p != ',') return 0;
    c->p++;
  }
}

static int lerFiltro(JCur *c, FarolBuscaFiltro *f) {
  jEspaco(c);
  if (jLit(c, "null")) return 1;
  if (c->p >= c->fim || *c->p != '{') return 0;
  c->p++;
  jEspaco(c);
  if (c->p < c->fim && *c->p == '}') { c->p++; return 1; }
  for (;;) {
    char chave[32];
    if (!jString(c, chave, sizeof chave)) return 0;
    jEspaco(c);
    if (c->p >= c->fim || *c->p != ':') return 0;
    c->p++;
    jEspaco(c);
    if (!strcmp(chave, "generos")) {
      if (!jLit(c, "null")) {
        if (c->p >= c->fim || *c->p != '[') return 0;
        c->p++;
        jEspaco(c);
        if (c->p < c->fim && *c->p == ']') {
          c->p++;
        } else {
          for (;;) {
            char g[32];
            if (!jString(c, g, sizeof g)) return 0;
            if (jSlugGeneroValido(g) && f->nGeneros < FAROL_BUSCA_FIL_GEN_MAX) {
              int dup = 0, i;
              for (i = 0; i < f->nGeneros; i++)
                if (!strcmp(f->generos[i], g)) { dup = 1; break; }
              if (!dup) copiarStr(f->generos[f->nGeneros++], sizeof f->generos[0], g);
            }
            jEspaco(c);
            if (c->p >= c->fim) return 0;
            if (*c->p == ']') { c->p++; break; }
            if (*c->p != ',') return 0;
            c->p++;
          }
        }
      }
    } else if (!strcmp(chave, "duracao_max")) {
      long long v = 0;
      if (jLit(c, "null")) f->duracao_max = 0;
      else if (jInt(c, &v)) f->duracao_max = (v >= 15 && v <= 600) ? (int)v : 0;
      else return 0;
    } else if (!strcmp(chave, "decada")) {
      long long v = 0;
      if (jLit(c, "null")) f->decada = 0;
      else if (jInt(c, &v))
        f->decada = (v >= 1900 && v <= 2030 && (v % 10) == 0) ? (int)v : 0;
      else return 0;
    } else if (!strcmp(chave, "parecido_com")) {
      char buf[FAROL_BUSCA_PARECIDO_MAX];
      if (jLit(c, "null")) f->parecido_com[0] = '\0';
      else if (jString(c, buf, sizeof buf)) {
        if (!temControleProibido(buf, 0) && !temUrl(buf))
          snprintf(f->parecido_com, sizeof f->parecido_com, "%s", buf);
      } else return 0;
    } else if (!strcmp(chave, "tipo")) {
      char buf[24];
      if (jLit(c, "null")) strcpy(f->tipo, "qualquer");
      else if (jString(c, buf, sizeof buf)) normalizarTipoFiltro(buf, f->tipo);
      else return 0;
    } else {
      if (!jPularValor(c, 2)) return 0;
    }
    jEspaco(c);
    if (c->p >= c->fim) return 0;
    if (*c->p == '}') { c->p++; return 1; }
    if (*c->p != ',') return 0;
    c->p++;
  }
}

int farolbusca_parsear_resposta(const char *json, size_t len,
                                FarolBuscaResp *out) {
  JCur c;
  int viuIds = 0;
  if (!out) return 0;
  memset(out, 0, sizeof *out);
  strcpy(out->filtro.tipo, "qualquer");
  if (!json || len == 0) return 0;
  c.p = json;
  c.fim = json + len;
  jEspaco(&c);
  if (c.p >= c.fim || *c.p != '{') goto falhou;
  c.p++;
  jEspaco(&c);
  if (c.p < c.fim && *c.p == '}') { c.p++; goto conferir; }
  for (;;) {
    char chave[32];
    if (!jString(&c, chave, sizeof chave)) goto falhou;
    jEspaco(&c);
    if (c.p >= c.fim || *c.p != ':') goto falhou;
    c.p++;
    jEspaco(&c);
    if (!strcmp(chave, "ok")) {
      long long v = 0;
      if (jLit(&c, "true")) out->ok = 1;
      else if (jLit(&c, "false")) out->ok = 0;
      else if (jInt(&c, &v)) out->ok = (v == 1);
      else goto falhou;
    } else if (!strcmp(chave, "ids")) {
      if (!lerArrayIds(&c, out)) goto falhou;
      viuIds = 1;
    } else if (!strcmp(chave, "filtro")) {
      if (!lerFiltro(&c, &out->filtro)) goto falhou;
    } else if (!strcmp(chave, "origem")) {
      char buf[24];
      if (jLit(&c, "null")) out->origem = FB_ORIG_NENHUMA;
      else if (jString(&c, buf, sizeof buf)) {
        if (!strcmp(buf, "llm")) out->origem = FB_ORIG_LLM;
        else if (!strcmp(buf, "palavras")) out->origem = FB_ORIG_PALAVRAS;
        else out->origem = FB_ORIG_NENHUMA;
      } else goto falhou;
    } else if (!strcmp(chave, "llm_status")) {
      long long v = 0;
      if (jLit(&c, "null")) out->llm_status = 0;
      else if (jInt(&c, &v)) out->llm_status = (v >= 0 && v <= 999) ? (int)v : 0;
      else goto falhou;
    } else {
      if (!jPularValor(&c, 1)) goto falhou;
    }
    jEspaco(&c);
    if (c.p >= c.fim) goto falhou;
    if (*c.p == '}') { c.p++; break; }
    if (*c.p != ',') goto falhou;
    c.p++;
  }
conferir:
  jEspaco(&c);
  if (c.p != c.fim) goto falhou;
  if (!out->ok || !viuIds) goto falhou;
  return 1;
falhou:
  memset(out, 0, sizeof *out);
  return 0;
}

// ----------------------------------------- mapeamento para intencao local --

static char dobrarLatim(long cp) {
  if (cp >= 'A' && cp <= 'Z') return (char)(cp - 'A' + 'a');
  if ((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9')) return (char)cp;
  switch (cp) {
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4:
    case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E4: return 'a';
    case 0x00C7: case 0x00E7: return 'c';
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: return 'e';
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: return 'i';
    case 0x00D1: case 0x00F1: return 'n';
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6:
    case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6: return 'o';
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: return 'u';
    default: return ' ';
  }
}

static void dobrarFrase(const char *src, char *dst, size_t cap) {
  const char *p, *fim;
  size_t u = 0;
  int esp = 1;
  if (!dst || cap < 2) return;
  dst[0] = '\0';
  if (!src) return;
  p = src;
  fim = src + strlen(src);
  while (p < fim && u + 1 < cap) {
    long cp = lerUtf8(&p, fim);
    char ch;
    if (cp < 0) { p++; continue; }
    ch = dobrarLatim(cp);
    if (ch == ' ') {
      if (!esp && u + 1 < cap) { dst[u++] = ' '; esp = 1; }
    } else {
      dst[u++] = ch;
      esp = 0;
    }
  }
  while (u > 0 && dst[u - 1] == ' ') u--;
  dst[u] = '\0';
}

static int contemPalavra(const char *txt, const char *termo) {
  size_t n = strlen(termo);
  const char *p = txt;
  if (!n) return 0;
  while ((p = strstr(p, termo)) != NULL) {
    int antes = (p == txt || p[-1] == ' ');
    int depois = (p[n] == '\0' || p[n] == ' ');
    if (antes && depois) return 1;
    p++;
  }
  return 0;
}

static int extrairMinutosFrase(const char *dob) {
  const char *p = dob;
  if (contemPalavra(dob, "meia hora") || contemPalavra(dob, "half hour"))
    return 30;
  if (contemPalavra(dob, "uma hora e meia") || contemPalavra(dob, "hour and a half"))
    return 90;
  if (contemPalavra(dob, "uma hora") || contemPalavra(dob, "one hour"))
    return 60;
  if (contemPalavra(dob, "duas horas") || contemPalavra(dob, "two hours"))
    return 120;
  while (*p) {
    if (isdigit((unsigned char)*p) && (p == dob || !isalnum((unsigned char)p[-1]))) {
      long v = 0;
      const char *q = p;
      while (isdigit((unsigned char)*q) && v < 10000) v = v * 10 + (*q++ - '0');
      while (*q == ' ') q++;
      if (*q == 'h' && (!isalpha((unsigned char)q[1]) ||
          !strncmp(q, "hora", 4) || !strncmp(q, "hour", 4) || !strncmp(q, "hr", 2))) {
        long min = v * 60;
        const char *r = q + 1;
        if (!strncmp(q, "horas", 5)) r = q + 5;
        else if (!strncmp(q, "hora", 4)) r = q + 4;
        else if (!strncmp(q, "hours", 5)) r = q + 5;
        else if (!strncmp(q, "hour", 4)) r = q + 4;
        else if (!strncmp(q, "hrs", 3)) r = q + 3;
        else if (!strncmp(q, "hr", 2)) r = q + 2;
        while (*r == ' ') r++;
        if (isdigit((unsigned char)*r)) {
          long m2 = 0;
          while (isdigit((unsigned char)*r) && m2 < 100) m2 = m2 * 10 + (*r++ - '0');
          if (m2 < 60) min += m2;
        }
        if (min >= 15 && min <= 240) return (int)min;
      } else if (!strncmp(q, "min", 3) || (*q == 'm' && (q[1] == '\0' || q[1] == ' '))) {
        if (v >= 15 && v <= 240) return (int)v;
      }
      p = q;
      continue;
    }
    p++;
  }
  return 0;
}

void farolbusca_intencao_local(const char *frase, FarolBuscaLocal *out) {
  static const struct { const char *termo; const char *canonico; } GENEROS[] = {
    { "suspense", "Suspense" }, { "thriller", "Suspense" },
    { "terror", "Terror" }, { "horror", "Terror" },
    { "comedia", "Comédia" }, { "comedy", "Comédia" }, { "rir", "Comédia" },
    { "drama", "Drama" },
    { "acao", "Ação" }, { "action", "Ação" },
    { "aventura", "Aventura" }, { "adventure", "Aventura" },
    { "animacao", "Animação" }, { "animation", "Animação" },
    { "crime", "Crime" }, { "policial", "Crime" },
    { "documentario", "Documentário" }, { "documentary", "Documentário" },
    { "familia", "Família" }, { "family", "Família" },
    { "fantasia", "Fantasia" }, { "fantasy", "Fantasia" },
    { "faroeste", "Faroeste" }, { "western", "Faroeste" },
    { "ficcao cientifica", "Ficção científica" }, { "ficcao", "Ficção científica" },
    { "sci fi", "Ficção científica" }, { "science fiction", "Ficção científica" },
    { "guerra", "Guerra" }, { "war", "Guerra" },
    { "historia", "História" }, { "history", "História" },
    { "misterio", "Mistério" }, { "mystery", "Mistério" },
    { "musica", "Música" }, { "musical", "Música" }, { "music", "Música" },
    { "romance", "Romance" }, { "romantico", "Romance" }, { "romantica", "Romance" }
  };
  static const char *const RETOMAR[] = {
    "continuar", "retomar", "onde parei", "parado", "parada",
    "continue", "resume", "left off", "pick up"
  };
  static const char *const CURTO[] = {
    "curto", "curta", "curtos", "curtas", "rapido", "rapida",
    "pouco tempo", "short", "quick", "brief", "menos de"
  };
  static const char *const NAO_VI[] = {
    "nao vi", "nunca vi", "ainda nao", "inedito", "inedita",
    "novo", "nova", "novidade", "unseen", "never seen", "haven t seen", "new"
  };
  char dob[FAROL_BUSCA_FRASE_BYTES];
  size_t i;
  int min;

  if (!out) return;
  memset(out, 0, sizeof *out);
  out->intencao = FA_SURPRESA;
  if (!frase || !frase[0]) return;

  dobrarFrase(frase, dob, sizeof dob);

  for (i = 0; i < sizeof GENEROS / sizeof GENEROS[0]; i++) {
    if (contemPalavra(dob, GENEROS[i].termo)) {
      snprintf(out->genero, sizeof out->genero, "%s", GENEROS[i].canonico);
      break;
    }
  }

  for (i = 0; i < sizeof RETOMAR / sizeof RETOMAR[0]; i++) {
    if (contemPalavra(dob, RETOMAR[i])) {
      out->intencao = FA_RETOMAR;
      return;
    }
  }

  min = extrairMinutosFrase(dob);
  if (min > 0) {
    out->intencao = FA_CURTO;
    out->maxMin = min;
    return;
  }
  for (i = 0; i < sizeof CURTO / sizeof CURTO[0]; i++) {
    if (contemPalavra(dob, CURTO[i])) {
      out->intencao = FA_CURTO;
      out->maxMin = FAROL_ACHA_CURTO_PADRAO;
      return;
    }
  }

  for (i = 0; i < sizeof NAO_VI / sizeof NAO_VI[0]; i++) {
    if (contemPalavra(dob, NAO_VI[i])) {
      out->intencao = FA_NAO_VI;
      return;
    }
  }

  out->intencao = FA_SURPRESA;
}

const char *farolbusca_frase_estado(FarolBuscaMotivo motivo) {
  switch (motivo) {
    case FB_MOTIVO_BUSCANDO:
      return FT_BUSCA_FRASE_PROCURANDO;
    case FB_MOTIVO_LIMITE:
      return FT_BUSCA_FRASE_LIMITE;
    case FB_MOTIVO_OFFLINE:
    case FB_MOTIVO_PRAZO:
    case FB_MOTIVO_ERRO:
    case FB_MOTIVO_VAZIO:
      return FT_BUSCA_FRASE_OFFLINE;
    default:
      return "";
  }
}
