// Cacadas ativas, filmes, progresso e aviso de conquista. Ver caca.h.
#include "caca.h"
#include "cacavisto.h"
#include "conquista.h"
#include "catalogo.h"
#include "colecoes.h"
#include "dados.h"
#include "descoberta.h"
#include "idioma.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "js.h"
#include "nuvem.h"
#include "perfis.h"
#include "recomenda.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
#define TMDB "https://api.themoviedb.org/3"
#define TMDB_IMG "https://image.tmdb.org/t/p/w185"
#define TTL_PESSOA   (14LL * 86400LL)   // filmografia: 14 dias (plano 4.4)
#define TTL_LISTA    (7LL * 86400LL)    // top 250, listas e trilhas: 7 dias
#define META_MAX     4096               // tmdb -> imdb/duracao/data/poster
#define FILA_MAX     16
#define RECALC_MS    2000u              // debounce do recalculo (plano 8.6)
#define PRAZO_S      15
#define GRUPO_FILEIRA "cacadas"
#define GRUPO_COLECAO "cacadas_colecao"

// ---------------------------------------------------------------------------
// Estado do fio principal
// ---------------------------------------------------------------------------
typedef struct {
  CacaInfo  i;
  long long criado, completaEm;
  CacaFilme *f;        // a lista INTEIRA (todas as classes), por data
  int        nf;
  int        pedido;   // ha trabalho na fila para esta chave
  char       chave[96];
} Slot;

static Slot slots[CACA_MAX + 1];
static int nSlots, perfilLido = -1;
static unsigned revisao = 1;
static unsigned long long genVisto = ~0ull;
static unsigned recalcEm, ultimoPasso;
static int sujo;                       // pedir recalculo
static ConqAviso aviso;
static int extrasSujos;

// Trilhas (catalogo de "+ Nova trilha").
#define TRILHAS_MAX 40
static CacaTrilha trilhas[TRILHAS_MAX];
static int nTrilhas, trilhasPedidas;

// ---------------------------------------------------------------------------
// Fila do fio de rede
// ---------------------------------------------------------------------------
enum { JOB_LISTA = 1, JOB_TRILHAS, JOB_POSTAR };
typedef struct {
  int  tipo, cacaTipo;
  long alvo;
  char lista[64];
  char chave[96];
  char corpo[256];
} Job;
typedef struct {
  char chave[96];
  CacaFilme *f;
  int nf, estado, resolvendo;
  char nome[96], foto[160];
} Res;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  sinal = PTHREAD_COND_INITIALIZER;
static Job fila[FILA_MAX];
static int nFila, fioVivo;
static Res res[FILA_MAX];
static int nRes;
static CacaTrilha trilhasNovas[TRILHAS_MAX];
static int nTrilhasNovas = -1;         // -1 = nada novo

// Meta (so o fio de rede mexe).
typedef struct { long tmdb; char imdb[16]; short dur; long data; char poster[48]; } Meta;
static Meta *meta;
static int nMeta, metaLida, metaSuja;

static long hojeData(void) {
  time_t t = time(NULL);
  struct tm tm;
  localtime_r(&t, &tm);
  return (long)(tm.tm_year + 1900) * 10000L + (tm.tm_mon + 1) * 100L + tm.tm_mday;
}

void caca_chave(int tipo, long alvo, const char *lista, char *dst, size_t tam) {
  switch (tipo) {
    case CACA_DIRECAO:     snprintf(dst, tam, "dir:%ld", alvo); break;
    case CACA_ATUACAO:     snprintf(dst, tam, "ator:%ld", alvo); break;
    case CACA_SAGA:        snprintf(dst, tam, "saga:%ld", alvo); break;
    case CACA_TOP_TMDB:    snprintf(dst, tam, "top:tmdb"); break;
    case CACA_LISTA_TRAKT: snprintf(dst, tam, "trakt:%s", lista ? lista : ""); break;
    case CACA_TRILHA:      snprintf(dst, tam, "trilha:%s", lista ? lista : ""); break;
    default:               snprintf(dst, tam, "?"); break;
  }
}

// Nome de arquivo seguro a partir da chave.
static void arquivoLista(const char *chave, char *dst, size_t tam) {
  char s[96];
  size_t i;
  snprintf(s, sizeof s, "%s", chave);
  for (i = 0; s[i]; i++)
    if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '-')) s[i] = '_';
  snprintf(dst, tam, "caca-l-%s.txt", s);
}

static void limparCampo(char *s) {
  for (; *s; s++) if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' ';
}

// ---------------------------------------------------------------------------
// Meta (fio de rede)
// ---------------------------------------------------------------------------
static Meta *metaAchar(long tmdb) {
  int i;
  for (i = 0; i < nMeta; i++) if (meta[i].tmdb == tmdb) return &meta[i];
  return NULL;
}

static void metaLer(void) {
  char *t, *l, *s;
  if (metaLida) return;
  metaLida = 1;
  if (!meta) meta = calloc(META_MAX, sizeof *meta);
  if (!meta) return;
  t = dados_ler("caca-meta.txt");
  if (!t) return;
  for (l = strtok_r(t, "\n", &s); l && nMeta < META_MAX; l = strtok_r(NULL, "\n", &s)) {
    Meta m;
    int dur = 0;
    memset(&m, 0, sizeof m);
    if (sscanf(l, "%ld %15s %d %ld %47s", &m.tmdb, m.imdb, &dur, &m.data, m.poster) >= 4 && m.tmdb > 0) {
      if (!strcmp(m.imdb, "-")) m.imdb[0] = 0;
      if (!strcmp(m.poster, "-")) m.poster[0] = 0;
      m.dur = (short)dur;
      meta[nMeta++] = m;
    }
  }
  free(t);
}

static void metaGravar(void) {
  char *b;
  size_t cap, n = 0;
  int i;
  if (!metaSuja || !meta) return;
  cap = (size_t)nMeta * 96 + 16;
  b = malloc(cap);
  if (!b) return;
  b[0] = 0;
  for (i = 0; i < nMeta; i++)
    n += (size_t)snprintf(b + n, cap - n, "%ld %s %d %ld %s\n", meta[i].tmdb,
                          meta[i].imdb[0] ? meta[i].imdb : "-", meta[i].dur, meta[i].data,
                          meta[i].poster[0] ? meta[i].poster : "-");
  dados_gravar("caca-meta.txt", b);
  free(b);
  metaSuja = 0;
}

