// O DIARIO. Contrato em diario.h; resumo para as outras frentes em
// docs/plans/social-pessoal/API-diario.md.
//
// MEMORIA: por perfil carregado, um vetor compacto de entradas (Ent, ~48 B),
// ordenado por `quando`, e uma tabela de titulos (Tit) com o id e o agregado,
// achada por hash. 5 mil entradas ~ 240 KB; o teto de 20 mil ~ 1 MB, e so
// perfis CONSULTADOS ficam carregados.
//
// DEPENDENCIAS DE PROPOSITO MINIMAS: dados_* (disco) e perfis_ativo(). Nada de
// SDL, rede ou conta aqui: as fontes moram em diariofontes.c e o cofre em
// diarionuvem.c, para este arquivo ser testado sozinho no Mac.
#include "diario.h"
#include "dados.h"
#include "perfis.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NPERFIS (CONTA_PERFIL_MAX + 1)   // 1..8; o 0 nao e usado
#define EV_MAX  64
#define OUV_MAX 8
#define GRAVAR_FOLGA_S 2

// Bit so de memoria: entrada mudou e ainda nao subiu ao cofre.
#define F_NUVEM 0x8000

typedef struct {
  long long quando;
  int  tit;
  int  seg, cor, dormiuSeg, durSeg;
  short t, e, hora;
  unsigned short flags, tags;
  unsigned char nota, com, origem;
} Ent;

typedef struct {
  char id[DIARIO_ID_MAX];
  char tipo;
  int  tmdb;
  // agregado (recalculado quando `agSujo`)
  int  vezes, episodios, seg;
  long long primeiro, ultimo, notaQuando;
  unsigned char nota;
  unsigned short flags;
} Tit;

typedef struct {
  int carregado;
  Ent *ent; int n, cap;
  Tit *tit; int nTit, capTit;
  int *hash; int capHash;          // indice em tit, -1 vazio
  int agSujo;
  int sujo; long long sujoDesde;
  unsigned geracao;
  int nuvem;                       // DIARIO_NUVEM_*
} Loja;

typedef struct { int perfil, ev, temE; DiarioEntrada e; } Evento;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Loja lojas[NPERFIS];
static Evento fila[EV_MAX];
static int nFila;
static int recarregarPend[NPERFIS];
static struct { DiarioOuvinte fn; void *ctx; } ouv[OUV_MAX];
static int nuvemLida;

long long diario_relogio_teste;     // != 0 congela o relogio (testes)
static long long agora(void) {
  return diario_relogio_teste ? diario_relogio_teste : (long long)time(NULL);
}

static int perfilReal(int p) {
  if (p <= 0) p = perfis_ativo();
  if (p < 1 || p >= NPERFIS) p = 1;
  return p;
}

// ------------------------------------------------------------------ puro

void diario_separar_id(const char *src, char *id, size_t tam, int *t, int *e) {
  const char *c;
  size_t n;
  int a = 0, b = 0;
  if (!src) src = "";
  // "tt123:2:5" ou "kitsu:123:1:4" — o id do titulo e tudo antes dos dois
  // ULTIMOS ":numero" (so se ambos forem numeros).
  c = strrchr(src, ':');
  n = strlen(src);
  if (c && c > src) {
    const char *d = c - 1;
    while (d > src && *d != ':') d--;
    if (*d == ':' && d > src) {
      char *f1, *f2;
      long x = strtol(d + 1, &f1, 10), y = strtol(c + 1, &f2, 10);
      if (f1 == c && *f2 == 0 && f1 != d + 1 && f2 != c + 1) {
        a = (int)x; b = (int)y; n = (size_t)(d - src);
      }
    }
  }
  if (tam) {
    if (n >= tam) n = tam - 1;
    memcpy(id, src, n); id[n] = 0;
  }
  if (t) *t = a;
  if (e) *e = b;
}

int diario_regra_confianca(char o) {
  switch (o) {
    case DIARIO_ORIG_PLAYER: return 5;
    case DIARIO_ORIG_TRAKT: return 4;
    case DIARIO_ORIG_CONTA: return 3;
    case DIARIO_ORIG_LETTERBOXD: return 2;
    case DIARIO_ORIG_MANUAL: return 1;
  }
  return 0;
}

int diario_regra_mesma_vez(const DiarioEntrada *a, const DiarioEntrada *b) {
  long long d;
  if (!a || !b || strcmp(a->id, b->id) || a->temporada != b->temporada ||
      a->episodio != b->episodio) return 0;
  d = a->quando - b->quando;
  if (d < 0) d = -d;
  return d < DIARIO_JANELA_S;
}

int diario_nota_da_reacao(int r) {
  return r == 1 ? 8 : r == 0 ? 5 : r == -1 ? 2 : 0;
}

size_t diario_linha(const DiarioEntrada *e, char *dst, size_t tam) {
  int n = snprintf(dst, tam, "%s\t%c\t%c\t%d\t%d\t%lld\t%d\t%d\t%u\t%u\t%u\t%u\t%d\t%06x\t%d\t%d\n",
                   e->id, e->tipo ? e->tipo : DIARIO_FILME, e->origem ? e->origem : DIARIO_ORIG_MANUAL,
                   e->temporada, e->episodio, e->quando, e->seg, e->hora,
                   (unsigned)e->nota10, (unsigned)e->comPerfis,
                   (unsigned)e->flags, (unsigned)e->tags, e->tmdb,
                   e->cor & 0xffffffu, e->dormiuSeg, e->durSeg);
  return (n > 0 && (size_t)n < tam) ? (size_t)n : 0;
}

