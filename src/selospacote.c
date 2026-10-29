#include "selospacote.h"
#include "regexjs.h"
#include "dados.h"
#include "perfis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>

// Ver selospacote.h. Espelha streamBadgeRules.js do app web (normalizacao,
// limite de 3, dedupe por sourceUrl, candidatos de casamento).

// ---- JSON minimo (so o necessario: a arvore inteira em memoria) -------------
enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ };
typedef struct J {
  int t;
  char *s;            // J_STR
  char *k;            // a chave, quando e filho de objeto
  double num;
  int b;
  struct J *filho, *prox;
} J;

typedef struct { const char *p, *fim; int prof; } JP;

static void jLiberar(J *j) {
  while (j) { J *p = j->prox; jLiberar(j->filho); free(j->s); free(j->k); free(j); j = p; }
}
static void jEsp(JP *p) { while (p->p < p->fim && (unsigned char)*p->p <= ' ') p->p++; }
static int hx(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static char *jCadeia(JP *p) {   // p->p esta na aspa de abertura
  size_t cap = 32, n = 0;
  char *o = malloc(cap);
  if (!o) return NULL;
  p->p++;
  while (p->p < p->fim && *p->p != '"') {
    unsigned c = (unsigned char)*p->p++;
    if (n + 8 >= cap) { char *r = realloc(o, cap *= 2); if (!r) { free(o); return NULL; } o = r; }
    if (c == '\\' && p->p < p->fim) {
      c = (unsigned char)*p->p++;
      switch (c) {
        case 'n': c = '\n'; break; case 't': c = '\t'; break; case 'r': c = '\r'; break;
        case 'b': c = 8; break; case 'f': c = 12; break;
        case 'u': {
          unsigned u = 0; int i;
          for (i = 0; i < 4 && p->p + i < p->fim && hx(p->p[i]) >= 0; i++) u = u * 16 + (unsigned)hx(p->p[i]);
          p->p += i;
          if (u >= 0xD800 && u < 0xDC00 && p->fim - p->p >= 6 && p->p[0] == '\\' && p->p[1] == 'u') {
            unsigned lo = 0; int k;
            for (k = 0; k < 4 && hx(p->p[2 + k]) >= 0; k++) lo = lo * 16 + (unsigned)hx(p->p[2 + k]);
            if (k == 4 && lo >= 0xDC00 && lo < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); p->p += 6; }
          }
          if (u < 0x80) o[n++] = (char)u;
          else if (u < 0x800) { o[n++] = (char)(0xC0 | u >> 6); o[n++] = (char)(0x80 | (u & 63)); }
          else if (u < 0x10000) { o[n++] = (char)(0xE0 | u >> 12); o[n++] = (char)(0x80 | ((u >> 6) & 63)); o[n++] = (char)(0x80 | (u & 63)); }
          else { o[n++] = (char)(0xF0 | u >> 18); o[n++] = (char)(0x80 | ((u >> 12) & 63)); o[n++] = (char)(0x80 | ((u >> 6) & 63)); o[n++] = (char)(0x80 | (u & 63)); }
          continue; }
        default: break;   // \" \\ \/ e o resto: o proprio caractere
      }
    }
    o[n++] = (char)c;
  }
  if (p->p >= p->fim) { free(o); return NULL; }
  p->p++;
  o[n] = 0;
  return o;
}
static J *jValor(JP *p);
static J *jNovo(int t) { J *j = calloc(1, sizeof *j); if (j) j->t = t; return j; }
static J *jValor(JP *p) {
  J *j;
  jEsp(p);
  if (p->p >= p->fim || p->prof > 40) return NULL;
  switch (*p->p) {
    case '"': {
      char *s = jCadeia(p);
      if (!s) return NULL;
      j = jNovo(J_STR);
      if (!j) { free(s); return NULL; }
      j->s = s; return j; }
    case '{': case '[': {
      int obj = *p->p == '{';
      J *ult = NULL;
      j = jNovo(obj ? J_OBJ : J_ARR);
      if (!j) return NULL;
      p->p++; p->prof++;
      jEsp(p);
      if (p->p < p->fim && *p->p == (obj ? '}' : ']')) { p->p++; p->prof--; return j; }
      for (;;) {
        char *chave = NULL; J *f;
        jEsp(p);
        if (obj) {
          if (p->p >= p->fim || *p->p != '"') goto erro;
          chave = jCadeia(p);
          if (!chave) goto erro;
          jEsp(p);
          if (p->p >= p->fim || *p->p != ':') { free(chave); goto erro; }
          p->p++;
        }
        f = jValor(p);
        if (!f) { free(chave); goto erro; }
        if (obj) f->k = chave;
        if (ult) ult->prox = f; else j->filho = f;
        ult = f;
        jEsp(p);
        if (p->p < p->fim && *p->p == ',') { p->p++; continue; }
        if (p->p < p->fim && *p->p == (obj ? '}' : ']')) { p->p++; p->prof--; return j; }
        goto erro;
      }
    erro:
      jLiberar(j); return NULL; }
    case 't': if (p->fim - p->p >= 4 && !strncmp(p->p, "true", 4)) { p->p += 4; j = jNovo(J_BOOL); if (j) j->b = 1; return j; } return NULL;
    case 'f': if (p->fim - p->p >= 5 && !strncmp(p->p, "false", 5)) { p->p += 5; return jNovo(J_BOOL); } return NULL;
    case 'n': if (p->fim - p->p >= 4 && !strncmp(p->p, "null", 4)) { p->p += 4; return jNovo(J_NULL); } return NULL;
    default: {
      char *e; double d = strtod(p->p, &e);
      if (e == p->p) return NULL;
      p->p = e;
      j = jNovo(J_NUM); if (j) j->num = d; return j; }
  }
}
static J *jParse(const char *txt, size_t n) {
  JP p; J *j;
  if (!txt) return NULL;
  p.p = txt; p.fim = txt + n; p.prof = 0;
  j = jValor(&p);
  if (!j) return NULL;
  jEsp(&p);
  if (p.p != p.fim) { jLiberar(j); return NULL; }
  return j;
}
static J *jGet(const J *o, const char *chave) {
  J *f;
  if (!o || o->t != J_OBJ) return NULL;
  for (f = o->filho; f; f = f->prox) if (f->k && !strcmp(f->k, chave)) return f;
  return NULL;
}
// Procura `chave` em qualquer nivel (profundidade primeiro).
static J *jAchar(const J *o, const char *chave, int prof) {
  J *f, *r;
  if (!o || prof > 12 || (o->t != J_OBJ && o->t != J_ARR)) return NULL;
  for (f = o->filho; f; f = f->prox) if (o->t == J_OBJ && f->k && !strcmp(f->k, chave)) return f;
  for (f = o->filho; f; f = f->prox) if ((r = jAchar(f, chave, prof + 1))) return r;
  return NULL;
}