// /movie/<id>: imdb, duracao, data, poster. 1 quando achou.
static int metaBuscar(long tmdb, Meta *saida) {
  char url[300], cam[64] = "", data[16] = "";
  const char *k = desc_chave_tmdb();
  char *c;
  Meta m;
  if (!k || !k[0] || tmdb <= 0 || !meta) return 0;
  snprintf(url, sizeof url, "%s/movie/%ld?api_key=%s", TMDB, tmdb, k);
  c = rede_baixar(url, PRAZO_S);
  if (!c) return 0;
  memset(&m, 0, sizeof m);
  m.tmdb = tmdb;
  js_texto(c, NULL, "imdb_id", m.imdb, sizeof m.imdb);
  if (strncmp(m.imdb, "tt", 2)) m.imdb[0] = 0;
  m.dur = (short)js_num(c, NULL, "runtime", 0.0);
  js_texto(c, NULL, "release_date", data, sizeof data);
  m.data = caca_data(data);
  if (js_texto(c, NULL, "poster_path", cam, sizeof cam) && cam[0] == '/')
    snprintf(m.poster, sizeof m.poster, "%s", cam);
  free(c);
  if (nMeta < META_MAX) meta[nMeta++] = m;
  else meta[tmdb % META_MAX] = m;
  metaSuja = 1;
  *saida = m;
  return 1;
}

// Completa um filme com a meta. 1 se o filme ficou com imdb.
static int metaAplicar(CacaFilme *f, int buscar) {
  Meta *m = metaAchar(f->tmdb), novo;
  if (!m && buscar && metaBuscar(f->tmdb, &novo)) m = metaAchar(f->tmdb);
  if (!m) return f->imdb[0] != 0;
  if (!f->imdb[0] && m->imdb[0]) snprintf(f->imdb, sizeof f->imdb, "%s", m->imdb);
  if (m->dur > 0 && m->dur < 40 && f->classe == CACA_LONGA) f->classe = CACA_CURTA;
  if (m->data > 0 && (f->data <= 0 || f->data % 10000 == 101)) f->data = m->data;
  if (!f->poster[0] && m->poster[0]) snprintf(f->poster, sizeof f->poster, "%s%s", TMDB_IMG, m->poster);
  return f->imdb[0] != 0;
}

// ---------------------------------------------------------------------------
// Lista em disco: "v1 <baixadoEpoch>\n" e uma linha por filme
// tmdb\timdb\tdata\tclasse\tposter\ttitulo
// ---------------------------------------------------------------------------
static int listaLer(const char *chave, CacaFilme *v, int max, long long *baixado,
                    char *nome, size_t tn, char *foto, size_t tf) {
  char arq[128], *t, *l, *s;
  int n = 0;
  arquivoLista(chave, arq, sizeof arq);
  t = dados_ler(arq);
  *baixado = 0;
  if (!t) return -1;
  l = strtok_r(t, "\n", &s);
  if (!l || sscanf(l, "v1 %lld", baixado) != 1) { free(t); return -1; }
  for (l = strtok_r(NULL, "\n", &s); l; l = strtok_r(NULL, "\n", &s)) {
    char *c[6], *q = l;
    int k;
    if (!strncmp(l, "#nome\t", 6)) { snprintf(nome, tn, "%s", l + 6); continue; }
    if (!strncmp(l, "#foto\t", 6)) { snprintf(foto, tf, "%s", l + 6); continue; }
    for (k = 0; k < 6; k++) { c[k] = q; if (k == 5) break; q = strchr(q, '\t'); if (!q) break; *q++ = 0; }
    if (k < 5 || n >= max) continue;
    memset(&v[n], 0, sizeof v[n]);
    v[n].tmdb = atol(c[0]);
    if (strcmp(c[1], "-")) snprintf(v[n].imdb, sizeof v[n].imdb, "%s", c[1]);
    v[n].data = atol(c[2]);
    v[n].classe = atoi(c[3]);
    if (strcmp(c[4], "-")) snprintf(v[n].poster, sizeof v[n].poster, "%s", c[4]);
    snprintf(v[n].titulo, sizeof v[n].titulo, "%s", c[5]);
    n++;
  }
  free(t);
  return n;
}

static void listaGravar(const char *chave, const CacaFilme *v, int n, const char *nome,
                        const char *foto) {
  char arq[128], *b;
  size_t cap = (size_t)n * 400 + 600, k = 0;
  int i;
  arquivoLista(chave, arq, sizeof arq);
  b = malloc(cap);
  if (!b) return;
  k += (size_t)snprintf(b + k, cap - k, "v1 %lld\n#nome\t%s\n#foto\t%s\n",
                        (long long)time(NULL), nome ? nome : "", foto ? foto : "");
  for (i = 0; i < n && k < cap; i++) {
    char tit[96];
    snprintf(tit, sizeof tit, "%s", v[i].titulo);
    limparCampo(tit);
    k += (size_t)snprintf(b + k, cap - k, "%ld\t%s\t%ld\t%d\t%s\t%s\n", v[i].tmdb,
                          v[i].imdb[0] ? v[i].imdb : "-", v[i].data, v[i].classe,
                          v[i].poster[0] ? v[i].poster : "-", tit);
  }
  dados_gravar(arq, b);
  free(b);
}

// ---------------------------------------------------------------------------
// Rede (fio)
// ---------------------------------------------------------------------------
static char *pedirServico(const char *caminho, int *st) {
  const char *cab[5] = { 0 };
  char aut[600], via[64], pf[64], url[400];
  *st = 0;
  if (!NV_REC_URL[0]) return NULL;
  if (!recomenda_cabecalhos(cab, aut, sizeof aut, via, sizeof via, pf, sizeof pf)) return NULL;
  snprintf(url, sizeof url, "%s%s", NV_REC_URL, caminho);
  return rede_baixar_st(url, PRAZO_S, cab, st);
}

static void publicar(const char *chave, const CacaFilme *v, int n, int estado,
                     const char *nome, const char *foto) {
  Res r;
  int i, resolvendo = 0;
  memset(&r, 0, sizeof r);
  snprintf(r.chave, sizeof r.chave, "%s", chave);
  if (n > 0) {
    r.f = malloc(sizeof *r.f * (size_t)n);
    if (!r.f) return;
    memcpy(r.f, v, sizeof *r.f * (size_t)n);
  }
  for (i = 0; i < n; i++) if (!v[i].imdb[0]) resolvendo++;
  r.nf = n; r.estado = estado; r.resolvendo = resolvendo;
  snprintf(r.nome, sizeof r.nome, "%s", nome ? nome : "");
  snprintf(r.foto, sizeof r.foto, "%s", foto ? foto : "");
  pthread_mutex_lock(&trava);
  if (nRes >= FILA_MAX) { free(res[0].f); memmove(res, res + 1, sizeof res[0] * (FILA_MAX - 1)); nRes--; }
  res[nRes++] = r;
  pthread_mutex_unlock(&trava);
}

