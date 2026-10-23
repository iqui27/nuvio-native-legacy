#include "watchstate.h"
#include "addonurl.h"
#include "js.h"
#include "jsw.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Valor da chave `chave` de profundidade 1 em [ini,fim): o resource e o root
// repetem "types"/"events" aninhados, e js_texto pegaria o primeiro.
static const char *raiz(const char *ini, const char *fim, const char *chave) {
  size_t kl = strlen(chave);
  int prof = 0;
  const char *p;
  if (!ini) return NULL;
  if (!fim) fim = ini + strlen(ini);
  for (p = ini; p < fim; p++) {
    if (*p == '"') {
      const char *a = p + 1, *q = a;
      while (q < fim && *q != '"') { if (*q == '\\' && q + 1 < fim) q++; q++; }
      if (prof == 1 && (size_t)(q - a) == kl && !strncmp(a, chave, kl)) {
        const char *v = q + 1;
        while (v < fim && isspace((unsigned char)*v)) v++;
        if (v < fim && *v == ':') {
          for (v++; v < fim && isspace((unsigned char)*v); v++) ;
          return v < fim ? v : NULL;
        }
      }
      p = q;
    } else if (*p == '{' || *p == '[') prof++;
    else if (*p == '}' || *p == ']') prof--;
  }
  return NULL;
}

// Strings de um array JSON (v no '['), cada uma entregue a `cb`.
static void strings(const char *v, void (*cb)(const char *, void *), void *u) {
  const char *fim, *p;
  if (!v || *v != '[' || !(fim = js_fim(v))) return;
  for (p = v + 1; p < fim; p++) {
    if (*p == '"') {
      char s[WS_PREF_TAM];
      size_t k = 0;
      for (p++; p < fim && *p != '"'; p++) {
        if (*p == '\\' && p + 1 < fim) p++;
        if (k + 1 < sizeof s) s[k++] = *p;
      }
      s[k] = 0;
      if (k) cb(s, u);
    }
  }
}

static void cbTipo(const char *s, void *u) {
  WsDecl *d = u;
  size_t o = strlen(d->tipos), l = strlen(s), i;
  if (!o) { d->tipos[0] = '|'; o = 1; }
  if (o + l + 2 > sizeof d->tipos) return;
  for (i = 0; i < l; i++) d->tipos[o + i] = (char)tolower((unsigned char)s[i]);
  d->tipos[o + l] = '|'; d->tipos[o + l + 1] = 0;
}
static void cbPref(const char *s, void *u) {
  WsDecl *d = u;
  if (d->nPref < WS_PREF_MAX) snprintf(d->pref[d->nPref++], WS_PREF_TAM, "%s", s);
}
static void cbEvento(const char *s, void *u) {
  static const struct { const char *n; int bit; } ev[] = {
    {"start", WS_START}, {"pause", WS_PAUSE}, {"stop", WS_STOP},
    {"played", WS_PLAYED}, {"unplayed", WS_UNPLAYED}};
  size_t i;
  for (i = 0; i < sizeof ev / sizeof *ev; i++)
    if (!strcmp(s, ev[i].n)) *(int *)u |= ev[i].bit;   // outros nomes: ignorados
}