// ---- normalizacao (streamBadgeRules.js) -------------------------------------
typedef struct FiltroRawS {
  char *id, *grupo, *nome, *padrao, *imagem, *tag, *estilo, *texto, *borda;
  int ativo;
} FiltroRaw;
typedef struct { char *id, *nome, *cor; } GrupoRaw;
typedef struct {
  char *url;
  int ativo;
  FiltroRaw *f; int nf;
  GrupoRaw *g; int ng;
} PacoteRaw;

static char *norm(const char *s) {   // normalizeText: espacos colapsados e aparados
  size_t n = s ? strlen(s) : 0, k = 0, i;
  char *o = malloc(n + 1);
  int esp = 0;
  if (!o) return NULL;
  for (i = 0; i < n; i++) {
    if (isspace((unsigned char)s[i])) { esp = 1; continue; }
    if (esp && k) o[k++] = ' ';
    esp = 0;
    o[k++] = s[i];
  }
  o[k] = 0;
  return o;
}
static char *normStr(const J *o, const char *chave) {
  J *f = jGet(o, chave);
  char buf[64];
  if (!f || f->t == J_NULL) return norm("");
  if (f->t == J_STR) return norm(f->s);
  if (f->t == J_BOOL) return norm(f->b ? "true" : "false");
  if (f->t == J_NUM) { snprintf(buf, sizeof buf, "%g", f->num); return norm(buf); }
  return norm("");
}
// normalizeColor: "#RRGGBB", "transparent", "rgba(...)" ou "" (invalida).
static char *normCor(const J *o, const char *chave) {
  char *t = normStr(o, chave), *r;
  const char *h; size_t n, i;
  if (!t) return NULL;
  if (!strcasecmp(t, "transparent")) { strcpy(t, "transparent"); return t; }
  if ((!strncasecmp(t, "rgba(", 5) || !strncasecmp(t, "rgb(", 4)) && t[strlen(t) - 1] == ')') {
    const char *q = strchr(t, '(') + 1; int ok = 1;
    for (; *q && *q != ')'; q++) if (!strchr("0123456789 ,%.", *q)) ok = 0;
    if (ok) return t;
    t[0] = 0; return t;
  }
  h = t[0] == '#' ? t + 1 : t;
  n = strlen(h);
  if (n != 6 && n != 8) { t[0] = 0; return t; }
  for (i = 0; i < n; i++) if (hx(h[i]) < 0) { t[0] = 0; return t; }
  r = malloc(48);
  if (!r) { free(t); return NULL; }
  if (n == 6) { snprintf(r, 48, "#%s", h); for (i = 0; r[i]; i++) r[i] = (char)toupper((unsigned char)r[i]); }
  else {
    int a = hx(h[0]) * 16 + hx(h[1]), rr = hx(h[2]) * 16 + hx(h[3]), g = hx(h[4]) * 16 + hx(h[5]), b = hx(h[6]) * 16 + hx(h[7]);
    if (a >= 255) { snprintf(r, 48, "#%.6s", h + 2); for (i = 0; r[i]; i++) r[i] = (char)toupper((unsigned char)r[i]); }
    else if (a <= 0) strcpy(r, "transparent");
    else snprintf(r, 48, "rgba(%d, %d, %d, %.3f)", rr, g, b, a / 255.0);
  }
  free(t);
  return r;
}

static void filtroLiberar(FiltroRaw *f) {
  free(f->id); free(f->grupo); free(f->nome); free(f->padrao); free(f->imagem);
  free(f->tag); free(f->estilo); free(f->texto); free(f->borda);
}
static void pacoteLiberar(PacoteRaw *p) {
  int i;
  for (i = 0; i < p->nf; i++) filtroLiberar(&p->f[i]);
  for (i = 0; i < p->ng; i++) { free(p->g[i].id); free(p->g[i].nome); free(p->g[i].cor); }
  free(p->f); free(p->g); free(p->url);
  memset(p, 0, sizeof *p);
}

static int ehObj(const J *j) { return j && j->t == J_OBJ; }