static void ordenarData(CacaFilme *v, int n) {
  int i, j;
  for (i = 1; i < n; i++) {
    CacaFilme t = v[i];
    for (j = i; j > 0 && v[j - 1].data > t.data; j--) v[j] = v[j - 1];
    v[j] = t;
  }
}

// Le uma lista de resultados TMDB ("results" ou "parts").
static int lerResultados(const char *c, const char *arr, CacaFilme *v, int n, int max, long hoje) {
  const char *p = js_array(c, NULL, arr);
  while (p && n < max) {
    const char *fim = js_fim(p);
    char data[16] = "", cam[128] = "";
    CacaFilme f;
    int i, dup = 0;
    if (!fim) break;
    memset(&f, 0, sizeof f);
    f.tmdb = (long)js_num(p, fim, "id", 0.0);
    js_texto(p, fim, "title", f.titulo, sizeof f.titulo);
    js_texto(p, fim, "release_date", data, sizeof data);
    f.data = caca_data(data);
    if (js_texto(p, fim, "poster_path", cam, sizeof cam) && cam[0] == '/')
      snprintf(f.poster, sizeof f.poster, "%s%s", TMDB_IMG, cam);
    for (i = 0; i < n; i++) if (v[i].tmdb == f.tmdb) dup = 1;
    if (f.tmdb > 0 && !dup && f.data > 0 && f.data <= hoje) v[n++] = f;
    p = js_prox(fim);
  }
  return n;
}

// Baixa a lista de um alvo. Devolve quantos (ou -1 em falha).
static int baixarLista(const Job *j, CacaFilme *v, int max, char *nome, size_t tn,
                       char *foto, size_t tf) {
  const char *k = desc_chave_tmdb();
  char url[400], *c;
  int n = -1, st;
  long hoje = hojeData();
  // 1. O Worker, para pessoa: ja classificado e com imdb.
  if (j->cacaTipo == CACA_DIRECAO || j->cacaTipo == CACA_ATUACAO) {
    snprintf(url, sizeof url, "/v1/filmografia?pessoa=%ld&papel=%s", j->alvo,
             j->cacaTipo == CACA_DIRECAO ? "dir" : "ator");
    c = pedirServico(url, &st);
    if (c && st == 200) {
      n = caca_ler_filmes(c, v, max);
      if (n > 0) { js_texto(c, NULL, "nome", nome, tn); js_texto(c, NULL, "foto", foto, tf); }
      else n = -1;
    }
    free(c);
    if (n > 0) { ordenarData(v, n); return n; }
    if (!k || !k[0]) return -1;
    snprintf(url, sizeof url, "%s/person/%ld/movie_credits?api_key=%s&language=%s",
             TMDB, j->alvo, k, desc_tmdb_idioma());
    c = rede_baixar(url, PRAZO_S);
    if (!c) return -1;
    n = caca_ler_creditos(c, j->cacaTipo, hoje, v, max);
    free(c);
    return n;
  }
  if (j->cacaTipo == CACA_SAGA) {
    char cam[128] = "";
    if (!k || !k[0]) return -1;
    snprintf(url, sizeof url, "%s/collection/%ld?api_key=%s&language=%s", TMDB, j->alvo, k,
             desc_tmdb_idioma());
    c = rede_baixar(url, PRAZO_S);
    if (!c) return -1;
    // O nome da colecao vem ANTES de "parts"; js_texto acha o primeiro "name".
    js_texto(c, NULL, "name", nome, tn);
    if (js_texto(c, NULL, "poster_path", cam, sizeof cam) && cam[0] == '/')
      snprintf(foto, tf, "%s%s", TMDB_IMG, cam);
    n = lerResultados(c, "parts", v, 0, max, hoje);
    free(c);
    ordenarData(v, n);
    return n;
  }
  if (j->cacaTipo == CACA_TOP_TMDB) {
    int pg;
    if (!k || !k[0]) return -1;
    n = 0;
    for (pg = 1; pg <= 13 && n < 250; pg++) {
      snprintf(url, sizeof url, "%s/movie/top_rated?api_key=%s&language=%s&page=%d", TMDB, k,
               desc_tmdb_idioma(), pg);
      c = rede_baixar(url, PRAZO_S);
      if (!c) break;
      n = lerResultados(c, "results", v, n, max < 250 ? max : 250, 99999999L);
      free(c);
    }
    if (n <= 0) return -1;
    ordenarData(v, n);
    return n;
  }
  if (j->cacaTipo == CACA_LISTA_TRAKT) {
    const char *cli = nuvem_trakt_cliente();
    char ch[200];
    const char *cab[] = { "trakt-api-version: 2", ch, NULL };
    const char *p;
    if (!cli[0] || !j->lista[0]) return -1;
    snprintf(ch, sizeof ch, "trakt-api-key: %s", cli);
    snprintf(url, sizeof url, "https://api.trakt.tv/lists/%s/items/movies", j->lista);
    c = rede_baixar_com(url, PRAZO_S, cab);
    if (!c) return -1;
    n = 0;
    for (p = js_raiz_array(c); p && n < max; ) {
      const char *fim = js_fim(p);
      CacaFilme f;
      if (!fim) break;
      memset(&f, 0, sizeof f);
      js_texto(p, fim, "title", f.titulo, sizeof f.titulo);
      js_texto(p, fim, "imdb", f.imdb, sizeof f.imdb);
      f.tmdb = (long)js_num(p, fim, "tmdb", 0.0);
      f.data = (long)js_num(p, fim, "year", 0.0) * 10000L + 101;
      if (strncmp(f.imdb, "tt", 2)) f.imdb[0] = 0;
      if (f.titulo[0] && (f.tmdb > 0 || f.imdb[0])) v[n++] = f;
      p = js_prox(fim);
    }
    free(c);
    return n;
  }
  if (j->cacaTipo == CACA_TRILHA) {
    char cam[128];
    snprintf(url, sizeof url, "/v1/trilha?id=%s", j->lista);
    c = pedirServico(url, &st);
    if (!c || st != 200) { free(c); return -1; }
    n = caca_ler_filmes(c, v, max);
    js_texto(c, NULL, "nome", nome, tn);
    if (js_texto(c, NULL, "foto", cam, sizeof cam)) snprintf(foto, tf, "%s", cam);
    // "ordem":"historia" mantem a ordem do JSON (data = posicao, para "Em
    // ordem" valer pela historia); senao, por lancamento.
    { char ord[16] = "";
      js_texto(c, NULL, "ordem", ord, sizeof ord);
      if (!strcmp(ord, "historia")) { int i; for (i = 0; i < n; i++) v[i].data = i + 1; }
      else ordenarData(v, n); }
    free(c);
    return n;
  }
  return -1;
}

