// Trava de serie: modelo, regra pura e arquivo. Ver trava.h.
#include "trava.h"
#include "dados.h"
#include "idioma.h"
#include "js.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ================================================================ regra pura

int trava_ep_cmp(int t1, int e1, int t2, int e2) {
  if (t1 != t2) return t1 < t2 ? -1 : 1;
  if (e1 != e2) return e1 < e2 ? -1 : 1;
  return 0;
}

static int comecou(const TravaMembro *m) { return m->t > 0 && m->e > 0; }

// O proximo episodio depois de (t, e). Sem saber quantos episodios a temporada
// tem, o proximo e (t, e+1): na virada de temporada isso SEGURA a pessoa no
// E1 da seguinte ate o catalogo dizer que (t, e) era o ultimo. Errar para o
// lado de esperar e o erro seguro — "Assistir mesmo assim" existe para isso.
static void proximo(int t, int e, TravaNEps nEps, void *ctx, int *pt, int *pe) {
  if (t <= 0 || e <= 0) { *pt = 1; *pe = 1; return; }
  { int n = nEps ? nEps(ctx, t) : 0;
    if (n > 0 && e >= n) { *pt = t + 1; *pe = 1; return; } }
  *pt = t; *pe = e + 1;
}

int trava_membro_deixa(const TravaMembro *m, int t, int e, TravaNEps nEps, void *ctx) {
  int pt, pe;
  if (t <= 0) return 1;                     // especial fica fora da regra
  if (comecou(m) && trava_ep_cmp(t, e, m->t, m->e) <= 0) return 1;
  proximo(comecou(m) ? m->t : 0, comecou(m) ? m->e : 0, nEps, ctx, &pt, &pe);
  return trava_ep_cmp(t, e, pt, pe) <= 0;
}

int trava_n_dentro(const Trava *tv) {
  int i, k = 0;
  for (i = 0; tv && i < tv->n; i++) if (tv->m[i].estado == TRAVA_DENTRO) k++;
  return k;
}

int trava_liberado(const Trava *tv, int eu, int t, int e, int jaVisto,
                   TravaNEps nEps, void *ctx, int *quem, int *nQuem) {
  int i, k = 0;
  if (nQuem) *nQuem = 0;
  if (!tv || tv->modo != TRAVA_SEPARADOS || tv->pausada || t <= 0 || jaVisto) return 1;
  for (i = 0; i < tv->n; i++) {
    if (i == eu || tv->m[i].estado != TRAVA_DENTRO) continue;
    if (!trava_membro_deixa(&tv->m[i], t, e, nEps, ctx)) {
      if (quem) quem[k] = i;
      k++;
    }
  }
  if (nQuem) *nQuem = k;
  return k == 0;
}

int trava_limite(const Trava *tv, int eu, TravaNEps nEps, void *ctx, int *t, int *e) {
  int i, achou = 0, mt = 0, me = 0;
  if (!tv || tv->modo != TRAVA_SEPARADOS || tv->pausada) return 0;
  for (i = 0; i < tv->n; i++) {
    const TravaMembro *m = &tv->m[i];
    int pt, pe;
    if (i == eu || m->estado != TRAVA_DENTRO) continue;
    proximo(comecou(m) ? m->t : 0, comecou(m) ? m->e : 0, nEps, ctx, &pt, &pe);
    if (!achou || trava_ep_cmp(pt, pe, mt, me) < 0) { mt = pt; me = pe; achou = 1; }
  }
  if (!achou) return 0;
  if (t) *t = mt;
  if (e) *e = me;
  return 1;
}

int trava_membro_do_perfil(const Trava *tv, int perfil) {
  int i;
  if (!tv || perfil < 1) return -1;
  for (i = 0; i < tv->n; i++) if (tv->m[i].perfil == perfil) return i;
  return -1;
}

void trava_ep_texto(int t, int e, char *dst, unsigned tam) {
  snprintf(dst, tam, i18n("T%dE%d"), t, e);
}

// "Jerry", "Jerry e Sandra", "Jerry e mais 2".
static void juntarNomes(const int *quem, int n, TravaNome nomeDe, void *ctx,
                        char *dst, unsigned tam) {
  const char *a = n > 0 && nomeDe ? nomeDe(ctx, quem[0]) : "";
  if (!a) a = "";
  if (n <= 1) snprintf(dst, tam, "%s", a);
  else if (n == 2) {
    const char *b = nomeDe ? nomeDe(ctx, quem[1]) : "";
    snprintf(dst, tam, i18n("%s e %s"), a, b ? b : "");
  } else snprintf(dst, tam, i18n("%s e mais %d"), a, n - 1);
}