// normalizeStreamBadgeImport. 1 = pacote valido em *out; 0 = descartado.
static int normImport(const J *src, const char *urlPadrao, PacoteRaw *out) {
  J *fs, *gs, *e;
  int nf = 0, ng = 0;
  char *url;
  memset(out, 0, sizeof *out);
  if (!ehObj(src)) return 0;
  url = normStr(src, "sourceUrl");
  if (url && !url[0] && urlPadrao) { free(url); url = norm(urlPadrao); }
  if (!url || !url[0]) { free(url); return 0; }
  out->url = url;
  { J *a = jGet(src, "isActive"), *b = jGet(src, "active");
    out->ativo = !((a && a->t == J_BOOL && !a->b) || (b && b->t == J_BOOL && !b->b)); }
  fs = jGet(src, "filters"); gs = jGet(src, "groups");
  // toBadgeArray: um valor so vira lista de um
  for (e = fs && fs->t == J_ARR ? fs->filho : fs; e; e = (fs && fs->t == J_ARR) ? e->prox : NULL) nf++;
  for (e = gs && gs->t == J_ARR ? gs->filho : gs; e; e = (gs && gs->t == J_ARR) ? e->prox : NULL) ng++;
  if (nf > 2000) nf = 2000;
  out->f = calloc((size_t)nf + 1, sizeof *out->f);
  out->g = calloc((size_t)ng + 1, sizeof *out->g);
  if (!out->f || !out->g) { pacoteLiberar(out); return 0; }
  for (e = fs && fs->t == J_ARR ? fs->filho : fs; e && out->nf < nf; e = (fs && fs->t == J_ARR) ? e->prox : NULL) {
    FiltroRaw *f = &out->f[out->nf];
    J *en;
    if (!ehObj(e)) continue;
    f->nome = normStr(e, "name"); f->padrao = normStr(e, "pattern");
    if (!f->nome || !f->padrao || !f->nome[0] || !f->padrao[0]) { filtroLiberar(f); memset(f, 0, sizeof *f); continue; }
    f->id = normStr(e, "id"); f->grupo = normStr(e, "groupId"); f->imagem = normStr(e, "imageURL");
    f->estilo = normStr(e, "tagStyle");
    f->tag = normCor(e, "tagColor"); f->texto = normCor(e, "textColor"); f->borda = normCor(e, "borderColor");
    en = jGet(e, "isEnabled");
    f->ativo = !(en && en->t == J_BOOL && !en->b);
    out->nf++;
  }
  for (e = gs && gs->t == J_ARR ? gs->filho : gs; e && out->ng < ng; e = (gs && gs->t == J_ARR) ? e->prox : NULL) {
    GrupoRaw *g = &out->g[out->ng];
    if (!ehObj(e)) continue;
    g->id = normStr(e, "id"); g->nome = normStr(e, "name"); g->cor = normCor(e, "color");
    out->ng++;
  }
  if (!out->nf) { pacoteLiberar(out); return 0; }
  return 1;
}

typedef struct { PacoteRaw p[SELOS_MAX_PACOTES]; int n; } Lote;
static void loteLiberar(Lote *l) { int i; for (i = 0; i < l->n; i++) pacoteLiberar(&l->p[i]); l->n = 0; }

// normalizeStreamBadgeRules: dedupe por sourceUrl (sem caixa; o ultimo
// substitui), limite de 3, um so ativo.
static void loteAdd(Lote *l, PacoteRaw *novo) {
  int i;
  for (i = 0; i < l->n; i++)
    if (!strcasecmp(l->p[i].url, novo->url)) { pacoteLiberar(&l->p[i]); l->p[i] = *novo; memset(novo, 0, sizeof *novo); return; }
  if (l->n < SELOS_MAX_PACOTES) { l->p[l->n++] = *novo; memset(novo, 0, sizeof *novo); return; }
  pacoteLiberar(novo);
}
static void loteDeImports(Lote *l, const J *arr) {
  J *e;
  for (e = arr->filho; e; e = e->prox) {
    PacoteRaw p;
    if (normImport(e, NULL, &p)) loteAdd(l, &p);
  }
}
// Forma 1: {imports:[...]}; forma 2: {streamBadgeRules|settings.streamBadgeRules};
// forma 3: {filters, groups, sourceUrl?, isActive?}. `v` pode ser string com
// JSON dentro (parseBadgePayload).
static int loteDoValor(Lote *l, const J *v, const char *url, int prof) {
  J *tmp = NULL, *imp, *aninhado, *set;
  int ok = 0;
  PacoteRaw p;
  if (prof > 3 || !v) return 0;
  if (v->t == J_STR) {
    char *t = norm(v->s);
    if (!t || !t[0]) { free(t); return 0; }
    tmp = jParse(v->s, strlen(v->s));
    free(t);
    if (!tmp) return 0;
    v = tmp;
  }
  if (!ehObj(v)) { jLiberar(tmp); return 0; }
  imp = jGet(v, "imports");
  if (imp && imp->t == J_ARR) {
    loteDeImports(l, imp);
    ok = l->n > 0;
  }
  if (!ok) {
    aninhado = jGet(v, "streamBadgeRules");
    if (!aninhado && (set = jGet(v, "settings"))) aninhado = jGet(set, "streamBadgeRules");
    if (aninhado) ok = loteDoValor(l, aninhado, url, prof + 1) && l->n > 0;
  }
  if (!ok && !(imp && imp->t == J_ARR)) {
    // forma 3: o proprio objeto tem filters/groups/isActive (sourceUrl opcional)
    if (normImport(v, url && *url ? url : "Pasted badge rules", &p)) { loteAdd(l, &p); ok = 1; }
  }
  jLiberar(tmp);
  return ok;
}


// ---- compilacao e estado -----------------------------------------------------
typedef struct {
  RegexJs *re;
  SeloFiltro pub;
  char *chave;      // imageURL (ou nome) em minuscula: o dedupe do web
  const struct FiltroRawS *raw;   // o cru de onde veio (so os embutidos usam)
  int rank;         // embutidos: posicao do grupo na fileira
  int subst;        // embutidos, padrao: o colorido cobre este selo
} FiltroC;
typedef struct {
  const PacoteRaw *raw;
  int daTv;
  FiltroC *c; int nc;
  char nome[96];
} Pacote;

static Lote conta, tv;               // donos do cru; `pac` so aponta para eles
static Pacote pac[SELOS_MAX_PACOTES];
static int nPac;
static int ativo = -1;
static int escolhaExplicita;         // 0 = ainda nao escolheu: vale o ativo da conta
static char escolhaUrl[512];         // "" = "Do Nuvio" escolhido de proposito
static unsigned versao = 1;

// O CASAMENTO RODA NUM FIO DE FUNDO (streams.c): sao centenas de regex por
// fonte, e na thread principal a pagina do titulo travava ~2 s a cada addon
// que respondia (TCL, 05/10/2026: upd=1667 ms com 20 fontes). Esta trava
// separa esse fio de quem troca os pacotes (Ajustes, sync, troca de perfil).
// Recursiva: as funcoes publicas se chamam entre si.
static pthread_mutex_t selosTrava;
static pthread_once_t selosTravaUma = PTHREAD_ONCE_INIT;
static void selosTravaCriar(void) {
  pthread_mutexattr_t a;
  pthread_mutexattr_init(&a);
  pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&selosTrava, &a);
  pthread_mutexattr_destroy(&a);
}
static void travar(void) { pthread_once(&selosTravaUma, selosTravaCriar); pthread_mutex_lock(&selosTrava); }
static void soltar(void) { pthread_mutex_unlock(&selosTrava); }
static int perfilLido = -1;