static void fazerLista(const Job *j) {
  CacaFilme *v = calloc(CACA_FILMES_MAX, sizeof *v);
  char nome[96] = "", foto[160] = "";
  long long baixado = 0, ttl;
  int n, i, falta = 0, feitos = 0, historia;
  if (!v) return;
  metaLer();
  ttl = (j->cacaTipo == CACA_DIRECAO || j->cacaTipo == CACA_ATUACAO) ? TTL_PESSOA : TTL_LISTA;
  n = listaLer(j->chave, v, CACA_FILMES_MAX, &baixado, nome, sizeof nome, foto, sizeof foto);
  if (n > 0) publicar(j->chave, v, n, CACA_PRONTA, nome, foto);
  if (n <= 0 || (long long)time(NULL) - baixado > ttl) {
    int m;
    CacaFilme *novo = calloc(CACA_FILMES_MAX, sizeof *novo);
    char nn[96] = "", ff[160] = "";
    if (!novo) { free(v); return; }
    m = baixarLista(j, novo, CACA_FILMES_MAX, nn, sizeof nn, ff, sizeof ff);
    if (m > 0) {
      // O que ja foi resolvido na lista velha nao volta para a fila.
      for (i = 0; i < m; i++) { metaAplicar(&novo[i], 0); }
      memcpy(v, novo, sizeof *v * (size_t)m);
      n = m;
      if (nn[0]) snprintf(nome, sizeof nome, "%s", nn);
      if (ff[0]) snprintf(foto, sizeof foto, "%s", ff);
      publicar(j->chave, v, n, CACA_PRONTA, nome, foto);
      listaGravar(j->chave, v, n, nome, foto);
    } else if (n <= 0) {
      publicar(j->chave, NULL, 0, CACA_ERRO, "", "");
      free(novo); free(v);
      return;
    }
    free(novo);
  }
  // A FILA LENTA: imdb (e duracao, data, poster) de quem ainda nao tem.
  historia = j->cacaTipo == CACA_TRILHA && n > 0 && v[n - 1].data == n;
  for (i = 0; i < n; i++) {
    long data = v[i].data;
    if (v[i].imdb[0] && v[i].poster[0] && v[i].data > 0) continue;
    if (!metaAplicar(&v[i], 1) && !v[i].imdb[0]) falta++;
    if (historia) v[i].data = data;
    if (++feitos % 20 == 0) publicar(j->chave, v, n, CACA_PRONTA, nome, foto);
    usleep(120000);   // ~8 por segundo: longe do limite do TMDB
  }
  if (feitos) {
    if (!historia) ordenarData(v, n);
    publicar(j->chave, v, n, CACA_PRONTA, nome, foto);
    listaGravar(j->chave, v, n, nome, foto);
    metaGravar();
  }
  (void)falta;
  free(v);
}

// As sagas e listas que o app conhece sem servidor. Ids conferidos no TMDB em
// 06/10/2026 (themoviedb.org/collection/<id>). O nome e o de partida: o TMDB
// devolve o nome no idioma quando a cacada carrega.
static const struct { long id; const char *nome; } SAGAS[] = {
  { 119,   "The Lord of the Rings" }, { 1241,  "Harry Potter" },
  { 10,    "Star Wars" },             { 10194, "Toy Story" },
  { 264,   "Back to the Future" },    { 2344,  "The Matrix" },
  { 230,   "The Godfather" },         { 263,   "The Dark Knight" },
};

static void fazerTrilhas(void) {
  CacaTrilha t[TRILHAS_MAX];
  int n = 0, st, i;
  char *c;
  memset(t, 0, sizeof t);
  t[n].tipo = CACA_TOP_TMDB;
  snprintf(t[n].nome, sizeof t[n].nome, "%s", i18n("250 mais bem avaliados (TMDB)"));
  n++;
  // O servidor manda as trilhas de curadoria (Oscar, as do dono) e a lista do
  // Trakt escolhida para "250 mais bem avaliados".
  c = pedirServico("/v1/trilhas", &st);
  if (c && st == 200) {
    const char *p = js_array(c, NULL, "trilhas");
    const char *idi = desc_tmdb_idioma();
    while (p && n < TRILHAS_MAX) {
      const char *fim = js_fim(p);
      char id[64] = "", tipo[16] = "", chaveNome[24], foto[160] = "";
      if (!fim) break;
      js_texto(p, fim, "id", id, sizeof id);
      js_texto(p, fim, "tipo", tipo, sizeof tipo);
      js_texto(p, fim, "foto", foto, sizeof foto);
      t[n].tipo = !strcmp(tipo, "trakt") ? CACA_LISTA_TRAKT : !strcmp(tipo, "saga") ? CACA_SAGA
                                                                                    : CACA_TRILHA;
      if (t[n].tipo == CACA_LISTA_TRAKT) js_texto(p, fim, "lista", t[n].lista, sizeof t[n].lista);
      else if (t[n].tipo == CACA_SAGA) t[n].alvo = (long)js_num(p, fim, "tmdb", 0.0);
      else snprintf(t[n].lista, sizeof t[n].lista, "%s", id);
      // "nome":{"pt-BR":"...","en":"..."}: o do idioma, senao o ingles.
      { const char *o = strstr(p, "\"nome\"");
        char nomeIdi[96] = "", nomeEn[96] = "";
        if (o && o < fim) {
          const char *of = strchr(o, '{');
          const char *off = of && of < fim ? js_fim(of) : NULL;
          if (off) {
            snprintf(chaveNome, sizeof chaveNome, "%s", idi);
            js_texto(of, off, chaveNome, nomeIdi, sizeof nomeIdi);
            if (!nomeIdi[0] && strlen(idi) > 2) { chaveNome[2] = 0; js_texto(of, off, chaveNome, nomeIdi, sizeof nomeIdi); }
            js_texto(of, off, "en", nomeEn, sizeof nomeEn);
          }
        }
        snprintf(t[n].nome, sizeof t[n].nome, "%s", nomeIdi[0] ? nomeIdi : nomeEn); }
      snprintf(t[n].foto, sizeof t[n].foto, "%s", foto);
      if (t[n].nome[0] && (t[n].lista[0] || t[n].alvo > 0)) n++;
      p = js_prox(fim);
    }
  }
  free(c);
  for (i = 0; i < (int)(sizeof SAGAS / sizeof SAGAS[0]) && n < TRILHAS_MAX; i++) {
    int k, dup = 0;
    for (k = 0; k < n; k++) if (t[k].tipo == CACA_SAGA && t[k].alvo == SAGAS[i].id) dup = 1;
    if (dup) continue;
    t[n].tipo = CACA_SAGA; t[n].alvo = SAGAS[i].id;
    snprintf(t[n].nome, sizeof t[n].nome, "%s", SAGAS[i].nome);
    n++;
  }
  pthread_mutex_lock(&trava);
  memcpy(trilhasNovas, t, sizeof t);
  nTrilhasNovas = n;
  pthread_mutex_unlock(&trava);
}