void trava_esperando_texto(const int *quem, int n, TravaNome nomeDe, void *ctx,
                           char *dst, unsigned tam) {
  char nomes[160];
  if (n < 1) { dst[0] = 0; return; }
  juntarNomes(quem, n, nomeDe, ctx, nomes, sizeof nomes);
  snprintf(dst, tam, i18n("Esperando %s"), nomes);
}

int trava_frase(const Trava *tv, int eu, TravaNome nomeDe, void *ctx,
                char *dst, unsigned tam) {
  int i, algum = 0, iguais = 1, atras[TRAVA_MEMBROS], nAtras = 0, frente = -1;
  const TravaMembro *me;
  char ep[32], ep2[32], nomes[160];
  dst[0] = 0;
  if (!tv || eu < 0 || eu >= tv->n) return TRAVA_FRASE_NADA;
  me = &tv->m[eu];
  if (tv->pausada) {
    snprintf(dst, tam, "%s", i18n("Trava pausada. Ninguém espera."));
    return TRAVA_FRASE_PAUSADA;
  }
  for (i = 0; i < tv->n; i++) {
    const TravaMembro *m = &tv->m[i];
    if (m->estado != TRAVA_DENTRO && i != eu) continue;
    if (comecou(m)) algum = 1;
    if (trava_ep_cmp(m->t, m->e, me->t, me->e)) iguais = 0;
  }
  if (!algum) {
    snprintf(dst, tam, "%s", i18n("Vocês ainda não começaram."));
    return TRAVA_FRASE_NAO_COMECARAM;
  }
  if (tv->modo == TRAVA_JUNTOS) {
    int mt = 0, mE = 0;
    for (i = 0; i < tv->n; i++)
      if ((tv->m[i].estado == TRAVA_DENTRO || i == eu) &&
          trava_ep_cmp(tv->m[i].t, tv->m[i].e, mt, mE) > 0) { mt = tv->m[i].t; mE = tv->m[i].e; }
    trava_ep_texto(mt, mE, ep, sizeof ep);
    snprintf(dst, tam, i18n("Juntos: vocês estão no %s."), ep);
    return TRAVA_FRASE_JUNTOS;
  }
  if (iguais) {
    trava_ep_texto(me->t, me->e, ep, sizeof ep);
    snprintf(dst, tam, i18n("Vocês estão no mesmo episódio, %s."), ep);
    return TRAVA_FRASE_MESMO;
  }
  for (i = 0; i < tv->n; i++) {
    const TravaMembro *m = &tv->m[i];
    if (i == eu || m->estado != TRAVA_DENTRO) continue;
    if (trava_ep_cmp(m->t, m->e, me->t, me->e) < 0) atras[nAtras++] = i;
    else if (trava_ep_cmp(m->t, m->e, me->t, me->e) > 0 &&
             (frente < 0 || trava_ep_cmp(m->t, m->e, tv->m[frente].t, tv->m[frente].e) > 0))
      frente = i;
  }
  if (nAtras > 0) {
    // O episodio em que VOCE para: o proximo depois do seu (na mesma
    // temporada; a frase nao conhece a contagem de episodios).
    if (comecou(me)) trava_ep_texto(me->t, me->e + 1, ep, sizeof ep);
    else trava_ep_texto(1, 1, ep, sizeof ep);
    if (nAtras == 1) {
      const TravaMembro *m = &tv->m[atras[0]];
      const char *nome = nomeDe ? nomeDe(ctx, atras[0]) : "";
      if (!nome) nome = "";
      if (!comecou(m))
        snprintf(dst, tam, i18n("%s ainda não começou: você espera no %s."), nome, ep);
      else if (m->t == me->t && me->e - m->e == 1)
        snprintf(dst, tam, i18n("%s está 1 episódio atrás: você espera no %s."), nome, ep);
      else if (m->t == me->t)
        snprintf(dst, tam, i18n("%s está %d episódios atrás: você espera no %s."),
                 nome, me->e - m->e, ep);
      else {
        trava_ep_texto(m->t, m->e, ep2, sizeof ep2);
        snprintf(dst, tam, i18n("%s está no %s: você espera no %s."), nome, ep2, ep);
      }
      return TRAVA_FRASE_ESPERA_UM;
    }
    juntarNomes(atras, nAtras, nomeDe, ctx, nomes, sizeof nomes);
    snprintf(dst, tam, i18n("Você espera por %s no %s."), nomes, ep);
    return TRAVA_FRASE_ESPERA_VARIOS;
  }
  if (frente >= 0) {
    const char *nome = nomeDe ? nomeDe(ctx, frente) : "";
    trava_ep_texto(tv->m[frente].t, tv->m[frente].e, ep, sizeof ep);
    snprintf(dst, tam, i18n("Você está atrás: %s está no %s."), nome ? nome : "", ep);
    return TRAVA_FRASE_ATRAS;
  }
  return TRAVA_FRASE_NADA;
}