static int parseCor(const char *s, int *tem, float rgba[4]) {
  *tem = 0;
  if (!s || !s[0]) return 0;
  if (!strcasecmp(s, "transparent")) { rgba[0] = rgba[1] = rgba[2] = 0; rgba[3] = 0; *tem = 1; return 1; }
  if (s[0] == '#' && strlen(s) == 7) {
    unsigned v = (unsigned)strtoul(s + 1, NULL, 16);
    rgba[0] = (float)((v >> 16) & 255) / 255.0f; rgba[1] = (float)((v >> 8) & 255) / 255.0f;
    rgba[2] = (float)(v & 255) / 255.0f; rgba[3] = 1.0f; *tem = 1; return 1;
  }
  if (!strncasecmp(s, "rgb", 3)) {
    double v[4] = { 0, 0, 0, 1 }; const char *q = strchr(s, '('); int k = 0;
    if (!q) return 0;
    q++;
    while (*q && *q != ')' && k < 4) {
      char *e; v[k++] = strtod(q, &e);
      if (e == q) break;
      q = e; while (*q == ',' || *q == ' ' || *q == '%') q++;
    }
    if (k < 3) return 0;
    rgba[0] = (float)(v[0] / 255.0); rgba[1] = (float)(v[1] / 255.0); rgba[2] = (float)(v[2] / 255.0);
    rgba[3] = (float)v[3]; *tem = 1; return 1;
  }
  return 0;
}

static void nomeDe(Pacote *p) {
  int i;
  p->nome[0] = 0;
  for (i = 0; i < p->raw->ng; i++)
    if (p->raw->g[i].nome && p->raw->g[i].nome[0]) { snprintf(p->nome, sizeof p->nome, "%s", p->raw->g[i].nome); break; }
  if (!p->nome[0]) {
    const char *u = p->raw->url, *h = strstr(u, "://");
    size_t n;
    h = h ? h + 3 : u;
    n = strcspn(h, "/?#");
    snprintf(p->nome, sizeof p->nome, "%.*s", (int)(n < 90 ? n : 90), h);
  }
}

static void pacoteDesmonta(Pacote *p) {
  int i;
  for (i = 0; i < p->nc; i++) { regexjs_liberar(p->c[i].re); free(p->c[i].chave); }
  free(p->c);
  memset(p, 0, sizeof *p);
}

static void compilaPacote(Pacote *p) {
  int i, j;
  p->c = calloc((size_t)p->raw->nf + 1, sizeof *p->c);
  p->nc = 0;
  if (!p->c) return;
  for (i = 0; i < p->raw->nf; i++) {
    const FiltroRaw *f = &p->raw->f[i];
    FiltroC *c = &p->c[p->nc];
    char erro[96];
    if (!f->ativo || strlen(f->padrao) > 2000) continue;
    c->re = regexjs_compilar_web(f->padrao, erro, sizeof erro);
    if (!c->re) {
      // UMA VEZ por padrao e por execucao: a lista e recompilada a cada troca
      // de perfil/pacote, e repetir a linha enchia o registro.
      static unsigned vistos[32]; static int nv;
      unsigned h = 5381; int v, ja = 0;
      for (const char *q = f->padrao; *q; q++) h = h * 33 + (unsigned char)*q;
      for (v = 0; v < nv; v++) if (vistos[v] == h) ja = 1;
      if (!ja) { if (nv < 32) vistos[nv++] = h;
      printf("[selos] padrao ignorado (\"%s\", %s): %s\n", f->nome, p->nome, erro); }
      continue;
    }
    c->raw = f;
    c->pub.nome = f->nome; c->pub.imagem = f->imagem;
    parseCor(f->tag, &c->pub.temTag, c->pub.tag);
    parseCor(f->texto, &c->pub.temTexto, c->pub.texto);
    parseCor(f->borda, &c->pub.temBorda, c->pub.borda);
    c->chave = strdup(f->imagem[0] ? f->imagem : f->nome);
    if (!c->chave) { regexjs_liberar(c->re); continue; }
    for (j = 0; c->chave[j]; j++) c->chave[j] = (char)tolower((unsigned char)c->chave[j]);
    p->nc++;
  }
  fflush(stdout);
}

// ---- disco -------------------------------------------------------------------
static void jsEsc(FILE *f, const char *s) {
  fputc('"', f);
  for (; s && *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
    else if (c < 32) fprintf(f, "\\u%04x", c);
    else fputc(c, f);
  }
  fputc('"', f);
}
static void serPacote(FILE *f, const PacoteRaw *p) {
  int i;
  fputs("{\"sourceUrl\":", f); jsEsc(f, p->url);
  fprintf(f, ",\"isActive\":%s,\"groups\":[", p->ativo ? "true" : "false");
  for (i = 0; i < p->ng; i++) {
    if (i) fputc(',', f);
    fputs("{\"id\":", f); jsEsc(f, p->g[i].id); fputs(",\"name\":", f); jsEsc(f, p->g[i].nome);
    fputs(",\"color\":", f); jsEsc(f, p->g[i].cor); fputc('}', f);
  }
  fputs("],\"filters\":[", f);
  for (i = 0; i < p->nf; i++) {
    const FiltroRaw *x = &p->f[i];
    if (i) fputc(',', f);
    fputs("{\"id\":", f); jsEsc(f, x->id); fputs(",\"groupId\":", f); jsEsc(f, x->grupo);
    fputs(",\"name\":", f); jsEsc(f, x->nome); fputs(",\"pattern\":", f); jsEsc(f, x->padrao);
    fputs(",\"imageURL\":", f); jsEsc(f, x->imagem);
    fprintf(f, ",\"isEnabled\":%s,\"tagColor\":", x->ativo ? "true" : "false"); jsEsc(f, x->tag);
    fputs(",\"tagStyle\":", f); jsEsc(f, x->estilo); fputs(",\"textColor\":", f); jsEsc(f, x->texto);
    fputs(",\"borderColor\":", f); jsEsc(f, x->borda); fputc('}', f);
  }
  fputs("]}", f);
}
// Serializa para um buffer novo (open_memstream nao existe em todo alvo: usa tmpfile).
static char *serializa(const Lote *l, int comoImports) {
  FILE *f = tmpfile();
  long n; char *b;
  int i;
  if (!f) return NULL;
  if (comoImports) fputs("{\"imports\":[", f);
  for (i = 0; i < l->n; i++) { if (i) fputc(',', f); serPacote(f, &l->p[i]); }
  if (comoImports) fputs("]}", f);
  n = ftell(f);
  b = malloc((size_t)n + 1);
  if (b) { rewind(f); if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; } else b[n] = 0; }
  fclose(f);
  return b;
}
static void nomeArq(char *d, size_t n, const char *fmt, int a, int b) { snprintf(d, n, fmt, a, b); }