void ws_decl_ler(const char *corpo, WsDecl *d) {
  const char *res, *fimR, *e, *ws, *fimW, *push, *fimP, *ver, *evs;
  int achou = 0;
  memset(d, 0, sizeof *d);
  if (!corpo) return;
  res = raiz(corpo, NULL, "resources");
  if (!res || *res != '[' || !(fimR = js_fim(res))) return;
  for (e = res + 1; e < fimR && !achou; ) {
    while (e < fimR && (isspace((unsigned char)*e) || *e == ',')) e++;
    if (e >= fimR) break;
    if (*e == '{') {
      const char *fe = js_fim(e);
      char nome[24] = "";
      if (!fe) break;
      js_texto_raiz_em(e, fe, "name", nome, sizeof nome);
      if (!strcmp(nome, "watch_state")) {
        const char *tp = raiz(e, fe, "types"), *pf = raiz(e, fe, "idPrefixes");
        achou = 1;
        // O resource manda; na falta, valem os da raiz do manifesto.
        strings(tp ? tp : raiz(corpo, NULL, "types"), cbTipo, d);
        strings(pf ? pf : raiz(corpo, NULL, "idPrefixes"), cbPref, d);
      }
      e = fe + 1;
    } else if (*e == '"') {   // forma curta "watch_state": herda os da raiz
      const char *a = e + 1;
      for (e++; e < fimR && *e != '"'; e++) if (*e == '\\') e++;
      if (!strncmp(a, "watch_state\"", 12)) {
        achou = 1;
        strings(raiz(corpo, NULL, "types"), cbTipo, d);
        strings(raiz(corpo, NULL, "idPrefixes"), cbPref, d);
      }
      e++;
    } else e++;
  }
  if (!achou) return;
  // O root decide a versao e as metades. Sem watchState{push{}} o addon nao
  // pediu push; versao != 2 e o contrato v1/futuro, que nao misturamos.
  ws = raiz(corpo, NULL, "watchState");
  if (!ws || *ws != '{' || !(fimW = js_fim(ws))) return;
  ver = raiz(ws, fimW, "version");
  if (!ver || atoi(ver) != 2) return;
  push = raiz(ws, fimW, "push");
  if (!push || *push != '{' || !(fimP = js_fim(push))) return;
  evs = raiz(push, fimP, "events");
  if (evs && *evs == '[') strings(evs, cbEvento, &d->eventos);
  else d->eventos = WS_TODOS;     // omitido = todo evento de reproducao
  d->eventos &= WS_TODOS;
  { const char *vw = raiz(ws, fimW, "viewers");
    d->viewers = vw && !strncmp(vw, "true", 4); }
  d->push = d->eventos != 0;
}

int ws_decl_aceita(const WsDecl *d, int ev, const char *tipo,
                   const char *metaId, const char *videoId) {
  int k;
  if (!d || !d->push || !(d->eventos & ev)) return 0;
  // viewers:true pede ?viewer=<slug> por pessoa (W8, ainda sem vinculo). Sem
  // ele o evento iria para o DONO da configuracao: melhor nao enviar.
  if (d->viewers) return 0;
  if (d->tipos[0]) {
    char marca[40];
    if (!tipo || !tipo[0]) return 0;
    snprintf(marca, sizeof marca, "|%s|", tipo);
    for (k = 0; marca[k]; k++) marca[k] = (char)tolower((unsigned char)marca[k]);
    if (!strstr(d->tipos, marca)) return 0;
  }
  if (d->nPref > 0) {
    for (k = 0; k < d->nPref; k++) {
      size_t l = strlen(d->pref[k]);
      if (metaId && !strncmp(metaId, d->pref[k], l)) return 1;
      if (videoId && !strncmp(videoId, d->pref[k], l)) return 1;
    }
    return 0;
  }
  return 1;
}

// ---- Evento, corpo, id, URL ------------------------------------------------

static const char *nomeEv(int ev) {
  switch (ev) {
    case WS_START: return "start";   case WS_PAUSE: return "pause";
    case WS_STOP: return "stop";     case WS_PLAYED: return "played";
    case WS_UNPLAYED: return "unplayed";
    default: return NULL;
  }
}

// Texto que vai para id, URL e diario: sem controle (o diario separa por TAB).
static int limpo(const char *s) {
  for (; *s; s++) if ((unsigned char)*s < 0x20 || *s == 0x7f) return 0;
  return 1;
}

static int valido(const WsEvento *e) {
  int serie, scrobble;
  if (!e || !nomeEv(e->ev)) return 0;
  serie = !strcmp(e->tipo, "series");
  if (!serie && strcmp(e->tipo, "movie")) return 0;
  if (!e->videoId[0] || !limpo(e->videoId) || !limpo(e->metaId) ||
      !limpo(e->imdb) || !limpo(e->tmdb)) return 0;
  if (e->atMs < 1000) return 0;
  if (serie && (e->season < 0 || e->episode < 0)) return 0;   // identidade ambigua
  scrobble = e->ev == WS_START || e->ev == WS_PAUSE || e->ev == WS_STOP;
  if (scrobble && (e->posMs < 0 || e->iniMs < 1000)) return 0;
  return 1;
}