// ================================================================ estado

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static Trava travas[TRAVA_MAX];
static int nTravas;
static TravaOp fila[TRAVA_FILA_MAX];
static int nFila;
static char conta[80];
static TravaPerfil perfis[TRAVA_PERFIS_MAX];
static int nPerfis, ativo = 1;
static char etag[96];
static long long rev;
static unsigned geracao;
static long long (*relogio)(void);
static TravaAlcance alcances[8];
static int nAlcances;

static long long agora(void) { return relogio ? relogio() : (long long)time(NULL); }
void trava_relogio(long long (*f)(void)) { relogio = f; }
unsigned trava_geracao(void) { return geracao; }

static void limpo(char *d, size_t tam, const char *s) {
  size_t k = 0;
  if (!s) s = "";
  while (s[k] && k + 1 < tam) {
    d[k] = (s[k] == '\t' || s[k] == '\n' || s[k] == '\r') ? ' ' : s[k];
    k++;
  }
  d[k] = 0;
}

static void serieDe(const char *s, char *d, size_t tam) {
  size_t k = 0;
  d[0] = 0;
  if (!s) return;
  while (s[k] && s[k] != ':' && k + 1 < tam) { d[k] = s[k]; k++; }
  d[k] = 0;
}

static void arquivo(char *d, size_t tam) {
  char u[64];
  size_t k = 0, j = 0;
  while (conta[k] && j + 1 < sizeof u) {
    char c = conta[k++];
    if (isalnum((unsigned char)c) || c == '-') u[j++] = c;
  }
  u[j] = 0;
  snprintf(d, tam, "travas-%s.txt", u[0] ? u : "local");
}

// So com o mutex.
static void gravar(void) {
  char nome[96], *buf, *p;
  size_t tam = 256 + (size_t)nFila * 260;
  int i, k;
  for (i = 0; i < nTravas; i++) tam += 900 + (size_t)travas[i].n * 200;
  buf = (char *)malloc(tam);
  if (!buf) return;
  p = buf;
  p += snprintf(p, tam - (size_t)(p - buf), "v1\nR\t%lld\t%s\n", rev, etag);
  for (i = 0; i < nTravas; i++) {
    const Trava *t = &travas[i];
    p += snprintf(p, tam - (size_t)(p - buf), "T\t%s\t%s\t%d\t%d\t%d\t%d\t%lld\t%lld\t%s\t%s\t%s\n",
                  t->id, t->serie, t->modo, t->pin, t->pausada, t->local,
                  t->criado, t->atualizado, t->criador[0] ? t->criador : "-",
                  t->titulo, t->poster);
    for (k = 0; k < t->n; k++) {
      const TravaMembro *m = &t->m[k];
      p += snprintf(p, tam - (size_t)(p - buf), "M\t%d\t%d\t%d\t%d\t%lld\t%s\n",
                    m->perfil, m->estado, m->t, m->e, m->em, m->pessoa[0] ? m->pessoa : "-");
    }
  }
  for (i = 0; i < nFila; i++)
    p += snprintf(p, tam - (size_t)(p - buf), "O\t%s\t%s\t%d\t%d\t%d\t%s\n",
                  fila[i].op, fila[i].id, fila[i].a, fila[i].b, fila[i].c,
                  fila[i].pessoa[0] ? fila[i].pessoa : "-");
  arquivo(nome, sizeof nome);
  dados_gravar(nome, buf);
  free(buf);
}

static void mudou(void) { geracao++; gravar(); }

// Separa `l` (modificada) em ate `max` campos por TAB.
static int campos(char *l, char **c, int max) {
  int n = 0;
  c[n++] = l;
  while (n < max) {
    char *t = strchr(c[n - 1], '\t');
    if (!t) break;
    *t = 0;
    c[n++] = t + 1;
  }
  return n;
}