static void gravarTv(int perfil) {
  char nome[48]; int k;
  for (k = 0; k < SELOS_MAX_PACOTES; k++) {
    nomeArq(nome, sizeof nome, "selos-p%d-%d.json", perfil, k);
    if (k < tv.n) {
      Lote um; char *s;
      memset(&um, 0, sizeof um);
      um.n = 1; um.p[0] = tv.p[k];
      s = serializa(&um, 0);
      if (s) { dados_gravar(nome, s); free(s); }
    } else dados_apagar(nome);
  }
}
static void gravarEscolha(int perfil) {
  char nome[48];
  snprintf(nome, sizeof nome, "selos-p%d.txt", perfil);
  if (!escolhaExplicita) { dados_apagar(nome); return; }
  dados_gravar(nome, escolhaUrl[0] ? escolhaUrl : "-");
}
static void gravarConta(int perfil) {
  char nome[48]; char *s;
  nomeArq(nome, sizeof nome, "selos-acct-p%d.json", perfil, 0);
  if (!conta.n) { dados_apagar(nome); return; }
  s = serializa(&conta, 1);
  if (s) { dados_gravar(nome, s); free(s); }
}

static void reconstruir(void) {
  int i, j;
  for (i = 0; i < nPac; i++) pacoteDesmonta(&pac[i]);
  nPac = 0;
  for (i = 0; i < conta.n && nPac < SELOS_MAX_PACOTES; i++) {
    pac[nPac].raw = &conta.p[i]; pac[nPac].daTv = 0; nPac++;
  }
  for (i = 0; i < tv.n && nPac < SELOS_MAX_PACOTES; i++) {
    int dup = 0;
    for (j = 0; j < nPac; j++) if (!strcasecmp(pac[j].raw->url, tv.p[i].url)) dup = 1;
    if (dup) continue;
    pac[nPac].raw = &tv.p[i]; pac[nPac].daTv = 1; nPac++;
  }
  for (i = 0; i < nPac; i++) { nomeDe(&pac[i]); compilaPacote(&pac[i]); }
  ativo = -1;
  if (escolhaExplicita) {
    for (i = 0; i < nPac; i++) if (escolhaUrl[0] && !strcasecmp(pac[i].raw->url, escolhaUrl)) ativo = i;
  } else {
    for (i = 0; i < nPac; i++) if (!pac[i].daTv && pac[i].raw->ativo && pac[i].nc) { ativo = i; break; }
  }
  if (ativo >= 0 && !pac[ativo].nc) ativo = -1;
  versao++;
}

static void selospacote_iniciar_i(void) {
  int perfil = perfis_ativo(), k;
  char nome[48], *t;
  if (perfil == perfilLido) return;
  perfilLido = perfil;
  loteLiberar(&conta); loteLiberar(&tv);
  escolhaExplicita = 0; escolhaUrl[0] = 0;
  nomeArq(nome, sizeof nome, "selos-acct-p%d.json", perfil, 0);
  if ((t = dados_ler(nome))) {
    J *j = jParse(t, strlen(t));
    if (j) { loteDoValor(&conta, j, "", 0); jLiberar(j); }
    free(t);
  }
  for (k = 0; k < SELOS_MAX_PACOTES; k++) {
    nomeArq(nome, sizeof nome, "selos-p%d-%d.json", perfil, k);
    if ((t = dados_ler(nome))) {
      J *j = jParse(t, strlen(t));
      if (j) { loteDoValor(&tv, j, "", 0); jLiberar(j); }
      free(t);
    }
  }
  snprintf(nome, sizeof nome, "selos-p%d.txt", perfil);
  if ((t = dados_ler(nome))) {
    size_t n = strlen(t);
    while (n && (t[n - 1] == '\n' || t[n - 1] == '\r' || t[n - 1] == ' ')) t[--n] = 0;
    escolhaExplicita = n > 0;
    snprintf(escolhaUrl, sizeof escolhaUrl, "%s", strcmp(t, "-") ? t : "");
    free(t);
  }
  reconstruir();
  printf("[selos] perfil %d: %d pacote(s)%s%s\n", perfil, nPac, ativo >= 0 ? ", ativo: " : ", ativo: Do Nuvio",
         ativo >= 0 ? pac[ativo].nome : "");
  fflush(stdout);
}
void selospacote_iniciar(void) { if (perfis_ativo() == perfilLido) return; travar(); selospacote_iniciar_i(); soltar(); }
void selospacote_recarregar(void) { travar(); perfilLido = -1; selospacote_iniciar_i(); soltar(); }

static void selospacote_conta_do_blob_i(const char *blob) {
  J *raiz, *f, *v;
  Lote novo;
  char *antes, *depois;
  if (!blob || !*blob) return;
  selospacote_iniciar();
  raiz = jParse(blob, strlen(blob));
  if (!raiz) return;
  f = jAchar(raiz, "stream_badge_rules", 0);
  if (!f) { jLiberar(raiz); return; }
  v = f;
  if (f->t == J_OBJ && jGet(f, "value") && !jGet(f, "imports")) v = jGet(f, "value");
  memset(&novo, 0, sizeof novo);
  loteDoValor(&novo, v, "", 0);
  jLiberar(raiz);
  antes = serializa(&conta, 1); depois = serializa(&novo, 1);
  if (antes && depois && !strcmp(antes, depois)) { free(antes); free(depois); loteLiberar(&novo); return; }
  free(antes); free(depois);
  loteLiberar(&conta);
  conta = novo;
  gravarConta(perfilLido);
  reconstruir();
  printf("[selos] conta: %d pacote(s)\n", conta.n);
  fflush(stdout);
}
void selospacote_conta_do_blob(const char *blob) { travar(); selospacote_conta_do_blob_i(blob); soltar(); }