int diario_ler_linha(const char *s, DiarioEntrada *e) {
  char id[64], tipo, orig;
  int t, ep, seg, hora, tmdb, dorm, dur;
  unsigned nota, com, fl, tags, cor;
  long long q;
  memset(e, 0, sizeof *e);
  if (!s || *s == '#' || *s == '\n' || !*s) return 0;
  if (sscanf(s, "%63[^\t]\t%c\t%c\t%d\t%d\t%lld\t%d\t%d\t%u\t%u\t%u\t%u\t%d\t%x\t%d\t%d",
             id, &tipo, &orig, &t, &ep, &q, &seg, &hora, &nota, &com, &fl, &tags,
             &tmdb, &cor, &dorm, &dur) != 16) return 0;
  if (!id[0] || strlen(id) >= DIARIO_ID_MAX || q < 0 || nota > 10) return 0;
  if (tipo != DIARIO_FILME && tipo != DIARIO_SERIE) return 0;
  if (!diario_regra_confianca(orig)) return 0;
  snprintf(e->id, sizeof e->id, "%s", id);
  e->tipo = tipo; e->origem = orig;
  e->temporada = (short)t; e->episodio = (short)ep;
  e->quando = q; e->seg = seg < 0 ? 0 : seg;
  e->hora = (short)(hora < -1 || hora > 167 ? -1 : hora);
  e->nota10 = (unsigned char)nota; e->comPerfis = (unsigned char)com;
  e->flags = (unsigned short)fl; e->tags = (unsigned short)tags;
  e->tmdb = tmdb; e->cor = cor & 0xffffffu; e->dormiuSeg = dorm; e->durSeg = dur;
  return 1;
}

static void csvCampo(char **p, char *fim, const char *s) {
  int aspas = s && strpbrk(s, ",\"\n\r") != NULL;
  if (*p >= fim) return;
  if (aspas && *p < fim) *(*p)++ = '"';
  for (; s && *s && *p < fim; s++) {
    if (*s == '"') { *(*p)++ = '"'; if (*p >= fim) break; }
    *(*p)++ = *s;
  }
  if (aspas && *p < fim) *(*p)++ = '"';
}

// Formato do importador do Letterboxd (letterboxd.com/about/importing-data):
// imdbID e tmdbID casam o filme sem depender do titulo.
size_t diario_csv_letterboxd(const DiarioEntrada *e, const char *titulo, int ano,
                             int cabecalho, char *dst, size_t tam) {
  char buf[1024], *p = buf, *fim = buf + sizeof buf - 2;
  char tmp[64], tags[256];
  int i, n;
  static const char *nomeTag[DIARIO_TAG_N] = {
    "visual", "soundtrack", "acting", "screenplay", "ending", "slow pace",
    "cried", "laughed a lot", "scary", "want to rewatch", "overrated", "underrated" };
  if (!dst || !tam) return 0;
  if (cabecalho) {
    n = snprintf(dst, tam, "Title,Year,imdbID,tmdbID,WatchedDate,Rating10,Rewatch,Tags\n");
    if (n < 0 || (size_t)n >= tam) return 0;
    if (!e) return (size_t)n;
    dst += n; tam -= (size_t)n;
  } else n = 0;
  if (!e) return 0;
  csvCampo(&p, fim, titulo ? titulo : ""); *p++ = ',';
  if (ano > 0) { snprintf(tmp, sizeof tmp, "%d", ano); csvCampo(&p, fim, tmp); }
  *p++ = ',';
  if (!strncmp(e->id, "tt", 2)) csvCampo(&p, fim, e->id);
  *p++ = ',';
  if (e->tmdb > 0) { snprintf(tmp, sizeof tmp, "%d", e->tmdb); csvCampo(&p, fim, tmp); }
  *p++ = ',';
  if (e->quando > 0) {
    time_t q = (time_t)e->quando; struct tm tmv;
    localtime_r(&q, &tmv);
    strftime(tmp, sizeof tmp, "%Y-%m-%d", &tmv);
    csvCampo(&p, fim, tmp);
  }
  *p++ = ',';
  if (e->nota10) { snprintf(tmp, sizeof tmp, "%u", (unsigned)e->nota10); csvCampo(&p, fim, tmp); }
  *p++ = ',';
  csvCampo(&p, fim, (e->flags & DIARIO_F_REVER) ? "true" : "false");
  *p++ = ',';
  tags[0] = 0;
  for (i = 0; i < DIARIO_TAG_N; i++) if (e->tags & (1u << i)) {
    size_t l = strlen(tags);
    snprintf(tags + l, sizeof tags - l, "%s%s", l ? ", " : "", nomeTag[i]);
  }
  csvCampo(&p, fim, tags);
  *p++ = '\n'; *p = 0;
  if ((size_t)(p - buf) >= tam) return 0;
  memcpy(dst, buf, (size_t)(p - buf) + 1);
  return (size_t)n + (size_t)(p - buf);
}

// --------------------------------------------------------------- interno

static unsigned hashStr(const char *s) {
  unsigned h = 2166136261u;
  for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
  return h;
}

static void refazerHash(Loja *L) {
  int cap = 64, i;
  while (cap < L->nTit * 2 + 2) cap *= 2;
  if (cap != L->capHash) {
    int *h = realloc(L->hash, sizeof(int) * (size_t)cap);
    if (!h) return;
    L->hash = h; L->capHash = cap;
  }
  for (i = 0; i < L->capHash; i++) L->hash[i] = -1;
  for (i = 0; i < L->nTit; i++) {
    unsigned k = hashStr(L->tit[i].id) & (unsigned)(L->capHash - 1);
    while (L->hash[k] >= 0) k = (k + 1) & (unsigned)(L->capHash - 1);
    L->hash[k] = i;
  }
}

static int acharTit(const Loja *L, const char *id) {
  unsigned k;
  if (!L->capHash || !id) return -1;
  k = hashStr(id) & (unsigned)(L->capHash - 1);
  while (L->hash[k] >= 0) {
    if (!strcmp(L->tit[L->hash[k]].id, id)) return L->hash[k];
    k = (k + 1) & (unsigned)(L->capHash - 1);
  }
  return -1;
}

static int criarTit(Loja *L, const char *id, char tipo) {
  int i = acharTit(L, id);
  if (i >= 0) return i;
  if (L->nTit == L->capTit) {
    int cap = L->capTit ? L->capTit * 2 : 64;
    Tit *t = realloc(L->tit, sizeof(Tit) * (size_t)cap);
    if (!t) return -1;
    L->tit = t; L->capTit = cap;
  }
  i = L->nTit++;
  memset(&L->tit[i], 0, sizeof L->tit[i]);
  snprintf(L->tit[i].id, sizeof L->tit[i].id, "%s", id);
  L->tit[i].tipo = tipo ? tipo : DIARIO_FILME;
  if (L->nTit * 2 + 2 > L->capHash) refazerHash(L);
  else {
    unsigned k = hashStr(id) & (unsigned)(L->capHash - 1);
    while (L->hash[k] >= 0) k = (k + 1) & (unsigned)(L->capHash - 1);
    L->hash[k] = i;
  }
  return i;
}