void trava_carregar(void) {
  char nome[96], *s, *l;
  pthread_mutex_lock(&mtx);
  nTravas = nFila = nAlcances = 0;
  etag[0] = 0;
  rev = 0;
  arquivo(nome, sizeof nome);
  s = dados_ler(nome);
  if (s) {
    l = s;
    while (l && *l) {
      char *fim = strchr(l, '\n'), *c[12];
      int n;
      if (fim) *fim = 0;
      n = campos(l, c, 12);
      if (c[0][0] == 'R' && n >= 2) {
        rev = atoll(c[1]);
        if (n >= 3) snprintf(etag, sizeof etag, "%s", c[2]);
      } else if (c[0][0] == 'T' && n >= 12 && nTravas < TRAVA_MAX) {
        Trava *t = &travas[nTravas++];
        memset(t, 0, sizeof *t);
        snprintf(t->id, sizeof t->id, "%s", c[1]);
        snprintf(t->serie, sizeof t->serie, "%s", c[2]);
        t->modo = atoi(c[3]); t->pin = atoi(c[4]); t->pausada = atoi(c[5]);
        t->local = atoi(c[6]);
        t->criado = atoll(c[7]); t->atualizado = atoll(c[8]);
        snprintf(t->criador, sizeof t->criador, "%s", strcmp(c[9], "-") ? c[9] : "");
        snprintf(t->titulo, sizeof t->titulo, "%s", c[10]);
        snprintf(t->poster, sizeof t->poster, "%s", c[11]);
      } else if (c[0][0] == 'M' && n >= 7 && nTravas > 0) {
        Trava *t = &travas[nTravas - 1];
        if (t->n < TRAVA_MEMBROS) {
          TravaMembro *m = &t->m[t->n++];
          memset(m, 0, sizeof *m);
          m->perfil = atoi(c[1]); m->estado = atoi(c[2]);
          m->t = atoi(c[3]); m->e = atoi(c[4]); m->em = atoll(c[5]);
          snprintf(m->pessoa, sizeof m->pessoa, "%s", strcmp(c[6], "-") ? c[6] : "");
        }
      } else if (c[0][0] == 'O' && n >= 7 && nFila < TRAVA_FILA_MAX) {
        TravaOp *o = &fila[nFila++];
        memset(o, 0, sizeof *o);
        snprintf(o->op, sizeof o->op, "%s", c[1]);
        snprintf(o->id, sizeof o->id, "%s", c[2]);
        o->a = atoi(c[3]); o->b = atoi(c[4]); o->c = atoi(c[5]);
        snprintf(o->pessoa, sizeof o->pessoa, "%s", strcmp(c[6], "-") ? c[6] : "");
      }
      l = fim ? fim + 1 : NULL;
    }
    free(s);
  }
  geracao++;
  pthread_mutex_unlock(&mtx);
  if (nTravas || nFila)
    printf("[trava] %d travas, %d operacoes na fila\n", nTravas, nFila);
}

void trava_esquecer(void) {
  pthread_mutex_lock(&mtx);
  nTravas = nFila = nAlcances = nPerfis = 0;
  conta[0] = etag[0] = 0;
  rev = 0;
  geracao++;
  pthread_mutex_unlock(&mtx);
}

void trava_definir_conta(const char *sub) {
  int troca;
  pthread_mutex_lock(&mtx);
  troca = strcmp(conta, sub ? sub : "") != 0;
  if (troca) snprintf(conta, sizeof conta, "%s", sub ? sub : "");
  pthread_mutex_unlock(&mtx);
  if (troca) trava_carregar();
}
const char *trava_conta(void) { return conta; }

void trava_definir_perfis(const TravaPerfil *v, int n, int at) {
  int i;
  pthread_mutex_lock(&mtx);
  if (n > TRAVA_PERFIS_MAX) n = TRAVA_PERFIS_MAX;
  if (n < 0) n = 0;
  for (i = 0; i < n; i++) perfis[i] = v[i];
  nPerfis = n;
  ativo = at > 0 ? at : 1;
  pthread_mutex_unlock(&mtx);
}
int trava_n_perfis(void) { return nPerfis; }
const TravaPerfil *trava_perfil(int i) { return i >= 0 && i < nPerfis ? &perfis[i] : NULL; }
const TravaPerfil *trava_perfil_indice(int indice) {
  int i;
  for (i = 0; i < nPerfis; i++) if (perfis[i].indice == indice) return &perfis[i];
  return NULL;
}
int trava_perfil_ativo(void) { return ativo; }

void trava_pessoa_do_perfil(int indice, char *dst, unsigned tam) {
  const TravaPerfil *p = trava_perfil_indice(indice);
  dst[0] = 0;
  if (!conta[0] || indice < 1) return;
  // O PRINCIPAL nao manda X-Nuvio-Perfil e e "nuvio:<sub>"; sem a lista,
  // o perfil 1 e o principal (a mesma regra do web e de perfis.c).
  if ((p && p->primario) || (!p && indice == 1)) snprintf(dst, tam, "nuvio:%s", conta);
  else snprintf(dst, tam, "nuvio:%s:%d", conta, indice);
}