static void fazerPostar(const Job *j) {
  const char *cab[5] = { 0 };
  char aut[600], via[64], pf[64], url[300];
  int st = 0;
  if (!NV_REC_URL[0]) return;
  if (!recomenda_cabecalhos(cab, aut, sizeof aut, via, sizeof via, pf, sizeof pf)) return;
  snprintf(url, sizeof url, "%s/v1/conquista", NV_REC_URL);
  free(rede_postar_st(url, PRAZO_S, cab, j->corpo, &st));
  printf("[caca] conquista enviada: HTTP %d\n", st);
}

static void *fio(void *u) {
  (void)u;
  for (;;) {
    Job j;
    pthread_mutex_lock(&trava);
    while (nFila == 0) pthread_cond_wait(&sinal, &trava);
    j = fila[0];
    memmove(fila, fila + 1, sizeof fila[0] * (size_t)(nFila - 1));
    nFila--;
    pthread_mutex_unlock(&trava);
    if (j.tipo == JOB_LISTA) fazerLista(&j);
    else if (j.tipo == JOB_TRILHAS) fazerTrilhas();
    else if (j.tipo == JOB_POSTAR) fazerPostar(&j);
  }
  return NULL;
}

static void enfileirar(const Job *j) {
  int i;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nFila; i++)
    if (fila[i].tipo == j->tipo && j->tipo != JOB_POSTAR && !strcmp(fila[i].chave, j->chave)) {
      pthread_mutex_unlock(&trava); return; }
  if (nFila < FILA_MAX) fila[nFila++] = *j;
  if (!fioVivo) {
    pthread_t t;
    if (pthread_create(&t, NULL, fio, NULL) == 0) { pthread_detach(t); fioVivo = 1; }
  }
  pthread_cond_signal(&sinal);
  pthread_mutex_unlock(&trava);
}

static void pedirLista(Slot *s) {
  Job j;
  memset(&j, 0, sizeof j);
  j.tipo = JOB_LISTA; j.cacaTipo = s->i.tipo; j.alvo = s->i.alvo;
  snprintf(j.lista, sizeof j.lista, "%s", s->i.lista);
  snprintf(j.chave, sizeof j.chave, "%s", s->chave);
  s->pedido = 1;
  if (s->i.estado == CACA_VAZIA || s->i.estado == CACA_ERRO) s->i.estado = CACA_CARREGANDO;
  enfileirar(&j);
}

// ---------------------------------------------------------------------------
// Disco das cacadas (por perfil)
// ---------------------------------------------------------------------------
static void arquivoCacas(char *dst, size_t tam) { snprintf(dst, tam, "cacas-p%d.txt", perfis_ativo()); }

static void gravarCacas(void) {
  char arq[48], *b;
  size_t cap = (size_t)(nSlots + 1) * 600, k = 0;
  int i;
  arquivoCacas(arq, sizeof arq);
  b = malloc(cap);
  if (!b) return;
  b[0] = 0;
  for (i = 0; i < nSlots; i++) {
    const Slot *s = &slots[i];
    char nome[96], foto[160];
    snprintf(nome, sizeof nome, "%s", s->i.nome); limparCampo(nome);
    snprintf(foto, sizeof foto, "%s", s->i.foto); limparCampo(foto);
    k += (size_t)snprintf(b + k, cap - k, "v1\t%d\t%ld\t%s\t%d\t%d\t%lld\t%d\t%lld\t%s\t%s\n",
                          s->i.tipo, s->i.alvo, s->i.lista[0] ? s->i.lista : "-", s->i.escopo,
                          s->i.flags, s->criado, s->i.ganhos, s->completaEm, nome[0] ? nome : "-",
                          foto[0] ? foto : "-");
  }
  dados_gravar(arq, b);
  free(b);
}

static void soltar(Slot *s) { free(s->f); memset(s, 0, sizeof *s); }

static void lerCacas(void) {
  char arq[48], *t, *l, *sv;
  int i;
  for (i = 0; i < nSlots; i++) soltar(&slots[i]);
  nSlots = 0;
  perfilLido = perfis_ativo();
  arquivoCacas(arq, sizeof arq);
  t = dados_ler(arq);
  if (t) {
    for (l = strtok_r(t, "\n", &sv); l && nSlots < CACA_MAX; l = strtok_r(NULL, "\n", &sv)) {
      char *c[11], *q = l;
      int k;
      Slot *s = &slots[nSlots];
      for (k = 0; k < 11; k++) { c[k] = q; if (k == 10) break; q = strchr(q, '\t'); if (!q) break; *q++ = 0; }
      if (k < 10 || strcmp(c[0], "v1")) continue;
      memset(s, 0, sizeof *s);
      s->i.tipo = atoi(c[1]);
      if (s->i.tipo <= CACA_NADA || s->i.tipo >= CACA_TIPO_N) continue;
      s->i.alvo = atol(c[2]);
      if (strcmp(c[3], "-")) snprintf(s->i.lista, sizeof s->i.lista, "%s", c[3]);
      s->i.escopo = atoi(c[4]);
      s->i.flags = atoi(c[5]);
      s->criado = atoll(c[6]);
      s->i.ganhos = atoi(c[7]);
      s->completaEm = atoll(c[8]);
      if (strcmp(c[9], "-")) snprintf(s->i.nome, sizeof s->i.nome, "%s", c[9]);
      if (strcmp(c[10], "-")) snprintf(s->i.foto, sizeof s->i.foto, "%s", c[10]);
      caca_chave(s->i.tipo, s->i.alvo, s->i.lista, s->chave, sizeof s->chave);
      nSlots++;
    }
    free(t);
  }
  for (i = 0; i < nSlots; i++) pedirLista(&slots[i]);
  sujo = 1; extrasSujos = 1; revisao++;
}