int selospacote_n(void) { selospacote_iniciar(); return nPac; }
const char *selospacote_nome(int i) { selospacote_iniciar(); return i >= 0 && i < nPac ? pac[i].nome : ""; }
const char *selospacote_url(int i) { selospacote_iniciar(); return i >= 0 && i < nPac ? pac[i].raw->url : ""; }
int selospacote_da_tv(int i) { selospacote_iniciar(); return i >= 0 && i < nPac && pac[i].daTv; }
int selospacote_n_filtros(int i) { selospacote_iniciar(); return i >= 0 && i < nPac ? pac[i].nc : 0; }
int selospacote_ativo(void) { selospacote_iniciar(); return ativo; }
unsigned selospacote_versao(void) { return versao; }

static void selospacote_escolher_i(int i) {
  selospacote_iniciar();
  if (i < 0 || i >= nPac) i = -1;
  escolhaExplicita = 1;
  snprintf(escolhaUrl, sizeof escolhaUrl, "%s", i >= 0 ? pac[i].raw->url : "");
  gravarEscolha(perfilLido);
  ativo = i >= 0 && pac[i].nc ? i : -1;
  versao++;
}
void selospacote_escolher(int i) { travar(); selospacote_escolher_i(i); soltar(); }

static int selospacote_adicionar_i(const char *json, const char *origem) {
  J *raiz;
  Lote novo;
  int i, k, entrou = 0, limite = 0, dup = 0;
  char primeiro[512] = "";
  if (!json) return SELOS_ERR_JSON;
  selospacote_iniciar();
  raiz = jParse(json, strlen(json));
  if (!raiz || (raiz->t != J_OBJ && raiz->t != J_STR)) { jLiberar(raiz); return SELOS_ERR_JSON; }
  memset(&novo, 0, sizeof novo);
  loteDoValor(&novo, raiz, origem ? origem : "", 0);
  if (!novo.n) {
    int pareceu = raiz->t == J_OBJ && (jGet(raiz, "filters") || jGet(raiz, "imports") || jGet(raiz, "streamBadgeRules"));
    jLiberar(raiz);
    return pareceu ? SELOS_ERR_VAZIO : SELOS_ERR_JSON;
  }
  jLiberar(raiz);
  for (i = 0; i < novo.n; i++) {
    int existe = -1;
    for (k = 0; k < conta.n; k++) if (!strcasecmp(conta.p[k].url, novo.p[i].url)) { dup = 1; existe = -2; }
    if (existe == -2) continue;
    for (k = 0; k < tv.n; k++) if (!strcasecmp(tv.p[k].url, novo.p[i].url)) existe = k;
    if (existe >= 0) {
      pacoteLiberar(&tv.p[existe]); tv.p[existe] = novo.p[i]; memset(&novo.p[i], 0, sizeof novo.p[i]);
    } else if (conta.n + tv.n >= SELOS_MAX_PACOTES) { limite = 1; continue; }
    else { tv.p[tv.n++] = novo.p[i]; memset(&novo.p[i], 0, sizeof novo.p[i]); }
    if (!primeiro[0]) snprintf(primeiro, sizeof primeiro, "%s", tv.p[existe >= 0 ? existe : tv.n - 1].url);
    entrou++;
  }
  loteLiberar(&novo);
  if (!entrou) return limite ? SELOS_ERR_LIMITE : (dup ? SELOS_ERR_DUPLICADO : SELOS_ERR_VAZIO);
  gravarTv(perfilLido);
  escolhaExplicita = 1;
  snprintf(escolhaUrl, sizeof escolhaUrl, "%s", primeiro);
  gravarEscolha(perfilLido);
  reconstruir();
  return SELOS_OK;
}
int selospacote_adicionar(const char *json, const char *origem) { int r; travar(); r = selospacote_adicionar_i(json, origem); soltar(); return r; }

static int selospacote_remover_i(int i) {
  int k, era;
  selospacote_iniciar();
  if (i < 0 || i >= nPac || !pac[i].daTv) return 0;
  for (k = 0; k < tv.n; k++) if (&tv.p[k] == pac[i].raw) break;
  if (k >= tv.n) return 0;
  era = i == ativo;
  // reconstruir() ja descarta o desmontado; o cru so sai depois dele
  for (int j = 0; j < nPac; j++) pacoteDesmonta(&pac[j]);
  nPac = 0;
  pacoteLiberar(&tv.p[k]);
  for (; k + 1 < tv.n; k++) tv.p[k] = tv.p[k + 1];
  memset(&tv.p[tv.n - 1], 0, sizeof tv.p[0]);
  tv.n--;
  gravarTv(perfilLido);
  if (era) { escolhaExplicita = 1; escolhaUrl[0] = 0; gravarEscolha(perfilLido); }
  reconstruir();
  return 1;
}
int selospacote_remover(int i) { int r; travar(); r = selospacote_remover_i(i); soltar(); return r; }

// ---- casamento ---------------------------------------------------------------
// badgeMatchCandidates: cada campo quebrado em linhas, espacos colapsados, sem
// repeticao (sem caixa); com mais de um, entra tambem todos juntos por espaco.
#define CAND_MAX 48
#define CAND_BYTES 2200

// Um filtro casa se algum candidato (ou todos juntos) passa no regex.
static int filtroCasa(const FiltroC *c, char *const *cand, const size_t *lens, int ncand,
                      const char *junto, size_t jn) {
  int i, achou = 0;
  for (i = 0; i < ncand && !achou; i++) achou = regexjs_testar(c->re, cand[i], lens[i]);
  if (!achou && junto) achou = regexjs_testar(c->re, junto, jn);
  return achou;
}