// "nuvio:<sub>" -> perfil principal; "nuvio:<sub>:<i>" -> i; outro -> 0.
static int perfilDaPessoa(const char *pessoa) {
  size_t n = strlen(conta);
  int i;
  if (!conta[0] || strncmp(pessoa, "nuvio:", 6) || strncmp(pessoa + 6, conta, n)) return 0;
  pessoa += 6 + n;
  if (!*pessoa) {
    for (i = 0; i < nPerfis; i++) if (perfis[i].primario) return perfis[i].indice;
    return 1;
  }
  if (*pessoa != ':' || !isdigit((unsigned char)pessoa[1])) return 0;
  return atoi(pessoa + 1);
}

int trava_n(void) { return nTravas; }
const Trava *trava_i(int i) { return i >= 0 && i < nTravas ? &travas[i] : NULL; }
static int achar(const char *id) {
  int i;
  for (i = 0; id && i < nTravas; i++) if (!strcmp(travas[i].id, id)) return i;
  return -1;
}
const Trava *trava_por_id(const char *id) { int i = achar(id); return i >= 0 ? &travas[i] : NULL; }

int trava_da_serie(const char *serie, int perfil, int *idx, int max) {
  char s[24];
  int i, k = 0;
  serieDe(serie, s, sizeof s);
  if (!s[0]) return 0;
  for (i = 0; i < nTravas; i++) {
    int m;
    if (strcmp(travas[i].serie, s)) continue;
    m = trava_membro_do_perfil(&travas[i], perfil);
    if (m < 0 || travas[i].m[m].estado != TRAVA_DENTRO) continue;
    if (idx && k < max) idx[k] = i;
    k++;
  }
  return k;
}

// So com o mutex. Nunca passa do teto: sem lugar, a operacao mais velha de
// "passo" sai (a posicao vai de novo no proximo passo, e o GET reconcilia).
static void enfileirar(const char *op, const char *id, const char *pessoa, int a, int b, int c) {
  TravaOp *o;
  if (nFila >= TRAVA_FILA_MAX) {
    int i;
    for (i = 0; i < nFila; i++) if (!strcmp(fila[i].op, "passo")) break;
    if (i == nFila) return;
    memmove(&fila[i], &fila[i + 1], sizeof fila[0] * (size_t)(nFila - i - 1));
    nFila--;
  }
  o = &fila[nFila++];
  memset(o, 0, sizeof *o);
  snprintf(o->op, sizeof o->op, "%s", op);
  snprintf(o->id, sizeof o->id, "%s", id);
  if (pessoa) snprintf(o->pessoa, sizeof o->pessoa, "%s", pessoa);
  o->a = a; o->b = b; o->c = c;
}

static void preencherMembro(TravaMembro *m, int perfil) {
  memset(m, 0, sizeof *m);
  m->perfil = perfil;
  m->estado = TRAVA_DENTRO;
  trava_pessoa_do_perfil(perfil, m->pessoa, sizeof m->pessoa);
}

int trava_criar(const char *serie, const char *titulo, const char *poster,
                int modo, int pin, const int *pf, int n) {
  Trava *t;
  int i, k, r;
  char s[24];
  serieDe(serie, s, sizeof s);
  if (!s[0]) return -1;
  pthread_mutex_lock(&mtx);
  if (nTravas >= TRAVA_MAX) { pthread_mutex_unlock(&mtx); return -1; }
  t = &travas[nTravas];
  memset(t, 0, sizeof *t);
  snprintf(t->id, sizeof t->id, "l-%08x%04x", (unsigned)(agora() & 0xffffffff),
           (unsigned)(rand() & 0xffff));
  snprintf(t->serie, sizeof t->serie, "%s", s);
  limpo(t->titulo, sizeof t->titulo, titulo);
  limpo(t->poster, sizeof t->poster, poster);
  t->modo = modo == TRAVA_JUNTOS ? TRAVA_JUNTOS : TRAVA_SEPARADOS;
  t->pin = pin ? 1 : 0;
  t->local = 1;
  t->criado = t->atualizado = agora();
  trava_pessoa_do_perfil(ativo, t->criador, sizeof t->criador);
  preencherMembro(&t->m[t->n++], ativo);
  for (i = 0; pf && i < n && t->n < TRAVA_MEMBROS; i++) {
    int dup = 0;
    if (pf[i] < 1) continue;
    for (k = 0; k < t->n; k++) if (t->m[k].perfil == pf[i]) dup = 1;
    if (!dup) preencherMembro(&t->m[t->n++], pf[i]);
  }
  if (t->n < 2) { pthread_mutex_unlock(&mtx); return -1; }
  r = nTravas++;
  enfileirar("criar", t->id, NULL, 0, 0, 0);
  mudou();
  pthread_mutex_unlock(&mtx);
  printf("[trava] criada %s em %s (%s, %d membros)\n", travas[r].id, s,
         modo == TRAVA_JUNTOS ? "juntos" : "separados", travas[r].n);
  return r;
}

