#include "posterprov.h"
#include "rede.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

// Ver posterprov.h. Este arquivo NAO desenha e NAO baixa: monta URL, guarda
// quais itens o provedor nao serviu e segura o ritmo dos downloads.

// ---------------------------------------------------------------- texto
static int digito(int c) { return c >= '0' && c <= '9'; }
static int alfa(int c)   { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int minusc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static const char *pula_espaco(const char *s) {
  while (s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
  return s;
}
// Copia sem espaco nas pontas; falha se estourar `n`.
static int aparar(const char *s, char *out, size_t n) {
  size_t k;
  s = pula_espaco(s);
  k = s ? strlen(s) : 0;
  while (k && (s[k - 1] == ' ' || s[k - 1] == '\t' || s[k - 1] == '\r' || s[k - 1] == '\n')) k--;
  if (k + 1 > n) return 0;
  memcpy(out, s, k);
  out[k] = 0;
  return 1;
}

// ---------------------------------------------------------------- instancia
static int host_local(const char *h, size_t n) {
  size_t i, pontos = 0, digitos = 0;
  { const char *dp = memchr(h, ':', n); if (dp) n = (size_t)(dp - h); }   // sem a porta
  if (n == 9 && !strncmp(h, "localhost", 9)) return 1;
  if (n >= 6 && !strncmp(h + n - 6, ".local", 6)) return 1;
  for (i = 0; i < n; i++) {
    if (h[i] == '.') pontos++;
    else if (digito(h[i])) digitos++;
    else if (h[i] == ':') break;          // porta: nao conta como letra
    else return 0;
  }
  return pontos == 3 && digitos >= 4;     // IPv4
}

int posterprov_normalizar_instancia(const char *entrada, char *out, size_t n) {
  char b[300], host[PP_INSTANCIA_MAX];
  const char *p, *h;
  const char *esquema = NULL;
  size_t k = 0, i;
  if (out && n) out[0] = 0;
  if (!entrada || !out || n < 12 || !aparar(entrada, b, sizeof b) || !b[0]) return 0;
  p = b;
  if (!strncasecmp(p, "stremio://", 10)) { esquema = "https"; p += 10; }
  else if (!strncasecmp(p, "https://", 8)) { esquema = "https"; p += 8; }
  else if (!strncasecmp(p, "http://", 7))  { esquema = "http";  p += 7; }
  else if (strstr(p, "://")) return 0;            // ftp:// etc.
  h = p;
  while (*p && *p != '/' && *p != '?' && *p != '#') p++;
  if (p == h || (size_t)(p - h) >= sizeof host) return 0;
  for (i = 0; h + i < p; i++) {
    int c = (unsigned char)h[i];
    if (!(alfa(c) || digito(c) || c == '.' || c == '-' || c == ':')) return 0;   // '@', '[' ...
    host[k++] = (char)minusc(c);
  }
  host[k] = 0;
  if (host[0] == '.' || host[0] == '-' || host[0] == ':') return 0;
  // Um host sem ponto so vale se for localhost ou tiver porta (nome de rede).
  if (!strchr(host, '.') && strcmp(host, "localhost") && !strchr(host, ':')) return 0;
  if (!esquema) esquema = host_local(host, k) ? "http" : "https";
  if (snprintf(out, n, "%s://%s", esquema, host) >= (int)n) { out[0] = 0; return 0; }
  return 1;
}

// ---------------------------------------------------------------- token
static int char_token(int c) {
  return alfa(c) || digito(c) || c == '_' || c == '.' || c == '~' || c == '=' || c == '-';
}
static int token_valido(const char *t, size_t k) {
  size_t i;
  if (!k || k > PP_TOKEN_MAX) return 0;
  for (i = 0; i < k; i++) if (!char_token((unsigned char)t[i])) return 0;
  return 1;
}

int posterprov_extrair_token(const char *entrada, char *tok, size_t n,
                             char *inst, size_t ni) {
  char b[900];
  const char *ini = NULL, *fim;
  if (tok && n) tok[0] = 0;
  if (inst && ni) inst[0] = 0;
  if (!tok || n < 2) return 0;
  if (!entrada || !aparar(entrada, b, sizeof b)) return 0;
  if (!b[0]) return 1;                                    // vazio = apagar
  if (strstr(b, "://")) {
    const char *c = strstr(b, "/c/");
    const char *q = strchr(b, '?');
    if (inst && ni) posterprov_normalizar_instancia(b, inst, ni);
    if (c && (!q || c < q)) ini = c + 3;
    else if (q) {
      const char *k = strstr(q, "config=");
      if (k && (k == q + 1 || k[-1] == '&')) ini = k + 7;
      else { k = strstr(q, "c="); if (k && (k == q + 1 || k[-1] == '&')) ini = k + 2; }
    }
    if (!ini) return 0;
    fim = ini;
    while (*fim && *fim != '/' && *fim != '?' && *fim != '#' && *fim != '&') fim++;
  } else {
    ini = b;
    fim = b + strlen(b);
  }
  if (!token_valido(ini, (size_t)(fim - ini)) || (size_t)(fim - ini) + 1 > n) {
    if (inst && ni) inst[0] = 0;
    return 0;
  }
  memcpy(tok, ini, (size_t)(fim - ini));
  tok[fim - ini] = 0;
  return 1;
}

// ---------------------------------------------------------------- extras
static int chave_extra(const char *k, size_t n, const char *nome) {
  return strlen(nome) == n && !strncmp(k, nome, n);
}
// 1 quando algum parametro de `q` (k=v&k=v) tem a chave `nome`.
static int tem_chave(const char *q, const char *nome) {
  const char *p = q;
  while (*p) {
    const char *e = p;
    while (*e && *e != '&' && *e != '=') e++;
    if (chave_extra(p, (size_t)(e - p), nome)) return 1;
    while (*e && *e != '&') e++;
    p = *e ? e + 1 : e;
  }
  return 0;
}
int posterprov_extra_normalizar(const char *entrada, char *out, size_t n) {
  char b[300];
  const char *p;
  size_t i, k = 0;
  if (out && n) out[0] = 0;
  if (!out || n < 2 || !entrada || !aparar(entrada, b, sizeof b)) return 0;
  p = b;
  while (*p == '?' || *p == '&') p++;
  for (i = 0; p[i]; i++) {
    int c = (unsigned char)p[i];
    if (!(alfa(c) || digito(c) || c == '_' || c == '=' || c == '&' || c == '.' || c == ',' ||
          c == '%' || c == '-')) return 0;
    if (k + 1 >= n || k >= PP_EXTRA_MAX) return 0;
    out[k++] = (char)c;
  }
  while (k && out[k - 1] == '&') k--;
  out[k] = 0;
  if (tem_chave(out, "fmt") || tem_chave(out, "format") || tem_chave(out, "config") ||
      tem_chave(out, "c")) { out[0] = 0; return 0; }
  return 1;
}

// ---------------------------------------------------------------- modelo
// Decode only template punctuation; credentials and other escapes stay byte-identical.
static int modelo_decodificar(const char *src, char *out) {
  size_t k = 0;
  if (!src || strlen(src) >= PP_MODELO_MAX) return 0;
  while (*src) {
    if (src[0] == '%' && src[1] == '7' && src[2]) {
      int c = minusc((unsigned char)src[2]);
      if (c == 'b' || c == 'c' || c == 'd') {
        out[k++] = c == 'b' ? '{' : c == 'c' ? '|' : '}';
        src += 3;
        continue;
      }
    }
    out[k++] = *src++;
  }
  out[k] = 0;
  return 1;
}

static int marcador_simples(const char *p, size_t k) {
  if (k > 2 && p[k - 2] == '?') k--;  // Optional markers use the same names.
  return (k == 6 && !strncmp(p, "{imdb", 5)) || (k == 6 && !strncmp(p, "{tmdb", 5)) ||
         (k == 6 && !strncmp(p, "{type", 5)) || (k == 11 && !strncmp(p, "{tipo_tmdb", 10)) ||
         (k == 9 && !strncmp(p, "{imdb_id", 8)) || (k == 9 && !strncmp(p, "{tmdb_id", 8)) ||
         (k == 4 && !strncmp(p, "{id", 3)) || (k == 7 && !strncmp(p, "{shape", 6)) ||
         (k == 9 && !strncmp(p, "{id_type", 8)) || (k == 10 && !strncmp(p, "{typed_id", 9));
}
// Pior caso de cada marcador montado (posterprov_montar_url): {imdb} vira o
// Ident.tt (ate 23), {tmdb} um long (ate 19 digitos), {type} "series" (6),
// {tipo_tmdb} "movie" (5). Os aliases *_id seguem o original; {id} usa o
// maior deles (23). So posterprov_modelo_cabe() usa: ver o .h.
#define PP_MAX_IMDB 23
#define PP_MAX_TMDB 19
static size_t montado_simples_max(const char *p, size_t k) {
  if (k > 2 && p[k - 2] == '?') k--;
  if ((k == 6 && !strncmp(p, "{imdb", 5)) || (k == 9 && !strncmp(p, "{imdb_id", 8)) ||
      (k == 4 && !strncmp(p, "{id", 3))) return PP_MAX_IMDB;
  if ((k == 6 && !strncmp(p, "{tmdb", 5)) || (k == 9 && !strncmp(p, "{tmdb_id", 8))) return PP_MAX_TMDB;
  if (k == 9) return 4;                                   // {id_type}: imdb/tmdb
  if (k == 10) return PP_MAX_TMDB + 7;                     // {typed_id}: series-N
  if (k == 7) return 9;                                   // {shape}: landscape
  if (k == 6) return 6;                                   // {type}: "series"
  return 5;                                               // {tipo_tmdb}: "movie"
}
// Validate every alternative and reserve the longest possible expansion.
static size_t marcador_max(const char *p, size_t k) {
  const char *end = p + k - 1;
  size_t max = 0;
  if (end[-1] == '?') end--;
  for (p++; p < end;) {
    const char *bar = memchr(p, '|', (size_t)(end - p));
    size_t len = (size_t)((bar ? bar : end) - p);
    char part[32];
    if (!len || len + 3 > sizeof part) return 0;
    part[0] = '{'; memcpy(part + 1, p, len); part[len + 1] = '}';
    // Optional applies to the whole token, never an individual alternative.
    if (memchr(p, '?', len) || !marcador_simples(part, len + 2)) return 0;
    size_t n = montado_simples_max(part, len + 2);
    if (n > max) max = n;
    if (!bar) return max;
    p = bar + 1;
  }
  return 0;
}

// Regras de sintaxe; com `pior` nao nulo, soma tambem o tamanho montado no
// pior caso.
static int modelo_sintaxe(const char *modelo, size_t *pior_out) {
  const char *p;
  char decoded[PP_MODELO_MAX];
  int achou = 0;
  size_t pior = 0;
  if (!modelo_decodificar(modelo, decoded)) return 0;
  modelo = decoded;
  if (strncmp(modelo, "http://", 7) && strncmp(modelo, "https://", 8)) return 0;
  for (p = modelo; *p; p++) {
    if ((unsigned char)*p <= ' ' || (unsigned char)*p == 0x7f) return 0;
    if (*p == '}') return 0;                 // '}' sem '{'
    if (*p == '{') {
      const char *f = strchr(p, '}');
      size_t max = f ? marcador_max(p, (size_t)(f - p + 1)) : 0;
      if (!max) return 0;
      pior += max;
      achou = 1;
      p = f;
    } else {
      pior++;
    }
  }
  if (pior_out) *pior_out = pior;
  return achou;
}
int posterprov_modelo_valido(const char *modelo) {
  return modelo_sintaxe(modelo, NULL);
}
int posterprov_modelo_cabe(const char *modelo) {
  size_t pior = 0;
  return modelo_sintaxe(modelo, &pior) && pior < PP_URL_MAX;
}

// ---------------------------------------------------------------- montagem
// Caminho do endpoint do SpatialPosters. Constante a parte, e nao dentro do
// formato do snprintf: tools/varredura-i18n.py toma "poster" por portugues.
#define PP_CAMINHO_API "/api/poster/"

typedef struct { char tt[24]; long tm; int serie; } Ident;

static int ident(const char *imdb, long tmdb, const char *tipo, Ident *d) {
  memset(d, 0, sizeof *d);
  if (!tipo) return 0;
  if (!strcmp(tipo, "movie") || !strcmp(tipo, "film")) d->serie = 0;
  else if (!strcmp(tipo, "series") || !strcmp(tipo, "tv") || !strcmp(tipo, "show")) d->serie = 1;
  else return 0;                                    // canal, tv ao vivo, pessoa...
  d->tm = tmdb > 0 ? tmdb : 0;
  if (imdb && imdb[0] == 't' && imdb[1] == 't') {
    size_t i = 2;
    while (digito((unsigned char)imdb[i])) i++;
    if (i >= 5 && i < sizeof d->tt && !imdb[i]) memcpy(d->tt, imdb, i + 1);
  } else if (imdb && !strncmp(imdb, "tmdb:", 5)) {
    const char *p = imdb + 5;
    char *e;
    long v;
    if (*p == 'm' || *p == 't') p++;                // "tmdb:m278" / "tmdb:t1399"
    v = strtol(p, &e, 10);
    if (e != p && !*e && v > 0 && !d->tm) d->tm = v;
  }
  return d->tt[0] || d->tm > 0;
}

static int chave_rpdb_valida(const char *k) {
  size_t i, n = k ? strlen(k) : 0;
  if (!n || n >= PP_CHAVE_MAX) return 0;
  for (i = 0; i < n; i++)
    if (!(alfa((unsigned char)k[i]) || digito((unsigned char)k[i]) || k[i] == '_' || k[i] == '-')) return 0;
  return 1;
}

static int montar_formato(const PosterProvCfg *c, const char *imdb, long tmdb,
                          const char *tipo, int landscape, char *out, size_t n) {
  Ident d;
  int w = -1;
  if (out && n) out[0] = 0;
  if (!c || !out || n < 32 || !ident(imdb, tmdb, tipo, &d)) return 0;
  switch (c->prov) {
    case PP_SPATIAL: {
      char inst[PP_INSTANCIA_MAX];
      char id[24];
      if (d.tt[0]) snprintf(id, sizeof id, "%s", d.tt);      // estavel: ver posterprov.h
      else snprintf(id, sizeof id, "%ld", d.tm);
      if (!c->instancia[0]) snprintf(inst, sizeof inst, "%s", PP_INSTANCIA_PADRAO);
      else snprintf(inst, sizeof inst, "%s", c->instancia);
      // fmt=jpeg explicito: sem ele o servidor negocia por Accept (Vary) e o
      // navegador do Tizen recebe WebP enquanto o LG recebe JPEG — dois
      // arquivos, dois caches, e nenhum ganho de tamanho medido.
      { char q[PP_URL_MAX];
        int qn = snprintf(q, sizeof q, "fmt=jpeg");
        // O idioma da interface, salvo se a pessoa mandou o dela nos extras:
        // sem ele o servidor usa o idioma padrao dele, e os selos saem em outra
        // lingua que a da interface (medido: o poster muda de bytes com lang=).
        if (c->lang[0] && !tem_chave(c->extra, "lang") && qn > 0 && (size_t)qn < sizeof q)
          qn += snprintf(q + qn, sizeof q - (size_t)qn, "&lang=%.7s", c->lang);
        if (c->extra[0] && qn > 0 && (size_t)qn < sizeof q)
          qn += snprintf(q + qn, sizeof q - (size_t)qn, "&%s", c->extra);
        if (c->token[0] && token_valido(c->token, strlen(c->token)) && qn > 0 && (size_t)qn < sizeof q)
          qn += snprintf(q + qn, sizeof q - (size_t)qn, "&config=%s", c->token);
        if (qn < 0 || (size_t)qn >= sizeof q) return 0;
        w = snprintf(out, n, "%s%s%s/%s?%s", inst, PP_CAMINHO_API,
                     d.serie ? "series" : "movie", id, q); }
      break;
    }
    case PP_RPDB:
      if (!chave_rpdb_valida(c->chave)) return 0;
      if (d.tt[0])
        w = snprintf(out, n, "https://api.ratingposterdb.com/%s/imdb/poster-default/%s.jpg?fallback=true",
                     c->chave, d.tt);
      else
        w = snprintf(out, n, "https://api.ratingposterdb.com/%s/tmdb/poster-default/%s-%ld.jpg?fallback=true",
                     c->chave, d.serie ? "series" : "movie", d.tm);
      break;
    case PP_MODELO: {
      char decoded[PP_MODELO_MAX];
      const char *p = decoded;
      size_t k = 0;
      if (!modelo_decodificar(c->modelo, decoded) || !posterprov_modelo_valido(p)) return 0;
      while (*p) {
        char v[32] = "";
        size_t lv, adv = 1;
        if (*p == '{') {
          adv = (size_t)(strchr(p, '}') - p + 1);   // sintaxe ja validada
          int opcional = p[adv - 2] == '?';
          const char *part = p + 1;
          const char *end = p + adv - 1 - opcional;
          while (part < end && !v[0]) {
            const char *bar = memchr(part, '|', (size_t)(end - part));
            size_t len = (size_t)((bar ? bar : end) - part);
            if (len >= 4 && !strncmp(part, "imdb", 4)) {
              snprintf(v, sizeof v, "%s", d.tt);
            } else if (len >= 4 && !strncmp(part, "tmdb", 4)) {
              if (d.tm > 0) snprintf(v, sizeof v, "%ld", d.tm);
            } else if (len == 7 && !strncmp(part, "id_type", len)) {
              snprintf(v, sizeof v, "%s", d.tt[0] ? "imdb" : "tmdb");
            } else if (len == 8 && !strncmp(part, "typed_id", len)) {
              if (d.tt[0]) snprintf(v, sizeof v, "%s", d.tt);
              else snprintf(v, sizeof v, "%s-%ld", d.serie ? "series" : "movie", d.tm);
            } else if (len == 2 && !strncmp(part, "id", len)) {
              if (d.tt[0]) snprintf(v, sizeof v, "%s", d.tt);
              else snprintf(v, sizeof v, "%ld", d.tm);
            } else if (len == 5 && !strncmp(part, "shape", len)) {
              snprintf(v, sizeof v, "%s", landscape ? "landscape" : "poster");
            } else if (len == 4 && !strncmp(part, "type", len)) {
              snprintf(v, sizeof v, "%s", d.serie ? "series" : "movie");
            } else { snprintf(v, sizeof v, "%s", d.serie ? "tv" : "movie"); }
            if (!bar) break;
            part = bar + 1;
          }
          if (!v[0] && !opcional) return 0;
          lv = strlen(v);
          if (k + lv + 1 > n) return 0;
          memcpy(out + k, v, lv); k += lv;
          p += adv;
        } else {
          if (k + 2 > n) return 0;
          out[k++] = *p++;
        }
      }
      out[k] = 0;
      w = (int)k;
      break;
    }
    default:
      return 0;
  }
  if (w < 0 || (size_t)w >= n || w >= PP_URL_MAX) { out[0] = 0; return 0; }
  return 1;
}

int posterprov_montar_url(const PosterProvCfg *c, const char *imdb, long tmdb,
                          const char *tipo, char *out, size_t n) {
  return montar_formato(c, imdb, tmdb, tipo, 0, out, n);
}

const char *posterprov_redigir(const char *url, char *out, size_t n) {
  return rede_url_publica(url, out, (unsigned)n);
}

// ---------------------------------------------------------------- estado
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static PosterProvCfg cfg;
static char prefixo[PP_INSTANCIA_MAX + 8];      // esquema://host do provedor

// Falhas da sessao: conjunto de hashes, sondagem linear, nunca despejado (uma
// sessao inteira de home nao passa de algumas centenas de itens). Cheio de
// verdade (>= 3/4), para de anotar: o item so volta a ser tentado, o que e
// barato (o tex_cache lembra que falhou).
#define PP_FALHAS 4096
static uint32_t falhou[PP_FALHAS];
static int nFalhou, seguidas;
static long disjuntorAte;

#define PP_SLOTS 256
typedef struct { uint32_t chave; char ok; char url[PP_URL_MAX]; } Slot;
static Slot slots[PP_SLOTS];

static int (*hookEstado)(const char *url);
static long (*hookRelogio)(void);
static long agora(void) { return hookRelogio ? hookRelogio() : (long)time(NULL); }

static void prefixo_de(const PosterProvCfg *c) {
  const char *m, *e;
  prefixo[0] = 0;
  switch (c->prov) {
    case PP_SPATIAL:
      snprintf(prefixo, sizeof prefixo, "%s", c->instancia[0] ? c->instancia : PP_INSTANCIA_PADRAO);
      break;
    case PP_RPDB:
      snprintf(prefixo, sizeof prefixo, "https://api.ratingposterdb.com");
      break;
    case PP_MODELO:
      m = c->modelo;
      e = strstr(m, "://");
      if (!e) break;
      e += 3;
      while (*e && *e != '/' && *e != '?' && *e != '{') e++;
      if (*e == '{' || (size_t)(e - m) >= sizeof prefixo) break;   // host variavel: sem portao
      memcpy(prefixo, m, (size_t)(e - m));
      prefixo[e - m] = 0;
      break;
  }
}

void posterprov_configurar(const PosterProvCfg *c) {
  pthread_mutex_lock(&trava);
  memset(&cfg, 0, sizeof cfg);
  if (c) cfg = *c;
  cfg.instancia[sizeof cfg.instancia - 1] = 0;
  cfg.token[sizeof cfg.token - 1] = 0;
  cfg.extra[sizeof cfg.extra - 1] = 0;
  cfg.lang[sizeof cfg.lang - 1] = 0;
  cfg.chave[sizeof cfg.chave - 1] = 0;
  cfg.modelo[sizeof cfg.modelo - 1] = 0;
  prefixo_de(&cfg);
  memset(falhou, 0, sizeof falhou);
  memset(slots, 0, sizeof slots);
  nFalhou = seguidas = 0;
  disjuntorAte = 0;
  pthread_mutex_unlock(&trava);
}

const PosterProvCfg *posterprov_cfg(void) { return &cfg; }

int posterprov_ativo(void) {
  PosterProvCfg t;
  char u[PP_URL_MAX];
  if (cfg.prov == PP_DESLIGADO) return 0;
  t = cfg;
  // "configurado" = existe pelo menos um titulo de exemplo que vira URL.
  return posterprov_montar_url(&t, "tt0111161", 278, "movie", u, sizeof u);
}

void posterprov_hook_estado(int (*fn)(const char *)) { hookEstado = fn; }
void posterprov_hook_relogio(long (*fn)(void)) { hookRelogio = fn; }

static uint32_t hash_item(const char *imdb, long tmdb, const char *tipo) {
  uint32_t h = 2166136261u;
  const char *p;
  for (p = imdb ? imdb : ""; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
  h = (h ^ '|') * 16777619u;
  h = (h ^ (uint32_t)tmdb) * 16777619u;
  for (p = tipo ? tipo : ""; *p; p++) h = (h ^ (unsigned char)*p) * 16777619u;
  return h | 1u;                                  // 0 = vazio
}

static int falhou_tem(uint32_t k) {
  uint32_t i = k % PP_FALHAS;
  int p;
  for (p = 0; p < PP_FALHAS; p++, i = (i + 1) % PP_FALHAS) {
    if (falhou[i] == k) return 1;
    if (!falhou[i]) return 0;
  }
  return 0;
}
static void falhou_poe(uint32_t k) {
  uint32_t i = k % PP_FALHAS;
  int p;
  if (nFalhou >= PP_FALHAS * 3 / 4) return;
  for (p = 0; p < PP_FALHAS; p++, i = (i + 1) % PP_FALHAS) {
    if (falhou[i] == k) return;
    if (!falhou[i]) { falhou[i] = k; nFalhou++; return; }
  }
}

#define PP_SEGUIDAS_MAX 6
#define PP_DISJUNTOR_S  300

// "POSTERES DO ADDON" (Ajustes, desligado de fabrica). Ver posterprov.h.
static int preferirAddon;
void posterprov_preferir_addon(int sim) { preferirAddon = sim ? 1 : 0; }
int posterprov_addon_vence(const char *origem, const char *orig) {
  return preferirAddon && origem && origem[0] && orig && orig[0] &&
         !strstr(orig, "images.metahub.space/");
}
const char *posterprov_card_addon(const char *origem, const char *imdb, long tmdb,
                                  const char *tipo, const char *orig) {
  if (posterprov_addon_vence(origem, orig)) return orig;
  return posterprov_card(imdb, tmdb, tipo, orig);
}

const char *posterprov_card(const char *imdb, long tmdb, const char *tipo,
                            const char *orig) {
  return posterprov_card_formato(NULL, imdb, tmdb, tipo, orig, 0);
}

const char *posterprov_card_formato(const char *origem, const char *imdb, long tmdb,
                                    const char *tipo, const char *orig, int landscape) {
  uint32_t k;
  Slot *s;
  const char *r = orig;
  if (cfg.prov == PP_DESLIGADO || posterprov_addon_vence(origem, orig)) return orig;
  if (landscape) {
    char decoded[PP_MODELO_MAX];
    if (cfg.prov != PP_MODELO || !modelo_decodificar(cfg.modelo, decoded) ||
        (!strstr(decoded, "{shape}") && !strstr(decoded, "{shape?}"))) return orig;
  }
  pthread_mutex_lock(&trava);
  if (disjuntorAte) {
    if (agora() < disjuntorAte) { pthread_mutex_unlock(&trava); return orig; }
    disjuntorAte = 0;                              // meia-abertura: tenta de novo
    seguidas = PP_SEGUIDAS_MAX - 1;                // uma falha reabre
  }
  k = hash_item(imdb, tmdb, tipo) ^ (landscape ? 0x80000000u : 0);
  if (falhou_tem(k)) { pthread_mutex_unlock(&trava); return orig; }
  s = &slots[k % PP_SLOTS];
  if (s->chave != k) {
    memset(s, 0, sizeof *s);
    s->chave = k;
    if (!montar_formato(&cfg, imdb, tmdb, tipo, landscape, s->url, sizeof s->url)) s->url[0] = 0;
  }
  if (!s->url[0]) { pthread_mutex_unlock(&trava); return orig; }   // provedor nao entende o item
  if (!s->ok && hookEstado) {
    int st = hookEstado(s->url);
    if (st < 0) {
      char l[80];
      falhou_poe(k);
      seguidas++;
      printf("[poster] falhou, usando o cartaz normal: %s\n", posterprov_redigir(s->url, l, sizeof l));
      fflush(stdout);
      if (seguidas >= PP_SEGUIDAS_MAX) {
        disjuntorAte = agora() + PP_DISJUNTOR_S;
        printf("[poster] %d falhas seguidas: provedor em pausa por %d s\n", seguidas, PP_DISJUNTOR_S);
        fflush(stdout);
      }
      pthread_mutex_unlock(&trava);
      return orig;
    }
    if (st > 0) { s->ok = 1; seguidas = 0; }
  }
  r = s->url;
  pthread_mutex_unlock(&trava);
  return r;
}

int posterprov_e_provedor(const char *url) {
  size_t n = strlen(prefixo);
  if (!url || !n || cfg.prov == PP_DESLIGADO) return 0;
  return !strncmp(url, prefixo, n) && (url[n] == '/' || url[n] == 0);
}

// ---------------------------------------------------------------- portao
static pthread_cond_t vaga = PTHREAD_COND_INITIALIZER;
static int emUso;
void posterprov_portao_entrar(void) {
  pthread_mutex_lock(&trava);
  while (emUso >= PP_SIMULT) pthread_cond_wait(&vaga, &trava);
  emUso++;
  pthread_mutex_unlock(&trava);
}
void posterprov_portao_sair(void) {
  pthread_mutex_lock(&trava);
  if (emUso > 0) emUso--;
  pthread_cond_signal(&vaga);
  pthread_mutex_unlock(&trava);
}

void posterprov_stat(PosterProvStat *s) {
  if (!s) return;
  pthread_mutex_lock(&trava);
  s->falhas = nFalhou;
  s->seguidas = seguidas;
  s->disjuntor_aberto = disjuntorAte != 0 && agora() < disjuntorAte;
  pthread_mutex_unlock(&trava);
}