// ---- pacotes embutidos (padrao + colorido) -------------------------------------
static char dirEmb[480];
static PacoteRaw embRaw[2];          // 0 padrao, 1 colorido
static Pacote embPac[2];
static int embTentou, embOk, modoCor;

// Mesma regra de tools/selos-xperience.py (plano()): o que vem depois de
// "/badges/" com "/" virando "_".
static void nomeLocal(char *d, size_t n, const char *url) {
  const char *p = strstr(url, "/badges/");
  size_t k = 0;
  p = p ? p + 8 : (strrchr(url, '/') ? strrchr(url, '/') + 1 : url);
  for (; *p && k + 1 < n; p++) d[k++] = *p == '/' ? '_' : *p;
  d[k] = 0;
}
static int existe(const char *caminho) { return access(caminho, R_OK) == 0; }

// O colorido cobre estes selos do padrao (mesmo papel, arte com cor); o resto
// o colorido nao tem e o padrao fica. Os niveis de grupo de release (remux-N,
// blu-ray-N, web-N) viram o selo unico de origem do colorido.
static int cobertoPeloColorido(const char *id) {
  static const char *const IDS[] = {
    "q-r", "q-b", "q-w", "src-webrip", "src-hdtv", "src-dvdrip",
    "r-4k", "r-1080", "r-720", "r-480p",
    "v-imax-e-2", "v-imax-2", "v-dv-hdr10p", "v-dv-hdr10", "v-dv-hdr", "a-dv",
    "v-hdr10p", "v-hdr10", "v-hdr", "v-hlg", "v-10bit", "v-sdr",
    "a-atmos-truehd", "a-dtsx-hdma", "a-atmos-ddplus", "a-dtsx-hd", "a-at", "a-dtsx",
    "a-th", "a-dtsma", "a-dtshd", "a-dp", "gv-dts-es", "a-dts", "a-dd",
    "ch-71", "ch-51", "s-h265", "s-h264",
  };
  size_t k;
  if (!strncmp(id, "remux-", 6) || !strncmp(id, "blu-ray-", 8) || !strncmp(id, "web-", 4)) return 1;
  for (k = 0; k < sizeof IDS / sizeof *IDS; k++) if (!strcmp(id, IDS[k])) return 1;
  return 0;
}
// Posicao do grupo do colorido na fileira (a mesma ordem dos grupos do padrao:
// origem/qualidade, resolucao, video, audio, canais, codec).
static int postoGrupoColorido(const char *g) {
  static const struct { const char *g; int rank; } T[] = {
    {"source", 3}, {"resolution", 2}, {"video-tech", 5}, {"bit-depth", 5},
    {"audio-tech", 6}, {"audio-channels", 7}, {"video-codec", 8},
  };
  size_t k;
  for (k = 0; k < sizeof T / sizeof *T; k++) if (!strcmp(g, T[k].g)) return T[k].rank;
  return 5;
}

static char *lerTudo(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  long n; char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  if (n <= 0 || n > 4 * 1024 * 1024) { fclose(f); return NULL; }
  b = malloc((size_t)n + 1);
  if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
  if (b) b[n] = 0;
  fclose(f);
  return b;
}

static void embLiberar(void) {
  int k;
  for (k = 0; k < 2; k++) { pacoteDesmonta(&embPac[k]); pacoteLiberar(&embRaw[k]); }
  embTentou = embOk = 0;
}

static int embCarregar(void) {
  static const char *const ARQ[2] = { "padrao", "colorido" };
  int k, i;
  if (embTentou) return embOk;
  embTentou = 1;
  if (!dirEmb[0]) return 0;
  for (k = 0; k < 2; k++) {
    char caminho[600], *txt;
    J *j;
    snprintf(caminho, sizeof caminho, "%s/selos/%s.json", dirEmb, ARQ[k]);
    txt = lerTudo(caminho);
    if (!txt) continue;
    j = jParse(txt, strlen(txt));
    free(txt);
    if (!j) continue;
    if (!normImport(j, ARQ[k], &embRaw[k])) { jLiberar(j); continue; }
    jLiberar(j);
    for (i = 0; i < embRaw[k].nf; i++) {
      FiltroRaw *f = &embRaw[k].f[i];
      char local[200], novo[700];
      if (!f->imagem[0]) continue;
#if SELOS_IMAGENS_NA_REDE
      (void)local; (void)novo;
#else
      nomeLocal(local, sizeof local, f->imagem);
      snprintf(novo, sizeof novo, "%s/selos/%s", dirEmb, local);
      if (!existe(novo)) {   // o estagio do Tizen/Android troca webp por png
        char *pt = strrchr(novo, '.');
        if (pt && !strcmp(pt, ".webp")) { strcpy(pt, ".png"); }
      }
      free(f->imagem);
      f->imagem = existe(novo) ? strdup(novo) : strdup("");
      if (!f->imagem) f->imagem = strdup("");
#endif
    }
    embPac[k].raw = &embRaw[k];
    nomeDe(&embPac[k]);
    compilaPacote(&embPac[k]);
    for (i = 0; i < embPac[k].nc; i++) {
      FiltroC *c = &embPac[k].c[i];
      const FiltroRaw *f = c->raw;
      int g, rank = 0;
      c->pub.arte = k == 0 ? SELO_ARTE_BRANCA : SELO_ARTE_COR;
      if (k == 0) {
        for (g = 0; g < embRaw[k].ng; g++) if (!strcmp(embRaw[k].g[g].id, f->grupo)) rank = g;
        c->subst = cobertoPeloColorido(f->id);
        c->pub.resolucao = !strcmp(f->grupo, "gr");
      } else {
        rank = postoGrupoColorido(f->grupo);
        c->pub.resolucao = !strcmp(f->grupo, "resolution");
      }
      c->rank = rank;
    }
  }
  embOk = embPac[0].nc > 0;
  printf("[selos] embutidos: padrao %d, colorido %d filtros%s\n", embPac[0].nc, embPac[1].nc,
         SELOS_IMAGENS_NA_REDE ? " (imagens do CDN)" : "");
  fflush(stdout);
  return embOk;
}