static void paraPublica(const Loja *L, const Ent *s, DiarioEntrada *d) {
  const Tit *t = &L->tit[s->tit];
  memset(d, 0, sizeof *d);
  memcpy(d->id, t->id, sizeof d->id);
  d->tipo = t->tipo; d->origem = (char)s->origem;
  d->temporada = s->t; d->episodio = s->e;
  d->quando = s->quando; d->seg = s->seg; d->hora = s->hora;
  d->nota10 = s->nota; d->comPerfis = s->com;
  d->flags = (unsigned short)(s->flags & 0x7fff); d->tags = s->tags;
  d->tmdb = t->tmdb; d->cor = (unsigned)s->cor; d->dormiuSeg = s->dormiuSeg; d->durSeg = s->durSeg;
}

static void enfileirar(int perfil, int ev, const Loja *L, const Ent *s) {
  if (nFila >= EV_MAX || !s) { recarregarPend[perfil] = 1; return; }
  fila[nFila].perfil = perfil; fila[nFila].ev = ev; fila[nFila].temE = 1;
  paraPublica(L, s, &fila[nFila].e);
  nFila++;
}

static void mudou(Loja *L) {
  L->geracao++;
  L->agSujo = 1;
  if (!L->sujo) { L->sujo = 1; L->sujoDesde = agora(); }
}

static int cmpEnt(const void *a, const void *b) {
  const Ent *x = a, *y = b;
  return x->quando < y->quando ? -1 : x->quando > y->quando;
}

// Recalcula os agregados de todos os titulos. O(n). Entrada PARCIAL (sono antes
// do fim) nao e uma vez vista: fica no diario, mas nao conta.
static void recalcular(Loja *L) {
  int i;
  if (!L->agSujo) return;
  for (i = 0; i < L->nTit; i++) {
    Tit *t = &L->tit[i];
    t->vezes = t->episodios = t->seg = 0; t->primeiro = t->ultimo = t->notaQuando = 0;
    t->nota = 0; t->flags = 0;
  }
  for (i = 0; i < L->n; i++) {
    Ent *e = &L->ent[i];
    Tit *t = &L->tit[e->tit];
    t->seg += e->seg;
    t->flags |= (unsigned short)(e->flags & 0x7fff);
    if (e->nota && e->quando >= t->notaQuando) { t->nota = e->nota; t->notaQuando = e->quando; }
    if (e->flags & DIARIO_F_PARCIAL) continue;
    t->vezes++;
    if (!(e->flags & DIARIO_F_REVER) && (e->t || e->e)) t->episodios++;
    if (!t->primeiro || e->quando < t->primeiro) t->primeiro = e->quando;
    if (e->quando > t->ultimo) t->ultimo = e->quando;
  }
  L->agSujo = 0;
}

static void reverGrupo(Loja *L, int de, char *feito) {
  int j, primeira = 1;
  const Ent *a = &L->ent[de];
  int tit = a->tit, t = a->t, e = a->e;
  for (j = de; j < L->n; j++) {
    Ent *x = &L->ent[j];
    unsigned short antes;
    if (x->tit != tit || x->t != t || x->e != e) continue;
    if (feito) feito[j] = 1;
    antes = x->flags;
    if (x->flags & DIARIO_F_PARCIAL) x->flags &= (unsigned short)~DIARIO_F_REVER;
    else if (primeira) { x->flags &= (unsigned short)~DIARIO_F_REVER; primeira = 0; }
    else x->flags |= DIARIO_F_REVER;
    if (x->flags != antes) x->flags |= F_NUVEM;
  }
}

// REVER: dentro de cada (tit,t,e), so a primeira vez completa nao e rever.
static void recalcularRever(Loja *L, int tit, int t, int e) {
  int i;
  for (i = 0; i < L->n; i++)
    if (L->ent[i].tit == tit && L->ent[i].t == t && L->ent[i].e == e) { reverGrupo(L, i, NULL); return; }
}

static const Loja *ordLoja;   // so durante o qsort, com a trava
static int cmpGrupo(const void *a, const void *b) {
  const Ent *x = &ordLoja->ent[*(const int *)a], *y = &ordLoja->ent[*(const int *)b];
  if (x->tit != y->tit) return x->tit < y->tit ? -1 : 1;
  if (x->t != y->t) return x->t < y->t ? -1 : 1;
  if (x->e != y->e) return x->e < y->e ? -1 : 1;
  if (x->quando != y->quando) return x->quando < y->quando ? -1 : 1;
  return *(const int *)a - *(const int *)b;
}

// O mesmo para todos os grupos, O(n log n): usado depois de um lote.
static void recalcularReverTudo(Loja *L) {
  int i, *ord = malloc(sizeof(int) * ((size_t)L->n + 1)), primeira = 1;
  if (!ord) return;
  for (i = 0; i < L->n; i++) ord[i] = i;
  ordLoja = L;
  qsort(ord, (size_t)L->n, sizeof(int), cmpGrupo);
  for (i = 0; i < L->n; i++) {
    Ent *x = &L->ent[ord[i]];
    unsigned short antes = x->flags;
    if (i > 0) {
      const Ent *a = &L->ent[ord[i - 1]];
      if (a->tit != x->tit || a->t != x->t || a->e != x->e) primeira = 1;
    }
    if (x->flags & DIARIO_F_PARCIAL) x->flags &= (unsigned short)~DIARIO_F_REVER;
    else if (primeira) { x->flags &= (unsigned short)~DIARIO_F_REVER; primeira = 0; }
    else x->flags |= DIARIO_F_REVER;
    if (x->flags != antes) x->flags |= F_NUVEM;
  }
  free(ord);
}

static const char *nomeArquivo(int p, char *buf, size_t tam) {
  snprintf(buf, tam, "diario-p%d.txt", p);
  return buf;
}

static void lerNuvem(void) {
  char *b, *s;
  if (nuvemLida) return;
  nuvemLida = 1;
  b = dados_ler("diario-nuvem.txt");
  if (!b) return;
  for (s = b; s && *s;) {
    int p, v;
    if (sscanf(s, "p%d=%d", &p, &v) == 2 && p >= 1 && p < NPERFIS && v >= 0 && v <= 2)
      lojas[p].nuvem = v;
    s = strchr(s, '\n'); if (s) s++;
  }
  free(b);
}