// ---------------------------------------------------------------------------
// Progresso (fio principal)
// ---------------------------------------------------------------------------
static void recalcular(Slot *s) {
  ConqItem *it;
  int j, n = 0;
  s->i.vistos = s->i.total = 0;
  s->i.degraus = 0;
  s->i.anoIni = s->i.anoFim = 0;
  s->i.resolvendo = 0;
  if (!s->f || s->nf <= 0) return;
  it = malloc(sizeof *it * (size_t)s->nf);
  if (!it) return;
  for (j = 0; j < s->nf; j++) {
    CacaFilme *f = &s->f[j];
    f->visto = cacavisto_filme(f->imdb, f->tmdb, &f->quando);
    if (!caca_no_escopo(f->classe, s->i.escopo)) continue;
    if (!f->imdb[0]) s->i.resolvendo++;
    it[n].ordem = f->data; it[n].visto = f->visto; it[n].quando = f->quando;
    n++;
    if (f->data > 1000) {
      long a = f->data / 10000;
      if (!s->i.anoIni || a < s->i.anoIni) s->i.anoIni = a;
      if (a > s->i.anoFim) s->i.anoFim = a;
    }
  }
  { ConqProgresso p = conq_calcular(it, n);
    s->i.vistos = p.vistos; s->i.total = p.total; s->i.degraus = p.degraus; }
  free(it);
}

static void postarConquista(const Slot *s, int degrau) {
  Job j;
  int vis = conq_visivel_amigos(recomenda_alcance(), !(s->i.flags & CACA_F_PRIVADA), 0, 0);
  char nome[96], *q;
  memset(&j, 0, sizeof j);
  j.tipo = JOB_POSTAR;
  snprintf(nome, sizeof nome, "%s", s->i.nome);
  for (q = nome; *q; q++) if (*q == '"' || *q == '\\') *q = '\'';
  snprintf(j.corpo, sizeof j.corpo,
           "{\"chave\":\"%s\",\"nivel\":%d,\"visivel\":%d,\"nome\":\"%s\",\"vistos\":%d,\"total\":%d}",
           s->chave, conq_indice(degrau) + 1, vis, nome, s->i.vistos, s->i.total);
  snprintf(j.chave, sizeof j.chave, "%s", s->chave);
  enfileirar(&j);
}

static void verDesbloqueios(void) {
  int i, mudou = 0;
  for (i = 0; i < nSlots; i++) {
    Slot *s = &slots[i];
    int novos;
    if (s->i.estado != CACA_PRONTA || s->i.total <= 0) continue;
    // Semeia sem aviso: ao comecar a cacar alguem ja meio visto, os degraus
    // que ja existiam nao viram festa (nada sai por abrir o app).
    if (!(s->i.flags & CACA_F_SEMEADA)) {
      if (s->i.resolvendo > 0) continue;
      s->i.ganhos = conq_guardar(s->i.ganhos, s->i.degraus);
      if (s->i.degraus & CONQ_COMPLETA) s->completaEm = (long long)time(NULL);
      s->i.flags |= CACA_F_SEMEADA;
      mudou = 1;
      continue;
    }
    novos = conq_novos(s->i.ganhos, s->i.degraus);
    if (!novos) continue;
    s->i.ganhos = conq_guardar(s->i.ganhos, s->i.degraus);
    if (novos & CONQ_COMPLETA) s->completaEm = (long long)time(NULL);
    printf("[caca] %s: novo degrau %s (%d/%d)\n", s->chave, conq_rotulo(conq_maior(novos)),
           s->i.vistos, s->i.total);
    conq_aviso_somar(&aviso, i);
    postarConquista(s, conq_maior(novos));
    mudou = 1;
  }
  if (mudou) { gravarCacas(); extrasSujos = 1; revisao++; }
}

static void avisar(int i, int extras) {
  IlhaAcao a;
  char frase[200], onde[200];
  const Slot *s;
  int d;
  if (i < 0 || i >= nSlots) return;
  s = &slots[i];
  d = conq_maior(s->i.ganhos);
  snprintf(frase, sizeof frase, i18n("Conquista: %s · %s"), i18n(conq_rotulo(d)), s->i.nome);
  if (extras > 0)
    snprintf(onde, sizeof onde, i18n("Você viu %d de %d · +%d conquistas"), s->i.vistos,
             s->i.total, extras);
  else if (s->i.total > 0 && s->i.vistos >= s->i.total) {
    char anos[40] = "";
    if (s->i.anoIni > 0)
      snprintf(anos, sizeof anos, " \xc2\xb7 %ld\xe2\x80\x93%ld", s->i.anoIni, s->i.anoFim);
    snprintf(onde, sizeof onde, i18n("Você viu todos os %d%s"), s->i.total, anos);
  } else
    snprintf(onde, sizeof onde, i18n("Você viu %d de %d"), s->i.vistos, s->i.total);
  memset(&a, 0, sizeof a);
  a.icone = "aj_trophy";
  a.frase = frase;
  a.titulo = s->i.nome;
  a.onde = onde;
  a.thumb = s->i.foto;
  a.tipo = ILHA_ACENTO;
  a.voar = s->i.foto[0] != 0;
  ilhaacao_feita(&a);
}

// ---------------------------------------------------------------------------
// Fileira e colecao na Home (pastas extras de colecoes.c)
// ---------------------------------------------------------------------------
static int fileiraVisivel(const Slot *s) {
  if (!(s->i.flags & CACA_F_FILEIRA) || s->i.total <= 0) return 0;
  // Fechou: fica um dia com "Completa", depois some sozinha (plano 8.3).
  if (s->i.vistos >= s->i.total)
    return s->completaEm > 0 && (long long)time(NULL) - s->completaEm < 86400;
  return 1;
}