enum { CAMPO_MODO, CAMPO_PIN, CAMPO_PAUSADA };
static int definirCampo(const char *id, int qual, int v) {
  static const char *const ops[] = { "modo", "pin", "pausar" };
  int i, r = 0, *c;
  pthread_mutex_lock(&mtx);
  i = achar(id);
  if (i >= 0) {
    c = qual == CAMPO_MODO ? &travas[i].modo : qual == CAMPO_PIN ? &travas[i].pin
                                                                 : &travas[i].pausada;
    if (*c != v) {
      *c = v;
      travas[i].atualizado = agora();
      enfileirar(ops[qual], id, NULL, v, 0, 0);
      mudou();
      r = 1;
    }
  }
  pthread_mutex_unlock(&mtx);
  return r;
}
int trava_definir_modo(const char *id, int modo) {
  return definirCampo(id, CAMPO_MODO, modo == TRAVA_JUNTOS ? 0 : 1);
}
int trava_definir_pin(const char *id, int pin) { return definirCampo(id, CAMPO_PIN, pin ? 1 : 0); }
int trava_definir_pausada(const char *id, int p) { return definirCampo(id, CAMPO_PAUSADA, p ? 1 : 0); }

int trava_definir_membros(const char *id, const int *pf, int n) {
  int i, k, r = 0;
  pthread_mutex_lock(&mtx);
  i = achar(id);
  if (i >= 0) {
    Trava *t = &travas[i];
    Trava novo = *t;
    novo.n = 0;
    // O ativo fica sempre (sair e outro gesto: trava_apagar ou "Sair").
    for (k = 0; k < t->n; k++)
      if (t->m[k].perfil == ativo || t->m[k].perfil == 0) novo.m[novo.n++] = t->m[k];
    for (k = 0; pf && k < n && novo.n < TRAVA_MEMBROS; k++) {
      int j, dup = 0, velho = -1;
      if (pf[k] < 1) continue;
      for (j = 0; j < novo.n; j++) if (novo.m[j].perfil == pf[k]) dup = 1;
      if (dup) continue;
      for (j = 0; j < t->n; j++) if (t->m[j].perfil == pf[k]) velho = j;
      if (velho >= 0) { novo.m[novo.n] = t->m[velho]; novo.m[novo.n].estado = TRAVA_DENTRO; novo.n++; }
      else preencherMembro(&novo.m[novo.n++], pf[k]);
    }
    if (novo.n >= 2) {
      int igual = novo.n == t->n;
      for (k = 0; igual && k < novo.n; k++) igual = novo.m[k].perfil == t->m[k].perfil;
      if (!igual) {
        novo.atualizado = agora();
        *t = novo;
        enfileirar("membros", id, NULL, 0, 0, 0);
        mudou();
        r = 1;
      }
    }
  }
  pthread_mutex_unlock(&mtx);
  return r;
}

int trava_apagar(const char *id) {
  int i, r = 0;
  pthread_mutex_lock(&mtx);
  i = achar(id);
  if (i >= 0) {
    int local = travas[i].local;
    memmove(&travas[i], &travas[i + 1], sizeof travas[0] * (size_t)(nTravas - i - 1));
    nTravas--;
    if (local) {
      // Nunca chegou ao servidor: as operacoes dela saem junto.
      int k = 0, j;
      for (j = 0; j < nFila; j++) if (strcmp(fila[j].id, id)) fila[k++] = fila[j];
      nFila = k;
    } else enfileirar("apagar", id, NULL, 0, 0, 0);
    mudou();
    r = 1;
  }
  pthread_mutex_unlock(&mtx);
  return r;
}

