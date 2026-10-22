// Trava de serie na mesma TV/conta. Ver trava.h.
#include "trava.h"
#include "dados.h"
#include "catalogo.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static Trava tv[TRAVA_MAX];
static int nTv, carregado, proxId = 1;
static unsigned donoHash, geracao;

static unsigned hashStr(const char *s) {
  unsigned h = 5381;
  while (*s) h = h * 33u + (unsigned char)*s++;
  return h;
}

static void limparTexto(char *dst, size_t tam, const char *src) {
  size_t i = 0;
  for (; src && *src && i + 1 < tam; src++)
    dst[i++] = (*src == '\t' || *src == '\n' || *src == '\r') ? ' ' : *src;
  dst[i] = 0;
}

// ---------------------------------------------------------------- arquivo

static void gravar(void) {
  char *b = malloc((size_t)nTv * 160 + 64), *p;
  int i, k;
  if (!b) return;
  p = b + sprintf(b, "#travas 1 %u\n", donoHash);
  for (i = 0; i < nTv; i++) {
    const Trava *t = &tv[i];
    p += sprintf(p, "%d\t%d\t%d\t%d\t", t->id, t->modo, t->pin, t->pausada);
    for (k = 0; k < t->n; k++) p += sprintf(p, "%s%d", k ? "," : "", t->perfil[k]);
    p += sprintf(p, "\t%s\t%s\n", t->serie, t->titulo);
  }
  dados_gravar("travas.txt", b);
  free(b);
}

static void ler(void) {
  char *b, *l, *fim;
  unsigned h = 0;
  nTv = 0; proxId = 1; carregado = 1;
  b = dados_ler("travas.txt");
  if (!b) return;
  if (sscanf(b, "#travas 1 %u", &h) != 1 || h != donoHash) { free(b); return; }   // outra conta
  for (l = strchr(b, '\n'); l && *l && nTv < TRAVA_MAX; l = fim) {
    Trava t;
    char *c[7], *q;
    int k = 0;
    l++;
    fim = strchr(l, '\n');
    if (fim) *fim = 0;
    memset(&t, 0, sizeof t);
    for (q = l; k < 7; ) {
      c[k++] = q;
      q = strchr(q, '\t');
      if (!q) break;
      *q++ = 0;
    }
    if (k == 7 && atoi(c[0]) > 0) {
      char *s;
      t.id = atoi(c[0]); t.modo = atoi(c[1]) ? TRAVA_SEPARADOS : TRAVA_JUNTOS;
      t.pin = atoi(c[2]) != 0; t.pausada = atoi(c[3]) != 0;
      for (s = c[4]; *s && t.n < TRAVA_MEMBROS; ) {
        int p = atoi(s);
        if (p >= 1 && p <= CONTA_PERFIL_MAX) t.perfil[t.n++] = (unsigned char)p;
        s = strchr(s, ',');
        if (!s) break;
        s++;
      }
      limparTexto(t.serie, sizeof t.serie, c[5]);
      limparTexto(t.titulo, sizeof t.titulo, c[6]);
      if (t.n >= 2 && t.serie[0]) { tv[nTv++] = t; if (t.id >= proxId) proxId = t.id + 1; }
    }
    if (!fim) break;
  }
  free(b);
}

static void garantir(void) { if (!carregado) ler(); }
static void mudou(void) { geracao++; gravar(); }

void trava_dono(const char *conta) {
  unsigned h = (conta && conta[0]) ? hashStr(conta) : 0;
  pthread_mutex_lock(&mx);
  if (h != donoHash) { donoHash = h; nTv = 0; carregado = 0; geracao++; }
  pthread_mutex_unlock(&mx);
}
void trava_iniciar(void) { pthread_mutex_lock(&mx); garantir(); pthread_mutex_unlock(&mx); }
void trava_esquecer(void) { pthread_mutex_lock(&mx); nTv = 0; carregado = 0; geracao++; pthread_mutex_unlock(&mx); }
unsigned trava_geracao(void) { return geracao; }

// ---------------------------------------------------------------- gestao

static int achar(int id) {
  int i;
  for (i = 0; i < nTv; i++) if (tv[i].id == id) return i;
  return -1;
}
static int temMembro(const Trava *t, int p) {
  int k;
  for (k = 0; k < t->n; k++) if (t->perfil[k] == p) return 1;
  return 0;
}
int trava_tem_membro(const Trava *t, int perfil) { return temMembro(t, perfil); }