static void aplicarExtras(void) {
  static ColFolder pastas[CACA_MAX * 2];
  static ColSource fontes[CACA_MAX * 2];
  int i, n = 0, nf;
  memset(pastas, 0, sizeof pastas);
  memset(fontes, 0, sizeof fontes);
  for (i = 0; i < nSlots; i++) {
    const Slot *s = &slots[i];
    ColFolder *v = &pastas[n];
    if (!fileiraVisivel(s)) continue;
    v->sources = &fontes[n];
    snprintf(v->group, sizeof v->group, "%s", i18n("Caçadas"));
    snprintf(v->groupId, sizeof v->groupId, "%s", GRUPO_FILEIRA);
    snprintf(v->id, sizeof v->id, "caca_%s", s->chave);
    if (s->i.vistos >= s->i.total)
      snprintf(v->title, sizeof v->title, i18n("%s · Completa"), s->i.nome);
    else
      snprintf(v->title, sizeof v->title, i18n("%s · faltam %d"), s->i.nome, s->i.total - s->i.vistos);
    snprintf(v->cover, sizeof v->cover, "%s", s->i.foto);
    v->forma = COL_FORMA_POSTER;
    snprintf(fontes[n].prov, sizeof fontes[n].prov, "caca");
    snprintf(fontes[n].catId, sizeof fontes[n].catId, "-%s", s->chave);
    snprintf(fontes[n].title, sizeof fontes[n].title, "%s", v->title);
    snprintf(fontes[n].midia, sizeof fontes[n].midia, "MOVIE");
    v->nSources = 1;
    n++;
  }
  nf = n;
  col_extra_definir_grupo(GRUPO_FILEIRA, pastas, nf);
  n = 0;
  for (i = 0; i < nSlots; i++) {
    const Slot *s = &slots[i];
    ColFolder *v = &pastas[nf + n];
    if (!(s->i.flags & CACA_F_COLECAO)) continue;
    v->sources = &fontes[nf + n];
    snprintf(v->group, sizeof v->group, "%s", i18n("Coleções da caça"));
    snprintf(v->groupId, sizeof v->groupId, "%s", GRUPO_COLECAO);
    snprintf(v->id, sizeof v->id, "cacacol_%s", s->chave);
    snprintf(v->title, sizeof v->title, "%s", s->i.nome);
    snprintf(v->cover, sizeof v->cover, "%s", s->i.foto);
    v->forma = COL_FORMA_POSTER;
    snprintf(fontes[nf + n].prov, sizeof fontes[nf + n].prov, "caca");
    snprintf(fontes[nf + n].catId, sizeof fontes[nf + n].catId, "*%s", s->chave);
    snprintf(fontes[nf + n].title, sizeof fontes[nf + n].title, "%s", s->i.nome);
    snprintf(fontes[nf + n].midia, sizeof fontes[nf + n].midia, "MOVIE");
    v->nSources = 1;
    n++;
  }
  col_extra_definir_grupo(GRUPO_COLECAO, pastas + nf, n);
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void caca_iniciar(void) {
  conq_aviso_zerar(&aviso);
  lerCacas();
}

void caca_perfil_trocado(void) {
  soltar(&slots[CACA_PREVIA]);
  lerCacas();
  // O aviso pendente era do perfil anterior.
  aviso.pendentes = 0; aviso.primeiro = -1;
}

unsigned caca_revisao(void) { return revisao; }

static Slot *slotPorChave(const char *chave) {
  int i;
  for (i = 0; i < nSlots; i++) if (!strcmp(slots[i].chave, chave)) return &slots[i];
  if (slots[CACA_PREVIA].i.tipo && !strcmp(slots[CACA_PREVIA].chave, chave)) return &slots[CACA_PREVIA];
  return NULL;
}

void caca_passo(unsigned agora, int momentoBom) {
  int i, adotou = 0;
  if (perfilLido != perfis_ativo()) caca_perfil_trocado();
  // Adota o que o fio trouxe.
  pthread_mutex_lock(&trava);
  for (i = 0; i < nRes; i++) {
    Slot *s = slotPorChave(res[i].chave);
    if (!s) { free(res[i].f); continue; }
    if (res[i].estado == CACA_ERRO && s->nf > 0) { free(res[i].f); continue; }
    free(s->f);
    s->f = res[i].f; s->nf = res[i].nf;
    s->i.estado = res[i].estado;
    if (res[i].nome[0] && (s->i.tipo == CACA_SAGA || s->i.tipo == CACA_TRILHA || !s->i.nome[0]))
      snprintf(s->i.nome, sizeof s->i.nome, "%s", res[i].nome);
    if (res[i].foto[0] && !s->i.foto[0]) snprintf(s->i.foto, sizeof s->i.foto, "%s", res[i].foto);
    adotou = 1;
  }
  nRes = 0;
  if (nTrilhasNovas >= 0) {
    memcpy(trilhas, trilhasNovas, sizeof trilhas);
    nTrilhas = nTrilhasNovas; nTrilhasNovas = -1; revisao++;
  }
  pthread_mutex_unlock(&trava);
  if (adotou) { sujo = 1; recalcEm = 0; }
  { unsigned long long g = cacavisto_geracao();
    if (g != genVisto) { genVisto = g; if (!sujo) recalcEm = agora + RECALC_MS; sujo = 1; } }
  // Debounce: o "visto" muda em rajada (o sync traz centenas de uma vez).
  if (sujo && (adotou || recalcEm == 0 || (int)(agora - recalcEm) >= 0)) {
    for (i = 0; i < nSlots; i++) recalcular(&slots[i]);
    if (slots[CACA_PREVIA].i.tipo) recalcular(&slots[CACA_PREVIA]);
    sujo = 0; recalcEm = 0;
    verDesbloqueios();
    extrasSujos = 1;
    revisao++;
  }
  if (extrasSujos) { extrasSujos = 0; aplicarExtras(); }
  // O aviso: um por sessao, so sem player (depois dos creditos / na Home).
  { int id, extras;
    if (conq_aviso_tirar(&aviso, momentoBom, &id, &extras)) avisar(id, extras); }
  // Uma vez por hora: refaz as listas vencidas (o fio decide pelo TTL).
  if (agora - ultimoPasso > 3600000u) {
    ultimoPasso = agora;
    for (i = 0; i < nSlots; i++) if (slots[i].i.estado != CACA_CARREGANDO) pedirLista(&slots[i]);
  }
}

int caca_n(void) { return nSlots; }

static const Slot *slotIdx(int i) {
  if (i >= 0 && i < nSlots) return &slots[i];
  if (i == CACA_PREVIA && slots[CACA_PREVIA].i.tipo) return &slots[CACA_PREVIA];
  return NULL;
}

int caca_info(int i, CacaInfo *saida) {
  const Slot *s = slotIdx(i);
  if (!s || !saida) return 0;
  *saida = s->i;
  return 1;
}

int caca_n_filmes(int i) {
  const Slot *s = slotIdx(i);
  int j, n = 0;
  if (!s || !s->f) return 0;
  for (j = 0; j < s->nf; j++) if (caca_no_escopo(s->f[j].classe, s->i.escopo)) n++;
  return n;
}

int caca_filme(int i, int k, CacaFilme *saida) {
  const Slot *s = slotIdx(i);
  int j, n = 0;
  if (!s || !s->f || !saida) return 0;
  for (j = 0; j < s->nf; j++) {
    if (!caca_no_escopo(s->f[j].classe, s->i.escopo)) continue;
    if (n++ == k) { *saida = s->f[j]; return 1; }
  }
  return 0;
}

int caca_achar(int tipo, long alvo, const char *lista) {
  char ch[96];
  int i;
  caca_chave(tipo, alvo, lista, ch, sizeof ch);
  for (i = 0; i < nSlots; i++) if (!strcmp(slots[i].chave, ch)) return i;
  return -1;
}

int caca_por_chave(const char *chave) {
  int i;
  if (!chave) return -1;
  for (i = 0; i < nSlots; i++) if (!strcmp(slots[i].chave, chave)) return i;
  return -1;
}

int caca_criar(int tipo, long alvo, const char *lista, const char *nome, const char *foto) {
  Slot *s;
  int i = caca_achar(tipo, alvo, lista);
  if (i >= 0) return i;
  if (nSlots >= CACA_MAX || tipo <= CACA_NADA || tipo >= CACA_TIPO_N) return -1;
  s = &slots[nSlots];
  memset(s, 0, sizeof *s);
  s->i.tipo = tipo; s->i.alvo = alvo;
  snprintf(s->i.lista, sizeof s->i.lista, "%s", lista ? lista : "");
  snprintf(s->i.nome, sizeof s->i.nome, "%s", nome ? nome : "");
  snprintf(s->i.foto, sizeof s->i.foto, "%s", foto ? foto : "");
  s->criado = (long long)time(NULL);
  caca_chave(tipo, alvo, lista, s->chave, sizeof s->chave);
  // A previa do mesmo alvo ja tem a lista: herda, sem pedir de novo.
  if (slots[CACA_PREVIA].i.tipo && !strcmp(slots[CACA_PREVIA].chave, s->chave) &&
      slots[CACA_PREVIA].nf > 0) {
    s->f = malloc(sizeof *s->f * (size_t)slots[CACA_PREVIA].nf);
    if (s->f) {
      memcpy(s->f, slots[CACA_PREVIA].f, sizeof *s->f * (size_t)slots[CACA_PREVIA].nf);
      s->nf = slots[CACA_PREVIA].nf;
      s->i.estado = CACA_PRONTA;
    }
  }
  nSlots++;
  if (!s->f) pedirLista(s);
  gravarCacas();
  sujo = 1; recalcEm = 0; revisao++;
  printf("[caca] comecou %s (%s)\n", s->chave, s->i.nome);
  return nSlots - 1;
}

void caca_parar(int i) {
  if (i < 0 || i >= nSlots) return;
  printf("[caca] parou %s\n", slots[i].chave);
  free(slots[i].f);
  memmove(&slots[i], &slots[i + 1], sizeof slots[0] * (size_t)(nSlots - i - 1));
  nSlots--;
  memset(&slots[nSlots], 0, sizeof slots[0]);
  gravarCacas();
  extrasSujos = 1; revisao++;
}

void caca_alternar_escopo(int i, int bit) {
  if (i < 0 || i >= nSlots) return;
  slots[i].i.escopo ^= bit;
  // Mudar o escopo nao e conquista: os degraus do escopo novo sao semeados.
  slots[i].i.flags &= ~CACA_F_SEMEADA;
  gravarCacas();
  sujo = 1; recalcEm = 0; revisao++;
}

int caca_alternar(int i, int marca) {
  if (i < 0 || i >= nSlots) return 0;
  slots[i].i.flags ^= marca;
  gravarCacas();
  extrasSujos = 1; revisao++;
  return (slots[i].i.flags & marca) != 0;
}

void caca_previa(int tipo, long alvo, const char *nome, const char *foto) {
  Slot *s = &slots[CACA_PREVIA];
  char ch[96];
  caca_chave(tipo, alvo, "", ch, sizeof ch);
  if (s->i.tipo && !strcmp(s->chave, ch)) return;
  soltar(s);
  s->i.tipo = tipo; s->i.alvo = alvo;
  snprintf(s->i.nome, sizeof s->i.nome, "%s", nome ? nome : "");
  snprintf(s->i.foto, sizeof s->i.foto, "%s", foto ? foto : "");
  snprintf(s->chave, sizeof s->chave, "%s", ch);
  // Ja e uma cacada: a previa e so um espelho, sem rede.
  if (caca_achar(tipo, alvo, "") >= 0) {
    const Slot *o = &slots[caca_achar(tipo, alvo, "")];
    s->i = o->i;
    if (o->nf > 0 && (s->f = malloc(sizeof *s->f * (size_t)o->nf)) != NULL) {
      memcpy(s->f, o->f, sizeof *s->f * (size_t)o->nf); s->nf = o->nf; }
    revisao++;
    return;
  }
  pedirLista(s);
  revisao++;
}

int caca_previa_cacar(void) {
  const Slot *s = &slots[CACA_PREVIA];
  if (!s->i.tipo) return -1;
  return caca_criar(s->i.tipo, s->i.alvo, s->i.lista, s->i.nome, s->i.foto);
}

int caca_sugestoes(int *idx, int max) {
  int i, n = 0, j;
  for (i = 0; i < nSlots && n < max; i++) {
    const CacaInfo *c = &slots[i].i;
    int falta = c->total - c->vistos;
    if (c->total <= 0 || falta <= 0 || falta > 3 || c->vistos * 2 < c->total) continue;
    idx[n++] = i;
  }
  // Mais perto de fechar primeiro.
  for (i = 1; i < n; i++)
    for (j = i; j > 0; j--) {
      const CacaInfo *a = &slots[idx[j - 1]].i, *b = &slots[idx[j]].i;
      if (a->total - a->vistos <= b->total - b->vistos) break;
      { int t = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t; }
    }
  return n;
}

int caca_n_trilhas(void) {
  if (!trilhasPedidas) {
    Job j;
    trilhasPedidas = 1;
    memset(&j, 0, sizeof j);
    j.tipo = JOB_TRILHAS;
    snprintf(j.chave, sizeof j.chave, "trilhas");
    enfileirar(&j);
  }
  return nTrilhas;
}

int caca_trilha(int i, CacaTrilha *saida) {
  if (i < 0 || i >= nTrilhas || !saida) return 0;
  *saida = trilhas[i];
  return 1;
}

int caca_itens(int i, int soFaltam, void *dst, int max) {
  CatItem *v = dst;
  const Slot *s = slotIdx(i);
  int j, n = 0;
  if (!s || !s->f || !v) return 0;
  for (j = 0; j < s->nf && n < max; j++) {
    const CacaFilme *f = &s->f[j];
    if (!caca_no_escopo(f->classe, s->i.escopo) || !f->imdb[0]) continue;
    if (soFaltam && f->visto) continue;
    memset(&v[n], 0, sizeof v[n]);
    snprintf(v[n].imdb, sizeof v[n].imdb, "%s", f->imdb);
    snprintf(v[n].tipo, sizeof v[n].tipo, "movie");
    snprintf(v[n].titulo, sizeof v[n].titulo, "%s", f->titulo);
    snprintf(v[n].poster, sizeof v[n].poster, "%s", f->poster);
    if (f->data > 0) snprintf(v[n].meta, sizeof v[n].meta, "%ld", f->data / 10000);
    v[n].tmdb = f->tmdb;
    n++;
  }
  return n;
}