int ws_id(const WsEvento *e, char *out, size_t n) {
  int k;
  if (!valido(e)) return 0;
  if (e->ev == WS_PLAYED || e->ev == WS_UNPLAYED)
    k = snprintf(out, n, "e|%s|%s|%lld", e->videoId, nomeEv(e->ev), e->atMs);
  else
    k = snprintf(out, n, "e|%s|%s|%lld|%lld", e->videoId, nomeEv(e->ev), e->posMs, e->iniMs);
  return k > 0 && (size_t)k < n;
}

int ws_corpo(const WsEvento *e, char *out, size_t n) {
  char id[160];
  Jsw w;
  const char *t;
  size_t l;
  if (!ws_id(e, id, sizeof id)) return 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "id", id);
  jsw_cs(&w, "event", nomeEv(e->ev));
  jsw_cs(&w, "scope", !strcmp(e->tipo, "series") ? "episode" : "movie");
  jsw_ci(&w, "at", e->atMs / 1000);
  if (e->metaId[0]) jsw_cs(&w, "metaId", e->metaId);
  jsw_cs(&w, "videoId", e->videoId);
  if (e->posMs >= 0) jsw_ci(&w, "positionMs", e->posMs);
  if (e->durMs > 0) jsw_ci(&w, "durationMs", e->durMs);
  jsw_cb(&w, "played", e->played);
  if (!strcmp(e->tipo, "series")) { jsw_ci(&w, "season", e->season); jsw_ci(&w, "episode", e->episode); }
  if (e->imdb[0] || e->tmdb[0]) {
    jsw_chave(&w, "ids");
    jsw_obj_ini(&w);
    if (e->imdb[0]) jsw_cs(&w, "imdb", e->imdb);
    if (e->tmdb[0]) jsw_cs(&w, "tmdb", e->tmdb);
    jsw_obj_fim(&w);
  }
  jsw_obj_fim(&w);
  t = jsw_texto_final(&w);
  l = t ? strlen(t) : 0;
  if (!l || l >= n) { jsw_livre(&w); return 0; }
  memcpy(out, t, l + 1);
  jsw_livre(&w);
  return 1;
}

// Percent-encoding de um segmento de caminho; ':' fica (os ids de video o usam).
static int segmento(char *o, size_t n, size_t *k, const char *s) {
  static const char hex[] = "0123456789ABCDEF";
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || strchr("-._~:", c)) {
      if (*k + 1 >= n) return 0;
      o[(*k)++] = (char)c;
    } else {
      if (*k + 3 >= n) return 0;
      o[(*k)++] = '%'; o[(*k)++] = hex[c >> 4]; o[(*k)++] = hex[c & 15];
    }
  }
  return 1;
}

int ws_url(const char *base, const WsEvento *e, char *out, size_t n) {
  char caminho[512];
  size_t k = 0;
  int w;
  if (!valido(e) || !base || !limpo(base)) return 0;
  if (strncmp(base, "https://", 8) && strncmp(base, "http://", 7)) return 0;
  caminho[k++] = '/';
  memcpy(caminho + k, "watch_state/push/", 17); k += 17;
  if (!segmento(caminho, sizeof caminho, &k, e->tipo) || k + 1 >= sizeof caminho) return 0;
  caminho[k++] = '/';
  if (!segmento(caminho, sizeof caminho, &k, e->videoId) || k + 6 > sizeof caminho) return 0;
  memcpy(caminho + k, ".json", 6);
  // A query da base carrega a configuracao do addon: o caminho vai ANTES dela
  // (addonurl.h), senao o pedido sai depois da query e a config chega corrompida.
  w = nv_addon_url(out, n, base, "%s", caminho);
  return w > 0 && out && out[0];
}