int trava_criar(const char *serie, const char *titulo, int modo, int pin, const int *perfis, int n) {
  Trava t;
  int k, id = -1;
  if (!serie || !serie[0] || !perfis) return -1;
  memset(&t, 0, sizeof t);
  limparTexto(t.serie, sizeof t.serie, serie);
  limparTexto(t.titulo, sizeof t.titulo, titulo ? titulo : "");
  t.modo = modo ? TRAVA_SEPARADOS : TRAVA_JUNTOS; t.pin = pin != 0;
  for (k = 0; k < n && t.n < TRAVA_MEMBROS; k++)
    if (perfis[k] >= 1 && perfis[k] <= CONTA_PERFIL_MAX && !temMembro(&t, perfis[k]))
      t.perfil[t.n++] = (unsigned char)perfis[k];
  if (t.n < 2) return -1;
  pthread_mutex_lock(&mx);
  garantir();
  if (nTv < TRAVA_MAX) { t.id = proxId++; tv[nTv++] = t; id = t.id; mudou(); }
  pthread_mutex_unlock(&mx);
  return id;
}

int trava_apagar(int id) {
  int i, r = 0;
  pthread_mutex_lock(&mx);
  garantir();
  i = achar(id);
  if (i >= 0) { memmove(&tv[i], &tv[i + 1], sizeof(Trava) * (size_t)(nTv - i - 1)); nTv--; mudou(); r = 1; }
  pthread_mutex_unlock(&mx);
  return r;
}

static int editar(int id, int campo, int v) {
  int i, r = 0;
  pthread_mutex_lock(&mx);
  garantir();
  i = achar(id);
  if (i >= 0) {
    int *p = campo == 0 ? &tv[i].modo : campo == 1 ? &tv[i].pin : &tv[i].pausada;
    int novo = campo == 0 ? (v ? TRAVA_SEPARADOS : TRAVA_JUNTOS) : (v != 0);
    if (*p != novo) { *p = novo; mudou(); }
    r = 1;
  }
  pthread_mutex_unlock(&mx);
  return r;
}
int trava_definir_modo(int id, int modo) { return editar(id, 0, modo); }
int trava_definir_pin(int id, int pin) { return editar(id, 1, pin); }
int trava_definir_pausada(int id, int p) { return editar(id, 2, p); }

int trava_adicionar_membro(int id, int perfil) {
  int i, r = 0;
  if (perfil < 1 || perfil > CONTA_PERFIL_MAX) return 0;
  pthread_mutex_lock(&mx);
  garantir();
  i = achar(id);
  if (i >= 0 && !temMembro(&tv[i], perfil) && tv[i].n < TRAVA_MEMBROS) {
    tv[i].perfil[tv[i].n++] = (unsigned char)perfil; mudou(); r = 1;
  }
  pthread_mutex_unlock(&mx);
  return r;
}

int trava_remover_membro(int id, int perfil) {
  int i, k, r = 0;
  pthread_mutex_lock(&mx);
  garantir();
  i = achar(id);
  if (i >= 0 && temMembro(&tv[i], perfil)) {
    Trava *t = &tv[i];
    for (k = 0; t->perfil[k] != perfil; k++) {}
    memmove(&t->perfil[k], &t->perfil[k + 1], (size_t)(t->n - k - 1));
    t->n--;
    r = 1;
    if (t->n < 2) {
      memmove(&tv[i], &tv[i + 1], sizeof(Trava) * (size_t)(nTv - i - 1)); nTv--; r = 2;
    }
    mudou();
  }
  pthread_mutex_unlock(&mx);
  return r;
}

// ---------------------------------------------------------------- leitura

int trava_n(void) { int n; pthread_mutex_lock(&mx); garantir(); n = nTv; pthread_mutex_unlock(&mx); return n; }
int trava_item(int i, Trava *s) {
  int r = 0;
  pthread_mutex_lock(&mx);
  garantir();
  if (i >= 0 && i < nTv) { if (s) *s = tv[i]; r = 1; }
  pthread_mutex_unlock(&mx);
  return r;
}
int trava_obter(int id, Trava *s) {
  int i, r = 0;
  pthread_mutex_lock(&mx);
  garantir();
  i = achar(id);
  if (i >= 0) { if (s) *s = tv[i]; r = 1; }
  pthread_mutex_unlock(&mx);
  return r;
}
int trava_da_serie(const char *serie, int perfil, int *ids, int max) {
  int i, n = 0;
  pthread_mutex_lock(&mx);
  garantir();
  for (i = 0; i < nTv; i++)
    if (!strcmp(tv[i].serie, serie) && (!perfil || temMembro(&tv[i], perfil))) {
      if (ids && n < max) ids[n] = tv[i].id;
      n++;
    }
  pthread_mutex_unlock(&mx);
  return n;
}