static Loja *carregar(int p) {
  Loja *L = &lojas[p];
  char nome[32], *b, *s;
  lerNuvem();
  if (L->carregado) return L;
  L->carregado = 1;
  if (!L->geracao) L->geracao = 1;
  refazerHash(L);
  b = dados_ler(nomeArquivo(p, nome, sizeof nome));
  if (!b) return L;
  for (s = b; s && *s;) {
    char *nl = strchr(s, '\n');
    DiarioEntrada d;
    if (nl) *nl = 0;
    if (diario_ler_linha(s, &d)) {
      Ent x; int ti;
      memset(&x, 0, sizeof x);
      ti = criarTit(L, d.id, d.tipo);
      if (ti >= 0) {
        if (d.tmdb && !L->tit[ti].tmdb) L->tit[ti].tmdb = d.tmdb;
        x.tit = ti; x.quando = d.quando; x.seg = d.seg; x.cor = (int)d.cor;
        x.dormiuSeg = d.dormiuSeg; x.durSeg = d.durSeg;
        x.t = d.temporada; x.e = d.episodio; x.hora = d.hora;
        x.flags = d.flags; x.tags = d.tags; x.nota = d.nota10; x.com = d.comPerfis;
        x.origem = (unsigned char)d.origem;
        if (L->n == L->cap) {
          int cap = L->cap ? L->cap * 2 : 256;
          Ent *v = realloc(L->ent, sizeof(Ent) * (size_t)cap);
          if (!v) break;
          L->ent = v; L->cap = cap;
        }
        L->ent[L->n++] = x;
      }
    }
    s = nl ? nl + 1 : NULL;
  }
  free(b);
  // O arquivo e gravado em ordem, mas uma edicao a mao (ou versao futura) nao
  // pode quebrar o "vetor por data" de que as consultas dependem.
  qsort(L->ent, (size_t)L->n, sizeof(Ent), cmpEnt);
  if (L->n > DIARIO_MAX) {
    memmove(L->ent, L->ent + (L->n - DIARIO_MAX), sizeof(Ent) * DIARIO_MAX);
    L->n = DIARIO_MAX;
  }
  L->agSujo = 1;
  return L;
}

static int acharEnt(Loja *L, const char *id, int t, int e, long long quando) {
  int ti = acharTit(L, id), i, melhor = -1;
  if (ti < 0) return -1;
  for (i = L->n - 1; i >= 0; i--) {
    Ent *x = &L->ent[i];
    if (x->tit != ti || x->t != t || x->e != e) continue;
    if (!quando) return i;               // a mais recente (vetor por data)
    if (x->quando == quando) return i;
    if (melhor < 0) {
      long long d = x->quando - quando; if (d < 0) d = -d;
      if (d < 60) melhor = i;            // tolera arredondamento de segundos
    }
  }
  return melhor;
}

// Insere mantendo a ordem por data. Devolve o indice.
static int inserirEnt(Loja *L, const Ent *x) {
  int i;
  if (L->n == L->cap) {
    int cap = L->cap ? L->cap * 2 : 256;
    Ent *v;
    if (cap > DIARIO_MAX + 1) cap = DIARIO_MAX + 1;
    if (cap <= L->n) return -1;
    v = realloc(L->ent, sizeof(Ent) * (size_t)cap);
    if (!v) return -1;
    L->ent = v; L->cap = cap;
  }
  i = L->n;
  while (i > 0 && L->ent[i - 1].quando > x->quando) i--;
  memmove(&L->ent[i + 1], &L->ent[i], sizeof(Ent) * (size_t)(L->n - i));
  L->ent[i] = *x;
  L->n++;
  return i;
}

static void aplicarTeto(Loja *L) {
  if (L->n > DIARIO_MAX) {
    int sobra = L->n - DIARIO_MAX;
    memmove(L->ent, L->ent + sobra, sizeof(Ent) * (size_t)DIARIO_MAX);
    L->n = DIARIO_MAX;
  }
}

// Funde `x` em `y` (y ja existe). Devolve 1 se algo mudou em y.
static int fundir(Ent *y, const Ent *x) {
  Ent antes = *y;
  int novoManda = diario_regra_confianca((char)x->origem) > diario_regra_confianca((char)y->origem);
  if (novoManda) {
    y->origem = x->origem;
    if (x->quando) y->quando = x->quando;
    if (x->seg) y->seg = x->seg;
    if (x->hora >= 0) y->hora = x->hora;
    if (x->cor) y->cor = x->cor;
    if (x->durSeg) y->durSeg = x->durSeg;
    // Fonte com hora de verdade tira a "data aproximada".
    if (!(x->flags & DIARIO_F_DATA_APROX)) y->flags &= (unsigned short)~DIARIO_F_DATA_APROX;
  } else {
    if (!y->seg) y->seg = x->seg;
    if (y->hora < 0) y->hora = x->hora;
    if (!y->cor) y->cor = x->cor;
    if (!y->durSeg) y->durSeg = x->durSeg;
  }
  if (!y->nota) y->nota = x->nota;
  if (!y->dormiuSeg) y->dormiuSeg = x->dormiuSeg;
  y->com |= x->com;
  y->tags |= x->tags;
  y->flags |= (unsigned short)(x->flags & ~(DIARIO_F_REVER | DIARIO_F_DATA_APROX | DIARIO_F_PARCIAL));
  // PARCIAL so sobrevive se as duas forem parciais (uma vez completa ganha).
  if (!(x->flags & DIARIO_F_PARCIAL)) y->flags &= (unsigned short)~DIARIO_F_PARCIAL;
  if (memcmp(&antes, y, sizeof antes)) { y->flags |= F_NUVEM; return 1; }
  return 0;
}