// ---- Fila com retry --------------------------------------------------------

void ws_destino(const char *base, const char *usuario, int perfil, char out[20]) {
  uint64_t h = UINT64_C(14695981039346656037);
  const char *partes[] = {usuario ? usuario : "", base ? base : ""};
  int i;
  for (i = 0; i < 2; i++) {
    const unsigned char *p = (const unsigned char *)partes[i];
    do { h ^= *p; h *= UINT64_C(1099511628211); } while (*p++);
  }
  snprintf(out, 20, "%016llx-%d", (unsigned long long)h, perfil);
}

long long ws_espera_s(int falhas) {
  static const long long t[] = {30, 120, 600, 3600, 21600};
  if (falhas < 1) falhas = 1;
  return falhas > 5 ? 21600 : t[falhas - 1];
}

static void liberaItem(WsItem *i) {
  free(i->addon); free(i->caminho); free(i->id); free(i->corpo);
  memset(i, 0, sizeof *i);
}
static void tiraItem(WsFila *f, int k) {
  liberaItem(&f->it[k]);
  memmove(&f->it[k], &f->it[k + 1], (size_t)(f->n - k - 1) * sizeof f->it[0]);
  memset(&f->it[--f->n], 0, sizeof f->it[0]);
}

void ws_fila_limpar(WsFila *f) {
  while (f->n) tiraItem(f, f->n - 1);
}

static int mesmo(const WsItem *i, const char *addon, const char *id) {
  return !strcmp(i->addon, addon) && !strcmp(i->id, id);
}

// Anexa um item ja validado (copia as strings); cheia: o mais antigo sai.
static int anexa(WsFila *f, const char *addon, const char *caminho, const char *id,
                 const char *corpo, long long criadoS, long long proxS, int tent) {
  WsItem *i;
  if (f->n == WS_FILA_MAX) tiraItem(f, 0);
  i = &f->it[f->n];
  i->addon = strdup(addon); i->caminho = strdup(caminho); i->id = strdup(id); i->corpo = strdup(corpo);
  if (!i->addon || !i->caminho || !i->id || !i->corpo) { liberaItem(i); return 0; }
  i->criadoS = criadoS; i->proxS = proxS; i->tentativas = tent;
  f->n++;
  return 1;
}

int ws_fila_empurrar(WsFila *f, const char *addon, const char *caminho, const char *id,
                     const char *corpo, long long agoraS) {
  int k;
  if (!addon || !caminho || !id || !corpo || !limpo(addon) || !limpo(caminho) || !limpo(id) ||
      !limpo(corpo) || !addon[0] || strpbrk(addon, ":/?") ||
      caminho[0] != '/' || strchr(caminho, '?')) return 0;
  for (k = 0; k < f->n; k++) if (mesmo(&f->it[k], addon, id)) return 0;
  return anexa(f, addon, caminho, id, corpo, agoraS, agoraS, 0);
}

char *ws_fila_serializar(const WsFila *f) {
  static const char versao[] = "NVWATCHSTATE 2\n";
  size_t cap = sizeof versao, k;
  char *o, *p;
  int i;
  for (i = 0; i < f->n; i++) {
    const WsItem *t = &f->it[i];
    cap += strlen(t->addon) + strlen(t->caminho) + strlen(t->id) + strlen(t->corpo) + 80;
  }
  o = p = malloc(cap);
  if (!o) return NULL;
  memcpy(p, versao, sizeof versao - 1); p += sizeof versao - 1;
  for (i = 0; i < f->n; i++) {
    const WsItem *t = &f->it[i];
    k = (size_t)snprintf(p, cap - (size_t)(p - o), "%lld\t%lld\t%d\t%s\t%s\t%s\t%s\n",
                         t->criadoS, t->proxS, t->tentativas, t->addon, t->caminho, t->id, t->corpo);
    p += k;
  }
  *p = 0;
  return o;
}