static void selospacote_dir_embutidos_i(const char *dir) {
  if (!dir) dir = "";
  if (!strcmp(dir, dirEmb)) return;
  embLiberar();
  snprintf(dirEmb, sizeof dirEmb, "%s", dir);
  versao++;
  // Agora, no fio principal da partida: o casamento roda depois no fio que
  // monta a lista de fontes, e nao deve ser ele a abrir os arquivos.
  embCarregar();
}
void selospacote_dir_embutidos(const char *dir) { travar(); selospacote_dir_embutidos_i(dir); soltar(); }
static void selospacote_colorido_i(int ligado) {
  ligado = ligado != 0;
  if (ligado == modoCor) return;
  modoCor = ligado;
  versao++;
}
void selospacote_colorido(int ligado) { if ((ligado != 0) == modoCor) return; travar(); selospacote_colorido_i(ligado); soltar(); }
int selospacote_embutidos_ok(void) { return embCarregar(); }

typedef struct { int rank; unsigned short id; } Acerto;
static int casarEmbutido(char *const *cand, const size_t *lens, int ncand, const char *junto, size_t jn,
                         unsigned short *ids, int max) {
  Acerto h[256];
  int nh = 0, ci, d, k, n, nd = embPac[0].nc;
  int colorido = modoCor && embPac[1].nc > 0;
  for (ci = 0; ci < nd && nh < 200; ci++) {
    const FiltroC *c = &embPac[0].c[ci];
    if (colorido && c->subst) continue;
    if (!filtroCasa(c, cand, lens, ncand, junto, jn)) continue;
    for (d = 0; d < nh; d++) if (!strcmp(embPac[0].c[h[d].id].chave, c->chave)) break;
    if (d < nh) continue;
    h[nh].rank = c->rank; h[nh].id = (unsigned short)ci; nh++;
  }
  if (colorido) for (ci = 0; ci < embPac[1].nc && nh < 250; ci++) {
    const FiltroC *c = &embPac[1].c[ci];
    if (!filtroCasa(c, cand, lens, ncand, junto, jn)) continue;
    h[nh].rank = c->rank; h[nh].id = (unsigned short)(nd + ci); nh++;
  }
  // Insercao estavel por posicao do grupo: dentro do grupo vale a ordem do pacote.
  for (k = 1; k < nh; k++) {
    Acerto x = h[k];
    for (d = k - 1; d >= 0 && h[d].rank > x.rank; d--) h[d + 1] = h[d];
    h[d + 1] = x;
  }
  for (n = 0; n < nh && n < max; n++) ids[n] = h[n].id;
  return n;
}

static int selospacote_casar_i(const char *const *campos, int nc, unsigned short *ids, int max) {
  char *arena, *cand[CAND_MAX + 1];
  size_t lens[CAND_MAX + 1];
  int ncand = 0, i, n = 0, ci;
  Pacote *p;
  char *junto;
  size_t jn = 0, jcap = 0;
  selospacote_iniciar();
  if (max <= 0) return 0;
  if (ativo < 0 && !embCarregar()) return 0;
  p = ativo >= 0 ? &pac[ativo] : NULL;
  arena = malloc((size_t)(CAND_MAX + 1) * CAND_BYTES);
  if (!arena) return 0;
  for (i = 0; i < nc && ncand < CAND_MAX; i++) {
    const char *s = campos[i], *l;
    if (!s) continue;
    for (l = s; *l && ncand < CAND_MAX;) {
      const char *e = l + strcspn(l, "\r\n");
      char *buf = arena + (size_t)ncand * CAND_BYTES, *t;
      size_t k = 0, m = (size_t)(e - l);
      int esp = 0, rep = 0, c;
      if (m > CAND_BYTES - 2) m = CAND_BYTES - 2;
      for (size_t q = 0; q < m; q++) {
        if (isspace((unsigned char)l[q])) { esp = 1; continue; }
        if (esp && k) buf[k++] = ' ';
        esp = 0; buf[k++] = l[q];
      }
      buf[k] = 0;
      for (c = 0; c < ncand && !rep; c++) if (!strcasecmp(cand[c], buf)) rep = 1;
      if (k && !rep) { cand[ncand] = buf; lens[ncand] = k; ncand++; }
      t = (char *)e;
      l = *t ? t + 1 + (t[0] == '\r' && t[1] == '\n') : t;
    }
  }
  if (!ncand) { free(arena); return 0; }
  if (ncand > 1) {
    for (i = 0; i < ncand; i++) jcap += lens[i] + 1;
    junto = malloc(jcap + 1);
    if (junto) {
      for (i = 0; i < ncand; i++) { if (i) junto[jn++] = ' '; memcpy(junto + jn, cand[i], lens[i]); jn += lens[i]; }
      junto[jn] = 0;
    }
  } else junto = NULL;
  if (!p) { n = casarEmbutido(cand, lens, ncand, junto, jn, ids, max); free(junto); free(arena); return n; }
  for (ci = 0; ci < p->nc && n < max; ci++) {
    const FiltroC *c = &p->c[ci];
    int d;
    if (!filtroCasa(c, cand, lens, ncand, junto, jn)) continue;
    for (d = 0; d < n; d++) if (!strcmp(p->c[ids[d]].chave, c->chave)) break;
    if (d < n) continue;
    ids[n++] = (unsigned short)ci;
  }
  free(junto); free(arena);
  return n;
}
int selospacote_casar(const char *const *campos, int nc, unsigned short *ids, int max) { int r; travar(); r = selospacote_casar_i(campos, nc, ids, max); soltar(); return r; }

const SeloFiltro *selospacote_filtro(unsigned short id) {
  if (ativo < 0) {
    int nd = embPac[0].nc;
    if (!embOk) return NULL;
    if (id < nd) return &embPac[0].c[id].pub;
    return id - nd < embPac[1].nc ? &embPac[1].c[id - nd].pub : NULL;
  }
  if (id >= pac[ativo].nc) return NULL;
  return &pac[ativo].c[id].pub;
}