// So com o mutex. Registra "alcancou voce" quando o membro `k` passa do ponto
// em que o perfil ativo esperava por ele.
static void notarAlcance(const Trava *t, int k, int antesT, int antesE) {
  int eu = trava_membro_do_perfil(t, ativo);
  const TravaMembro *me, *m;
  if (eu < 0 || k == eu || t->modo != TRAVA_SEPARADOS || t->pausada) return;
  me = &t->m[eu]; m = &t->m[k];
  if (!comecou(me)) return;
  if (trava_ep_cmp(antesT, antesE, me->t, me->e) < 0 &&
      trava_ep_cmp(m->t, m->e, me->t, me->e) >= 0 && nAlcances < 8) {
    TravaAlcance *a = &alcances[nAlcances++];
    snprintf(a->id, sizeof a->id, "%s", t->id);
    a->membro = k; a->t = m->t; a->e = m->e;
  }
}

int trava_passo(const char *serie, int perfil, int t, int e, int desfazer) {
  char s[24];
  int i, k = 0;
  serieDe(serie, s, sizeof s);
  if (!s[0] || t <= 0 || e <= 0) return 0;
  pthread_mutex_lock(&mtx);
  for (i = 0; i < nTravas; i++) {
    Trava *tv = &travas[i];
    int m;
    if (strcmp(tv->serie, s)) continue;
    m = trava_membro_do_perfil(tv, perfil);
    if (m < 0 || tv->m[m].estado != TRAVA_DENTRO) continue;
    if (desfazer ? trava_ep_cmp(t, e, tv->m[m].t, tv->m[m].e) != 0
                 : trava_ep_cmp(t, e, tv->m[m].t, tv->m[m].e) > 0) {
      int at = tv->m[m].t, ae = tv->m[m].e;
      tv->m[m].t = t; tv->m[m].e = e; tv->m[m].em = agora();
      tv->atualizado = tv->m[m].em;
      if (!desfazer) notarAlcance(tv, m, at, ae);
      enfileirar("passo", tv->id, tv->m[m].pessoa, t, e, desfazer ? 1 : 0);
      k++;
    }
  }
  if (k) mudou();
  pthread_mutex_unlock(&mtx);
  return k;
}

int trava_posicao(const char *id, int perfil, int t, int e, long long em) {
  int i, m, r = 0;
  pthread_mutex_lock(&mtx);
  i = achar(id);
  if (i >= 0 && (m = trava_membro_do_perfil(&travas[i], perfil)) >= 0 &&
      trava_ep_cmp(t, e, travas[i].m[m].t, travas[i].m[m].e) > 0) {
    int at = travas[i].m[m].t, ae = travas[i].m[m].e;
    travas[i].m[m].t = t; travas[i].m[m].e = e; travas[i].m[m].em = em;
    notarAlcance(&travas[i], m, at, ae);
    mudou();
    r = 1;
  }
  pthread_mutex_unlock(&mtx);
  return r;
}

int trava_alcance_proximo(TravaAlcance *ev) {
  int r = 0;
  pthread_mutex_lock(&mtx);
  if (nAlcances > 0) {
    *ev = alcances[0];
    memmove(&alcances[0], &alcances[1], sizeof alcances[0] * (size_t)(nAlcances - 1));
    nAlcances--;
    r = 1;
  }
  pthread_mutex_unlock(&mtx);
  return r;
}

// ================================================================ servidor

const char *trava_etag(void) { return etag; }
void trava_definir_etag(const char *e) {
  pthread_mutex_lock(&mtx);
  snprintf(etag, sizeof etag, "%s", e ? e : "");
  gravar();
  pthread_mutex_unlock(&mtx);
}
long long trava_rev(void) { return rev; }
void trava_definir_rev(long long r) {
  pthread_mutex_lock(&mtx);
  if (r != rev) { rev = r; gravar(); }
  pthread_mutex_unlock(&mtx);
}