static int deEntrada(Loja *L, const DiarioEntrada *e, Ent *x) {
  char id[DIARIO_ID_MAX];
  int t = 0, ep = 0, ti;
  char tipo;
  diario_separar_id(e->id, id, sizeof id, &t, &ep);
  if (!id[0]) return -1;
  if (e->temporada || e->episodio) { t = e->temporada; ep = e->episodio; }
  tipo = e->tipo ? e->tipo : ((t || ep) ? DIARIO_SERIE : DIARIO_FILME);
  ti = criarTit(L, id, tipo);
  if (ti < 0) return -1;
  if (tipo == DIARIO_SERIE) L->tit[ti].tipo = DIARIO_SERIE;
  if (e->tmdb > 0 && !L->tit[ti].tmdb) L->tit[ti].tmdb = e->tmdb;
  memset(x, 0, sizeof *x);
  x->tit = ti; x->t = (short)t; x->e = (short)ep;
  x->quando = e->quando > 0 ? e->quando : agora();
  x->seg = e->seg > 0 ? e->seg : 0;
  x->hora = (e->hora >= 0 && e->hora <= 167) ? e->hora : -1;
  x->nota = e->nota10 <= 10 ? e->nota10 : 10;
  x->com = e->comPerfis;
  x->flags = (unsigned short)((e->flags & 0x7fff & ~DIARIO_F_REVER) | F_NUVEM);
  x->tags = e->tags;
  x->cor = (int)(e->cor & 0xffffffu);
  x->dormiuSeg = e->dormiuSeg; x->durSeg = e->durSeg;
  x->origem = (unsigned char)(diario_regra_confianca(e->origem) ? e->origem : DIARIO_ORIG_MANUAL);
  return 0;
}

// Sem trava: o chamador ja segura. Devolve 1 nova, 0 fundida, -1 erro.
static int adicionarSemTrava(int p, Loja *L, const DiarioEntrada *e, int avisar, int lote) {
  Ent x;
  int i, lo, hi;
  if (deEntrada(L, e, &x) < 0) return -1;
  // So a janela de +-6 h importa: busca binaria ate o comeco dela.
  lo = 0; hi = L->n;
  while (lo < hi) {
    int m = (lo + hi) / 2;
    if (L->ent[m].quando <= x.quando - DIARIO_JANELA_S) lo = m + 1; else hi = m;
  }
  for (i = lo; i < L->n && L->ent[i].quando < x.quando + DIARIO_JANELA_S; i++) {
    Ent *y = &L->ent[i];
    if (y->tit != x.tit || y->t != x.t || y->e != x.e) continue;
    {
      if (fundir(y, &x)) {
        Ent copia = *y;
        // A data pode ter mudado: reposiciona.
        memmove(&L->ent[i], &L->ent[i + 1], sizeof(Ent) * (size_t)(L->n - i - 1));
        L->n--;
        inserirEnt(L, &copia);
        if (!lote) recalcularRever(L, copia.tit, copia.t, copia.e);
        mudou(L);
        if (avisar) {
          int k = acharEnt(L, L->tit[copia.tit].id, copia.t, copia.e, copia.quando);
          if (k >= 0) enfileirar(p, DIARIO_EV_ALTERADA, L, &L->ent[k]);
        }
      }
      return 0;
    }
  }
  if (inserirEnt(L, &x) < 0) return -1;
  if (!lote) recalcularRever(L, x.tit, x.t, x.e);
  aplicarTeto(L);
  mudou(L);
  if (avisar) {
    int k = acharEnt(L, L->tit[x.tit].id, x.t, x.e, x.quando);
    if (k >= 0) enfileirar(p, DIARIO_EV_NOVA, L, &L->ent[k]);
  }
  return 1;
}

// ------------------------------------------------------------------ API

void diario_iniciar(void) {
  pthread_mutex_lock(&trava);
  lerNuvem();
  pthread_mutex_unlock(&trava);
}

int diario_adicionar(int perfil, const DiarioEntrada *e) {
  int p = perfilReal(perfil), r;
  if (!e) return -1;
  pthread_mutex_lock(&trava);
  r = adicionarSemTrava(p, carregar(p), e, 1, 0);
  pthread_mutex_unlock(&trava);
  return r;
}

int diario_adicionar_lote(int perfil, const DiarioEntrada *v, int n) {
  int p = perfilReal(perfil), i, novas = 0;
  unsigned g;
  Loja *L;
  if (!v || n <= 0) return 0;
  pthread_mutex_lock(&trava);
  L = carregar(p);
  g = L->geracao;
  for (i = 0; i < n; i++) if (adicionarSemTrava(p, L, &v[i], 0, 1) > 0) novas++;
  if (L->geracao != g) { recalcularReverTudo(L); recarregarPend[p] = 1; }
  pthread_mutex_unlock(&trava);
  return novas;
}

typedef enum { M_NOTA, M_FLAGS, M_TAGS, M_DATA, M_APAGAR } Mexida;