// ---------------------------------------------------------------- regra

int trava_posicoes(const Trava *t, TravaPos *v) {
  int k;
  for (k = 0; k < t->n; k++) {
    int pt = 0, pe = 0;
    long long q;
    if (!diario_ultimo_episodio(t->perfil[k], t->serie, &pt, &pe, &q) || pt < 0) pt = pe = 0;
    v[k].perfil = t->perfil[k]; v[k].t = pt; v[k].e = pe;
  }
  return t->n;
}

static int cmp(int t1, int e1, int t2, int e2) { return t1 != t2 ? (t1 < t2 ? -1 : 1) : (e1 < e2 ? -1 : e1 > e2); }

int trava_estado(const Trava *t, int perfil, TravaEstado *s) {
  TravaPos v[TRAVA_MEMBROS];
  int k, n = trava_posicoes(t, v), first = 1, meT = 0, meE = 0;
  memset(s, 0, sizeof *s);
  for (k = 0; k < n; k++) {
    if (v[k].perfil == perfil) { meT = v[k].t; meE = v[k].e; continue; }
    if (first || cmp(v[k].t, v[k].e, s->minT, s->minE) < 0) { s->minT = v[k].t; s->minE = v[k].e; s->quem = 0; first = 0; }
    if (v[k].t == s->minT && v[k].e == s->minE) s->quem |= 1u << (v[k].perfil - 1);
  }
  if (first) return 0;
  s->esperando = cmp(meT, meE, s->minT, s->minE) > 0;
  s->atras = (s->esperando && meT == s->minT) ? meE - s->minE : -1;
  return 1;
}

unsigned trava_quem_segura(const Trava *t, int perfil, int te, int ee, int epsNaTemp) {
  TravaEstado s;
  long long q;
  if (t->pausada || t->modo == TRAVA_JUNTOS || te <= 0 || ee <= 0 || !temMembro(t, perfil)) return 0;
  if (diario_episodio(perfil, t->serie, te, ee, &q) > 0) return 0;   // rever nao e travado
  if (!trava_estado(t, perfil, &s)) return 0;
  if (cmp(te, ee, s.minT, s.minE) <= 0) return 0;
  if (s.minT == 0 && s.minE == 0) { if (te == 1 && ee == 1) return 0; }
  else if (te == s.minT && ee == s.minE + 1) return 0;
  else if (epsNaTemp > 0 && s.minE >= epsNaTemp && te == s.minT + 1 && ee == 1) return 0;
  return s.quem;
}
int trava_liberado(const Trava *t, int perfil, int te, int ee, int epsNaTemp) {
  return trava_quem_segura(t, perfil, te, ee, epsNaTemp) == 0;
}

int trava_bloqueio(const char *serie, int perfil, int te, int ee) {
  int ids[TRAVA_MAX], i, n = trava_da_serie(serie, perfil, ids, TRAVA_MAX);
  for (i = 0; i < n && i < TRAVA_MAX; i++) {
    Trava t;
    if (trava_obter(ids[i], &t)) {
      TravaEstado s;
      int eps = 0;
      if (ee == 1 && trava_estado(&t, perfil, &s) && te == s.minT + 1) {
        int ix = cat_indice_titulo(serie, -1), count = ix >= 0 ? cat_n_episodios(ix) : 0;
        for (int k = 0; k < count; k++) {
          const CatEp *ep = cat_episodio(ix, k);
          if (ep && ep->temporada == s.minT && ep->episodio > eps) eps = ep->episodio;
        }
      }
      if (!trava_liberado(&t, perfil, te, ee, eps)) return t.id;
    }
  }
  return 0;
}

int trava_juntos_alvos(const char *serie, int perfil, int *alvos, int max) {
  int i, k, n = 0;
  pthread_mutex_lock(&mx);
  garantir();
  for (i = 0; i < nTv; i++) {
    if (strcmp(tv[i].serie, serie) || tv[i].modo != TRAVA_JUNTOS || tv[i].pausada || !temMembro(&tv[i], perfil)) continue;
    for (k = 0; k < tv[i].n; k++) {
      int p = tv[i].perfil[k], j, dup = 0;
      if (p == perfil) continue;
      for (j = 0; j < n; j++) if (alvos[j] == p) dup = 1;
      if (!dup && n < max) alvos[n++] = p;
    }
  }
  pthread_mutex_unlock(&mx);
  return n;
}