int trava_aplicar_servidor(const char *json) {
  const char *a;
  Trava novas[TRAVA_MAX];
  int n = 0, i, k;
  if (!json || !js_tem(json, NULL, "travas")) return 0;
  memset(novas, 0, sizeof novas);
  a = js_array(json, NULL, "travas");
  while (a && *a == '{' && n < TRAVA_MAX) {
    const char *f = js_fim(a), *mm;
    Trava *t = &novas[n];
    char tmp[TRAVA_PESSOA_MAX];
    memset(t, 0, sizeof *t);
    if (!f) break;
    js_texto_raiz_em(a, f, "id", t->id, sizeof t->id);
    js_texto_raiz_em(a, f, "serie", tmp, sizeof tmp);
    serieDe(tmp, t->serie, sizeof t->serie);
    js_texto_raiz_em(a, f, "titulo", tmp, sizeof tmp);
    limpo(t->titulo, sizeof t->titulo, tmp);
    { char p[400] = ""; js_texto_raiz_em(a, f, "poster", p, sizeof p); limpo(t->poster, sizeof t->poster, p); }
    js_texto_raiz_em(a, f, "criador", t->criador, sizeof t->criador);
    t->modo = (int)js_num(a, f, "modo", 1) == 0 ? TRAVA_JUNTOS : TRAVA_SEPARADOS;
    t->pin = (int)js_num(a, f, "pin", 0) ? 1 : 0;
    t->pausada = (int)js_num(a, f, "pausada", 0) ? 1 : 0;
    t->criado = (long long)js_num(a, f, "criado", 0);
    t->atualizado = (long long)js_num(a, f, "atualizado", 0);
    mm = js_array(a, f, "membros");
    while (mm && *mm == '{' && t->n < TRAVA_MEMBROS) {
      const char *mf = js_fim(mm);
      TravaMembro *m = &t->m[t->n];
      if (!mf) break;
      memset(m, 0, sizeof *m);
      js_texto(mm, mf, "pessoa", m->pessoa, sizeof m->pessoa);
      m->estado = (int)js_num(mm, mf, "estado", TRAVA_DENTRO);
      m->t = (int)js_num(mm, mf, "t", 0);
      m->e = (int)js_num(mm, mf, "e", 0);
      m->em = (long long)js_num(mm, mf, "em", 0);
      if (m->pessoa[0]) t->n++;
      mm = js_prox(mf);
    }
    if (t->id[0] && t->serie[0] && t->n > 0) n++;
    a = js_prox(f);
  }
  pthread_mutex_lock(&mtx);
  for (i = 0; i < n; i++) {
    Trava *t = &novas[i];
    int velho = achar(t->id);
    for (k = 0; k < t->n; k++) {
      TravaMembro *m = &t->m[k];
      m->perfil = perfilDaPessoa(m->pessoa);
      // A posicao local MAIS ADIANTADA fica: o passo dela pode estar na fila.
      if (velho >= 0) {
        int j = trava_membro_do_perfil(&travas[velho], m->perfil);
        if (j >= 0 && m->perfil > 0) {
          const TravaMembro *v = &travas[velho].m[j];
          if (trava_ep_cmp(v->t, v->e, m->t, m->e) > 0) { m->t = v->t; m->e = v->e; m->em = v->em; }
          else if (trava_ep_cmp(m->t, m->e, v->t, v->e) > 0) notarAlcance(t, k, v->t, v->e);
        }
      }
    }
  }
  // As locais (ainda nao confirmadas) e as que tem operacao na fila ficam como
  // estao: o servidor ainda nao sabe delas.
  for (i = 0; i < nTravas && n < TRAVA_MAX; i++) {
    int j, temOp = 0, ja = 0;
    for (j = 0; j < n; j++) if (!strcmp(novas[j].id, travas[i].id)) ja = 1;
    if (ja) continue;
    for (j = 0; j < nFila; j++) if (!strcmp(fila[j].id, travas[i].id) && strcmp(fila[j].op, "passo")) temOp = 1;
    if (travas[i].local || temOp) novas[n++] = travas[i];
  }
  memcpy(travas, novas, sizeof travas[0] * (size_t)n);
  nTravas = n;
  mudou();
  pthread_mutex_unlock(&mtx);
  return 1;
}

void trava_confirmar(const char *idLocal, const char *idServidor) {
  int i, j;
  if (!idLocal || !idServidor || !idServidor[0]) return;
  pthread_mutex_lock(&mtx);
  i = achar(idLocal);
  if (i >= 0) {
    snprintf(travas[i].id, sizeof travas[i].id, "%s", idServidor);
    travas[i].local = 0;
    for (j = 0; j < nFila; j++)
      if (!strcmp(fila[j].id, idLocal)) snprintf(fila[j].id, sizeof fila[j].id, "%s", idServidor);
    mudou();
  }
  pthread_mutex_unlock(&mtx);
}

int trava_fila_n(void) { return nFila; }
int trava_fila_ver(int i, TravaOp *op) {
  int r = 0;
  pthread_mutex_lock(&mtx);
  if (i >= 0 && i < nFila) { *op = fila[i]; r = 1; }
  pthread_mutex_unlock(&mtx);
  return r;
}
void trava_fila_feita(int n) {
  pthread_mutex_lock(&mtx);
  if (n > nFila) n = nFila;
  if (n > 0) {
    memmove(&fila[0], &fila[n], sizeof fila[0] * (size_t)(nFila - n));
    nFila -= n;
    gravar();
  }
  pthread_mutex_unlock(&mtx);
}