void ws_fila_carregar(WsFila *f, const char *texto) {
  const char *p;
  if (!texto || strncmp(texto, "NVWATCHSTATE 2\n", 15)) return;
  p = texto + 15;
  while (*p) {
    const char *fim = strchr(p, '\n');
    size_t l = fim ? (size_t)(fim - p) : strlen(p);
    char *ln = malloc(l + 1), *campo[6], *q;
    int nc = 0;
    if (!ln) return;
    memcpy(ln, p, l); ln[l] = 0;
    for (q = ln; nc < 6; ) {
      campo[nc++] = q;
      q = strchr(q, '\t');
      if (!q) break;
      *q++ = 0;
    }
    if (nc == 6 && q && campo[3][0] && campo[4][0] == '/' && campo[5][0] && q[0]) {
      char *e1, *e2, *e3;
      long long c = strtoll(campo[0], &e1, 10), x = strtoll(campo[1], &e2, 10);
      long t = strtol(campo[2], &e3, 10);
      if (!*e1 && !*e2 && !*e3 && c > 0 && t >= 0 && t < 1000 &&
          ws_fila_empurrar(f, campo[3], campo[4], campo[5], q, c)) {
        f->it[f->n - 1].proxS = x;
        f->it[f->n - 1].tentativas = (int)t;
      }
    }
    free(ln);
    if (!fim) break;
    p = fim + 1;
  }
}

long long ws_fila_proxima(const WsFila *f) {
  long long m = -1;
  int i, j;
  for (i = 0; i < f->n; i++) {
    for (j = 0; j < i && strcmp(f->it[j].addon, f->it[i].addon); j++) ;
    if (j == i && (m < 0 || f->it[i].proxS < m)) m = f->it[i].proxS;   // so cabecas
  }
  return m;
}

// Primeiro item vencido que e a frente do seu addon; -1 se nao ha.
static int devido(const WsFila *f, long long agora) {
  int i, j;
  for (i = 0; i < f->n; i++) {
    for (j = 0; j < i; j++) if (!strcmp(f->it[j].addon, f->it[i].addon)) break;
    if (j < i) continue;                      // ha um item mais velho do mesmo addon
    if (f->it[i].proxS <= agora) return i;
  }
  return -1;
}

int ws_fila_passo(WsFila *f, long long agora, WsEnvio envia, void *u) {
  int feitas = 0, i, k;
  for (i = f->n - 1; i >= 0; i--)
    if (agora - f->it[i].criadoS >= WS_VIDA_S) tiraItem(f, i);
  while ((i = devido(f, agora)) >= 0) {
    char *addon = strdup(f->it[i].addon), *id = strdup(f->it[i].id), *corpo = strdup(f->it[i].corpo);
    int ra = 0, st;
    if (!addon || !id || !corpo) { free(addon); free(id); free(corpo); break; }
    st = envia(addon, f->it[i].caminho, corpo, &ra, u);
    if (st != WS_AUSENTE) feitas++;   // WS_AUSENTE nem chegou a ser um pedido
    // O transporte pode ter soltado a trava: o item pode ter saido (fila cheia).
    for (k = 0; k < f->n && !mesmo(&f->it[k], addon, id); k++) ;
    if (k < f->n) {
      WsItem *t = &f->it[k];
      if (st == WS_AUSENTE) t->proxS = agora + WS_AUSENTE_S;
      else if (st == 200 || st == 204) tiraItem(f, k);
      else if (st == 401 || st == 403) t->proxS = agora + WS_SEGURA_S;
      else if (st == 0 || st == 429 || st >= 500) {
        long long w = ws_espera_s(++t->tentativas);
        if (ra > w) w = ra > WS_VIDA_S ? WS_VIDA_S : ra;
        t->proxS = agora + w;
      } else tiraItem(f, k);
    }
    free(addon); free(id); free(corpo);
  }
  return feitas;
}

int ws_fila_podar(WsFila *f, int (*ainda)(const char *addon, void *u), void *u) {
  int i, saiu = 0;
  for (i = f->n - 1; i >= 0; i--)
    if (!ainda || !ainda(f->it[i].addon, u)) { tiraItem(f, i); saiu++; }
  return saiu;
}