static int mexer(int perfil, const char *idEnt, int t, int e, long long quando,
                 Mexida m, unsigned a, unsigned b, long long novo) {
  int p = perfilReal(perfil), i, ok = 0;
  char id[DIARIO_ID_MAX];
  int t2 = 0, e2 = 0;
  Loja *L;
  diario_separar_id(idEnt, id, sizeof id, &t2, &e2);
  if (!t && !e) { t = t2; e = e2; }
  pthread_mutex_lock(&trava);
  L = carregar(p);
  i = acharEnt(L, id, t, e, quando);
  if (i >= 0) {
    Ent *x = &L->ent[i], antes = *x;
    switch (m) {
      case M_NOTA: x->nota = (unsigned char)(a > 10 ? 10 : a); break;
      case M_FLAGS: {
        unsigned liga = a & 0x7fffu & ~(unsigned)DIARIO_F_REVER;
        unsigned desl = b & 0x7fffu & ~(unsigned)DIARIO_F_REVER;
        x->flags = (unsigned short)((x->flags | liga) & ~desl);
        if (!(x->flags & (DIARIO_F_DORMIU | DIARIO_F_DORMIU_SUS))) x->dormiuSeg = 0;
        if ((liga | desl) & DIARIO_F_PARCIAL) { recalcularRever(L, x->tit, x->t, x->e); x = &L->ent[i]; }
        break;
      }
      case M_TAGS: x->tags = (unsigned short)a; break;
      case M_DATA: {
        Ent c = *x;
        int k;
        if (novo <= 0) break;
        c.quando = novo;
        c.flags &= (unsigned short)~DIARIO_F_DATA_APROX;
        memmove(&L->ent[i], &L->ent[i + 1], sizeof(Ent) * (size_t)(L->n - i - 1));
        L->n--;
        inserirEnt(L, &c);
        recalcularRever(L, c.tit, c.t, c.e);
        k = acharEnt(L, L->tit[c.tit].id, c.t, c.e, c.quando);
        x = k >= 0 ? &L->ent[k] : NULL;
        if (x) { x->flags |= F_NUVEM; mudou(L); enfileirar(p, DIARIO_EV_ALTERADA, L, x); }
        ok = 1; x = NULL;
        break;
      }
      case M_APAGAR: {
        Ent c = *x;
        enfileirar(p, DIARIO_EV_APAGADA, L, &c);
        memmove(&L->ent[i], &L->ent[i + 1], sizeof(Ent) * (size_t)(L->n - i - 1));
        L->n--;
        recalcularRever(L, c.tit, c.t, c.e);
        mudou(L);
        x = NULL; ok = 1;
        break;
      }
    }
    if (x && memcmp(&antes, x, sizeof antes)) {
      x->flags |= F_NUVEM;
      mudou(L);
      enfileirar(p, DIARIO_EV_ALTERADA, L, x);
      ok = 1;
    } else if (x) ok = 1;   // ja estava assim
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int diario_definir_nota(int perfil, const char *id, int t, int e, long long q, int nota10) {
  if (nota10 < 0 || nota10 > 10) return 0;
  return mexer(perfil, id, t, e, q, M_NOTA, (unsigned)nota10, 0, 0);
}
int diario_definir_flags(int perfil, const char *id, int t, int e, long long q,
                         unsigned ligar, unsigned desligar) {
  return mexer(perfil, id, t, e, q, M_FLAGS, ligar, desligar, 0);
}
int diario_definir_tags(int perfil, const char *id, int t, int e, long long q, unsigned tags) {
  return mexer(perfil, id, t, e, q, M_TAGS, tags & ((1u << DIARIO_TAG_N) - 1), 0, 0);
}
int diario_definir_data(int perfil, const char *id, int t, int e, long long q, long long novo) {
  return mexer(perfil, id, t, e, q, M_DATA, 0, 0, novo);
}
int diario_apagar(int perfil, const char *id, int t, int e, long long q) {
  return mexer(perfil, id, t, e, q, M_APAGAR, 0, 0, 0);
}

int diario_avaliar(int perfil, const char *idEnt, char tipo, int nota10) {
  int p = perfilReal(perfil), ti, i, alvo = -1, r = 0;
  char id[DIARIO_ID_MAX];
  Loja *L;
  if (nota10 < 0 || nota10 > 10) return 0;
  diario_separar_id(idEnt, id, sizeof id, NULL, NULL);
  if (!id[0]) return 0;
  pthread_mutex_lock(&trava);
  L = carregar(p);
  ti = acharTit(L, id);
  if (ti >= 0) {
    // Preferencia: a vez de nivel de titulo mais recente; senao a mais recente
    // de qualquer episodio (nota de serie mora na ultima vez vista dela).
    for (i = L->n - 1; i >= 0; i--) if (L->ent[i].tit == ti && !L->ent[i].t && !L->ent[i].e) { alvo = i; break; }
    if (alvo < 0) for (i = L->n - 1; i >= 0; i--) if (L->ent[i].tit == ti) { alvo = i; break; }
  }
  if (alvo >= 0) {
    Ent *x = &L->ent[alvo];
    if (x->nota != nota10) {
      x->nota = (unsigned char)nota10; x->flags |= F_NUVEM;
      mudou(L);
      enfileirar(p, DIARIO_EV_ALTERADA, L, x);
    }
    r = 1;
  } else if (nota10 > 0) {
    DiarioEntrada d;
    memset(&d, 0, sizeof d);
    snprintf(d.id, sizeof d.id, "%s", id);
    d.tipo = tipo ? tipo : DIARIO_FILME;
    d.origem = DIARIO_ORIG_MANUAL;
    d.quando = agora(); d.hora = -1;
    d.nota10 = (unsigned char)nota10;
    d.flags = DIARIO_F_DATA_APROX;
    r = adicionarSemTrava(p, L, &d, 1, 0) >= 0;
  }
  pthread_mutex_unlock(&trava);
  return r;
}

int diario_n(int perfil) {
  int p = perfilReal(perfil), n;
  pthread_mutex_lock(&trava); n = carregar(p)->n; pthread_mutex_unlock(&trava);
  return n;
}
int diario_n_titulos(int perfil) {
  int p = perfilReal(perfil), n, i;
  Loja *L;
  pthread_mutex_lock(&trava);
  L = carregar(p); recalcular(L);
  // Titulo sem vez completa (apagado, ou so sono parcial) nao conta.
  for (n = 0, i = 0; i < L->nTit; i++) if (L->tit[i].vezes) n++;
  pthread_mutex_unlock(&trava);
  return n;
}

int diario_entrada(int perfil, int i, DiarioEntrada *s) {
  int p = perfilReal(perfil), ok = 0;
  Loja *L;
  pthread_mutex_lock(&trava);
  L = carregar(p);
  if (i >= 0 && i < L->n) { if (s) paraPublica(L, &L->ent[i], s); ok = 1; }
  pthread_mutex_unlock(&trava);
  return ok;
}

static void titPublico(const Tit *t, DiarioTitulo *s) {
  memset(s, 0, sizeof *s);
  memcpy(s->id, t->id, sizeof s->id);
  s->tipo = t->tipo; s->vezes = t->vezes; s->episodios = t->episodios;
  s->primeiro = t->primeiro; s->ultimo = t->ultimo; s->nota10 = t->nota;
  s->flags = t->flags; s->seg = t->seg; s->tmdb = t->tmdb;
}

int diario_titulo(int perfil, const char *idEnt, DiarioTitulo *s) {
  int p = perfilReal(perfil), ti, ok = 0;
  char id[DIARIO_ID_MAX];
  Loja *L;
  diario_separar_id(idEnt, id, sizeof id, NULL, NULL);
  pthread_mutex_lock(&trava);
  L = carregar(p); recalcular(L);
  ti = acharTit(L, id);
  if (ti >= 0 && L->tit[ti].vezes) { if (s) titPublico(&L->tit[ti], s); ok = 1; }
  else if (s) {
    if (ti >= 0) titPublico(&L->tit[ti], s);   // nota de quem so dormiu, p.ex.
    else { memset(s, 0, sizeof *s); snprintf(s->id, sizeof s->id, "%s", id); }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int diario_titulo_i(int perfil, int i, DiarioTitulo *s) {
  int p = perfilReal(perfil), k, j, ok = 0;
  Loja *L;
  pthread_mutex_lock(&trava);
  L = carregar(p); recalcular(L);
  for (k = 0, j = 0; j < L->nTit; j++) {
    if (!L->tit[j].vezes) continue;
    if (k++ == i) { if (s) titPublico(&L->tit[j], s); ok = 1; break; }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int diario_episodio(int perfil, const char *idEnt, int t, int e, long long *quando) {
  int p = perfilReal(perfil), ti, i, n = 0;
  char id[DIARIO_ID_MAX];
  Loja *L;
  diario_separar_id(idEnt, id, sizeof id, NULL, NULL);
  if (quando) *quando = 0;
  pthread_mutex_lock(&trava);
  L = carregar(p);
  ti = acharTit(L, id);
  if (ti >= 0) for (i = 0; i < L->n; i++) {
    Ent *x = &L->ent[i];
    if (x->tit != ti || x->t != t || x->e != e || (x->flags & DIARIO_F_PARCIAL)) continue;
    n++;
    if (quando) *quando = x->quando;
  }
  pthread_mutex_unlock(&trava);
  return n;
}

int diario_ultimo_episodio(int perfil, const char *idEnt, int *t, int *e, long long *quando) {
  int p = perfilReal(perfil), ti, i, ok = 0, bt = 0, be = 0;
  long long bq = 0;
  char id[DIARIO_ID_MAX];
  Loja *L;
  diario_separar_id(idEnt, id, sizeof id, NULL, NULL);
  pthread_mutex_lock(&trava);
  L = carregar(p);
  ti = acharTit(L, id);
  if (ti >= 0) for (i = 0; i < L->n; i++) {
    Ent *x = &L->ent[i];
    if (x->tit != ti || (!x->t && !x->e) || (x->flags & DIARIO_F_PARCIAL)) continue;
    if (!ok || x->t > bt || (x->t == bt && x->e > be) ||
        (x->t == bt && x->e == be && x->quando > bq)) {
      bt = x->t; be = x->e; bq = x->quando; ok = 1;
    }
  }
  pthread_mutex_unlock(&trava);
  if (t) *t = bt;
  if (e) *e = be;
  if (quando) *quando = bq;
  return ok;
}

typedef struct { DiarioEntrada *v; int max, n; } Junta;
static int juntar(const DiarioEntrada *e, void *c) {
  Junta *j = c;
  if (j->v && j->n < j->max) j->v[j->n] = *e;
  j->n++;
  return 1;
}

int diario_percorrer(int perfil, const DiarioFiltro *f, DiarioVisita cb, void *ctx) {
  int p = perfilReal(perfil), i, n = 0, ti = -2, ini, fim, passo;
  char id[DIARIO_ID_MAX];
  Loja *L;
  DiarioFiltro z;
  if (!f) { memset(&z, 0, sizeof z); f = &z; }
  pthread_mutex_lock(&trava);
  L = carregar(p);
  if (f->id) {
    diario_separar_id(f->id, id, sizeof id, NULL, NULL);
    ti = acharTit(L, id);
    if (ti < 0) { pthread_mutex_unlock(&trava); return 0; }
  }
  if (f->decrescente) { ini = L->n - 1; fim = -1; passo = -1; }
  else { ini = 0; fim = L->n; passo = 1; }
  for (i = ini; i != fim; i += passo) {
    Ent *x = &L->ent[i];
    DiarioEntrada d;
    if (ti >= 0 && x->tit != ti) continue;
    if (f->tipo && L->tit[x->tit].tipo != f->tipo) continue;
    if (f->de && x->quando < f->de) continue;
    if (f->ate && x->quando >= f->ate) continue;
    if ((x->flags & f->comFlags) != f->comFlags) continue;
    if (x->flags & f->semFlags) continue;
    if (f->origem && x->origem != (unsigned char)f->origem) continue;
    n++;
    if (cb) {
      paraPublica(L, x, &d);
      if (!cb(&d, ctx)) break;
    }
  }
  pthread_mutex_unlock(&trava);
  return n;
}

int diario_por_titulo(int perfil, const char *id, DiarioEntrada *s, int max) {
  Junta j = { s, max, 0 };
  DiarioFiltro f;
  memset(&f, 0, sizeof f);
  f.id = id;
  diario_percorrer(perfil, &f, juntar, &j);
  return j.n;
}

int diario_por_periodo(int perfil, long long de, long long ate, DiarioEntrada *s, int max) {
  Junta j = { s, max, 0 };
  DiarioFiltro f;
  memset(&f, 0, sizeof f);
  f.de = de; f.ate = ate;
  diario_percorrer(perfil, &f, juntar, &j);
  return j.n;
}

int diario_por_lista(int perfil, const char *const *ids, int n, DiarioTitulo *s) {
  int p = perfilReal(perfil), i, vistos = 0;
  Loja *L;
  if (!ids || n <= 0) return 0;
  pthread_mutex_lock(&trava);
  L = carregar(p); recalcular(L);
  for (i = 0; i < n; i++) {
    char id[DIARIO_ID_MAX];
    int ti;
    diario_separar_id(ids[i], id, sizeof id, NULL, NULL);
    ti = acharTit(L, id);
    if (ti >= 0 && L->tit[ti].vezes) {
      vistos++;
      if (s) titPublico(&L->tit[ti], &s[i]);
    } else if (s) { memset(&s[i], 0, sizeof s[i]); snprintf(s[i].id, sizeof s[i].id, "%s", id); }
  }
  pthread_mutex_unlock(&trava);
  return vistos;
}

unsigned diario_geracao(int perfil) {
  int p = perfilReal(perfil);
  unsigned g;
  pthread_mutex_lock(&trava); g = lojas[p].geracao; pthread_mutex_unlock(&trava);
  return g;
}

int diario_ouvir(DiarioOuvinte fn, void *ctx) {
  int i, ok = 0;
  if (!fn) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < OUV_MAX; i++) if (ouv[i].fn == fn && ouv[i].ctx == ctx) { ok = 1; break; }
  if (!ok) for (i = 0; i < OUV_MAX; i++) if (!ouv[i].fn) { ouv[i].fn = fn; ouv[i].ctx = ctx; ok = 1; break; }
  pthread_mutex_unlock(&trava);
  return ok;
}

void diario_parar_de_ouvir(DiarioOuvinte fn, void *ctx) {
  int i;
  pthread_mutex_lock(&trava);
  for (i = 0; i < OUV_MAX; i++) if (ouv[i].fn == fn && ouv[i].ctx == ctx) ouv[i].fn = NULL;
  pthread_mutex_unlock(&trava);
}

// Monta o arquivo inteiro. Chamado com a trava.
static char *montarArquivo(Loja *L) {
  size_t cap = 64 + (size_t)L->n * 96, k;
  char *b = malloc(cap);
  int i;
  if (!b) return NULL;
  k = (size_t)snprintf(b, cap, "# nuvio diario v1\n");
  for (i = 0; i < L->n; i++) {
    DiarioEntrada d;
    paraPublica(L, &L->ent[i], &d);
    d.flags = L->ent[i].flags;           // com a marca de envio pendente
    if (cap - k < 200) {
      char *n = realloc(b, cap * 2);
      if (!n) { free(b); return NULL; }
      b = n; cap *= 2;
    }
    k += diario_linha(&d, b + k, cap - k);
  }
  return b;
}

static void gravarPerfil(int p, int forcar) {
  Loja *L = &lojas[p];
  char *b = NULL, nome[32];
  pthread_mutex_lock(&trava);
  if (L->carregado && L->sujo && (forcar || agora() - L->sujoDesde >= GRAVAR_FOLGA_S)) {
    b = montarArquivo(L);
    if (b) L->sujo = 0;
  }
  pthread_mutex_unlock(&trava);
  if (b) {
    dados_gravar(nomeArquivo(p, nome, sizeof nome), b);
    free(b);
  }
}

void diario_gravar_agora(void) {
  int p;
  for (p = 1; p < NPERFIS; p++) gravarPerfil(p, 1);
}

void diario_passo(void) {
  static Evento local[EV_MAX];   // so o laco principal chama
  int n, p, i, rec[NPERFIS], algum = 0;
  struct { DiarioOuvinte fn; void *ctx; } o[OUV_MAX];
  pthread_mutex_lock(&trava);
  n = nFila;
  if (n) memcpy(local, fila, sizeof(Evento) * (size_t)n);
  nFila = 0;
  for (p = 0; p < NPERFIS; p++) { rec[p] = recarregarPend[p]; algum |= rec[p] | lojas[p].sujo; }
  memset(recarregarPend, 0, sizeof recarregarPend);
  memcpy(o, ouv, sizeof o);
  pthread_mutex_unlock(&trava);
  if (!n && !algum) return;
  for (i = 0; i < n; i++) {
    int k;
    for (k = 0; k < OUV_MAX; k++)
      if (o[k].fn) o[k].fn(local[i].perfil, local[i].ev, local[i].temE ? &local[i].e : NULL, o[k].ctx);
  }
  for (p = 1; p < NPERFIS; p++) if (rec[p]) {
    int k;
    for (k = 0; k < OUV_MAX; k++) if (o[k].fn) o[k].fn(p, DIARIO_EV_RECARREGADO, NULL, o[k].ctx);
  }
  for (p = 1; p < NPERFIS; p++) if (lojas[p].sujo) gravarPerfil(p, 0);
}

void diario_esquecer(void) {
  int p;
  diario_gravar_agora();
  pthread_mutex_lock(&trava);
  for (p = 0; p < NPERFIS; p++) {
    unsigned g = lojas[p].geracao;
    free(lojas[p].ent); free(lojas[p].tit); free(lojas[p].hash);
    memset(&lojas[p], 0, sizeof lojas[p]);
    lojas[p].geracao = g + 1;   // quem guardou a geracao ve que mudou
  }
  nFila = 0;
  nuvemLida = 0;
  pthread_mutex_unlock(&trava);
}

// ---------------------------------------------------------------- cofre

int diario_nuvem_estado(int perfil) {
  int p = perfilReal(perfil), v;
  pthread_mutex_lock(&trava); lerNuvem(); v = lojas[p].nuvem; pthread_mutex_unlock(&trava);
  return v;
}

void diario_nuvem_definir(int perfil, int estado) {
  int p = perfilReal(perfil), i;
  char buf[256];
  size_t k = 0;
  if (estado < 0 || estado > 2) return;
  pthread_mutex_lock(&trava);
  lerNuvem();
  if (estado == DIARIO_NUVEM_SIM && lojas[p].nuvem != DIARIO_NUVEM_SIM) {
    // Ligou agora: o historico inteiro sobe (em lotes, pelo diarionuvem.c).
    Loja *L = carregar(p);
    for (i = 0; i < L->n; i++) L->ent[i].flags |= F_NUVEM;
    if (L->n) mudou(L);
  }
  lojas[p].nuvem = estado;
  buf[0] = 0;
  for (i = 1; i < NPERFIS; i++)
    if (lojas[i].nuvem) k += (size_t)snprintf(buf + k, sizeof buf - k, "p%d=%d\n", i, lojas[i].nuvem);
  pthread_mutex_unlock(&trava);
  dados_gravar("diario-nuvem.txt", buf);
}

int diario_nuvem_pendentes(int perfil) {
  int p = perfilReal(perfil), i, n = 0;
  Loja *L;
  pthread_mutex_lock(&trava);
  L = carregar(p);
  for (i = 0; i < L->n; i++) if (L->ent[i].flags & F_NUVEM) n++;
  pthread_mutex_unlock(&trava);
  return n;
}

// Para diarionuvem.c (fora da API publica): copia ate `max` entradas
// pendentes de envio. diario_nuvem_confirmar tira a marca das que subiram.
int diario_nuvem_lote(int perfil, DiarioEntrada *v, int max) {
  int p = perfilReal(perfil), i, n = 0;
  Loja *L;
  pthread_mutex_lock(&trava);
  L = carregar(p);
  for (i = 0; i < L->n && n < max; i++) if (L->ent[i].flags & F_NUVEM) paraPublica(L, &L->ent[i], &v[n++]);
  pthread_mutex_unlock(&trava);
  return n;
}

void diario_nuvem_confirmar(int perfil, const DiarioEntrada *v, int n) {
  int p = perfilReal(perfil), i;
  Loja *L;
  pthread_mutex_lock(&trava);
  L = carregar(p);
  for (i = 0; i < n; i++) {
    int k = acharEnt(L, v[i].id, v[i].temporada, v[i].episodio, v[i].quando);
    if (k >= 0) {
      DiarioEntrada atual;
      paraPublica(L, &L->ent[k], &atual);
      // So limpa se nao mudou de novo enquanto subia.
      if (!memcmp(&atual, &v[i], sizeof atual)) L->ent[k].flags &= (unsigned short)~F_NUVEM;
    }
  }
  pthread_mutex_unlock(&trava);
}
