// Ver plugins.h. Portado de agente/plugins2 (F09) e refeito em cima do N01:
// manifesto/codigo/TMDB por plugrede com teto antes de alocar, geracao de
// conta/perfil por RedeGrupo, estado por conta+perfil, ACK do sync por
// revisao, e nenhuma trava segurada durante rede ou disco.
#include "plugins.h"
#include "addons.h"
#include "pluginjs.h"
#include "plugrede.h"
#include "dados.h"
#include "descoberta.h"
#include "idbase.h"
#include "js.h"
#include "perfis.h"
#include "sessao.h"
#include "perfisessao.h"
#include "debrid.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define PLUG_HIT_S         (30 * 24 * 3600)  // successful scrapers stay preferred for 30 days
#define PLUG_CACHE_S       (6 * 3600)        // manifesto e codigo: 6 h
#define PLUG_MANIFESTO_MAX (256u * 1024)
#define PLUG_CODIGO_MAX    (2u * 1024 * 1024)
#define PLUG_PRAZO_MS      20000              // por scraper
// A RODADA INTEIRA, contada do disparo: scraper que ainda nao comecou aos
// PLUG_TETO_MS nao comeca, e quem esta rodando e cortado (fica o que terminou).
#ifndef PLUG_TETO_MS
#define PLUG_TETO_MS       30000
#endif
#define PLUG_REDE_S        15                 // por fetch
#ifndef PLUG_PARALELOS
#define PLUG_PARALELOS     8
#endif

typedef struct {
  char id[64], nome[96], arquivo[256], url[700];
  int filme, serie, ativo, repo, anime;
  time_t ultimoHit;        // Round-local ordering score; not configuration.
} Scraper;

// Escolha por scraper, por perfil (local, como no web): chave (manifesto, id).
#define PLUG_ESCOLHAS_MAX 320
typedef struct { unsigned long long h; char id[64]; char man[600]; int ativo; time_t ultimoHit; } Escolha;

typedef struct { PlugRepo r; int estado; } Repo;

typedef struct {
  Repo repos[PLUG_REPOS_MAX]; int nRepos;
  Scraper scr[PLUG_SCRAPERS_MAX]; int nScr;
  Escolha esc[PLUG_ESCOLHAS_MAX]; int nEsc;
  int ligado, pendente;
  unsigned rev;
} Estado;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Estado E;                       // sob `trava`
static PlugResumo ultimoResumo;
static char resumoId[64], resumoTipo[16];
static unsigned resumoGeracao;
static char ctxUsuario[80];            // dono do estado carregado (sob trava)
static int ctxPerfil = -1;
static unsigned geracao = 1;           // atomico
static int ligadoAt;                   // espelho atomico de E.ligado
static int atualizando;                // atomico
static RedeGrupo *grupo;
static pthread_once_t grupoUma = PTHREAD_ONCE_INIT;
// Gravacao do estado fora da trava principal; o numero de sequencia impede
// que uma gravacao velha sobrescreva uma nova.
static pthread_mutex_t gravaTrava = PTHREAD_MUTEX_INITIALIZER;
static unsigned gravaSeq, gravaFeita;

static void grupoCriar(void) { grupo = rede_grupo_criar(); }
static RedeGrupo *grupoDe(void) { pthread_once(&grupoUma, grupoCriar); return grupo; }

static unsigned gerAtual(void) { return __atomic_load_n(&geracao, __ATOMIC_ACQUIRE); }
unsigned plugins_geracao(void) { return gerAtual(); }
static void gerAvancar(void) {
  __atomic_add_fetch(&geracao, 1, __ATOMIC_ACQ_REL);
  if (grupoDe()) rede_grupo_avancar(grupoDe());
}

int plugins_disponivel(void) { return (plugrede_capacidades() & PR_CAP_REDE) != 0; }

// ------------------------------------------------------------ utilitarios

static unsigned long long fnv(const char *s) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
  return h;
}

// "https://x/y" -> "https://x/y/manifest.json"; quem ja termina em .json fica.
static void urlManifesto(const char *u, char *dst, size_t tam) {
  size_t n;
  char t[600];
  snprintf(t, sizeof t, "%s", u ? u : "");
  n = strlen(t);
  while (n && (t[n - 1] == ' ' || t[n - 1] == '\n' || t[n - 1] == '\r')) t[--n] = 0;
  { char *h = strchr(t, '#'); if (h) *h = 0; }
  n = strlen(t);
  if (n > 5 && !strcasecmp(t + n - 5, ".json")) { snprintf(dst, tam, "%s", t); return; }
  while (n && t[n - 1] == '/') t[--n] = 0;
  snprintf(dst, tam, "%s/manifest.json", t);
}

// Endereco do codigo: relativo a pasta do manifesto (resolvePluginUrl).
static void urlCodigo(const char *manifesto, const char *arquivo, char *dst, size_t tam) {
  const char *barra;
  if (!strncmp(arquivo, "http://", 7) || !strncmp(arquivo, "https://", 8)) { snprintf(dst, tam, "%s", arquivo); return; }
  if (arquivo[0] == '/') {
    const char *h = strstr(manifesto, "://");
    const char *fimHost = h ? strchr(h + 3, '/') : NULL;
    int nh = fimHost ? (int)(fimHost - manifesto) : (int)strlen(manifesto);
    snprintf(dst, tam, "%.*s%s", nh, manifesto, arquivo);
    return;
  }
  barra = strrchr(manifesto, '/');
  while (!strncmp(arquivo, "./", 2)) arquivo += 2;
  snprintf(dst, tam, "%.*s/%s", barra ? (int)(barra - manifesto) : (int)strlen(manifesto), manifesto, arquivo);
}

static int ehHttp(const char *u) { return u && (!strncmp(u, "http://", 7) || !strncmp(u, "https://", 8)); }

// Repositorio que nao roda aqui (DEX/CloudStream do Android): fica na lista e
// volta igual para a conta, mas nao e lido nem executado.
static int naoRoda(const PlugRepo *r) {
  size_t n = strlen(r->url);
  if (r->tipo[0] && strcasecmp(r->tipo, "NUVIO_JS") && strcasecmp(r->tipo, "JS")) return 1;
  return n > 4 && !strcasecmp(r->url + n - 4, ".cs3");
}

static unsigned long long chaveEscolha(const char *man, const char *id) {
  char t[700];
  snprintf(t, sizeof t, "%s\n%s", man, id);
  return fnv(t);
}
static Escolha *escolhaDe(Estado *s, const char *man, const char *id) {
  unsigned long long h = chaveEscolha(man, id);
  int k;
  for (k = 0; k < s->nEsc; k++) if (s->esc[k].h == h) return &s->esc[k];
  return NULL;
}
static void lembrarEscolha(Estado *s, const char *man, const char *id, int ativo) {
  Escolha *e = escolhaDe(s, man, id);
  if (!e) {
    if (s->nEsc >= PLUG_ESCOLHAS_MAX) {
      memmove(s->esc, s->esc + 1, sizeof(Escolha) * (PLUG_ESCOLHAS_MAX - 1));
      s->nEsc--;
    }
    e = &s->esc[s->nEsc++];
    memset(e, 0, sizeof *e);
    e->h = chaveEscolha(man, id);
    snprintf(e->id, sizeof e->id, "%s", id);
    snprintf(e->man, sizeof e->man, "%s", man);
  }
  e->ativo = ativo ? 1 : 0;
}

// ------------------------------------------------------------ estado em disco
// plugins-<conta>-p<perfil>.txt: POR CONTA E PERFIL — uma conta nova no mesmo
// aparelho nao herda os repositorios da anterior. Linhas:
//   "ligado\t1", "rev\t<n>\t<pendente>", "repo\t<ativo>\t<tipo>\t<nome>\t<url>",
//   "scraper\t<ativo>\t<id>\t<url do manifesto>".
static void nomeEstado(const char *usuario, int perfil, char *dst, size_t tam) {
  snprintf(dst, tam, "plugins-%08llx-p%d.txt",
           usuario && *usuario ? fnv(usuario) & 0xffffffffULL : 0ULL, perfil);
}

// Monta o texto com a trava; quem chama grava FORA dela (escreverEstado).
static char *serializar(char *nome, size_t tamNome, unsigned *seq) {
  size_t cap = 512 + (size_t)E.nRepos * 800 + (size_t)E.nEsc * 1500, u = 0;
  char *buf = malloc(cap);
  int i;
  if (!buf) return NULL;
  u += (size_t)snprintf(buf + u, cap - u, "ligado\t%d\nrev\t%u\t%d\n", E.ligado, E.rev, E.pendente);
  for (i = 0; i < E.nRepos; i++) {
    char nomeR[96], *q;
    snprintf(nomeR, sizeof nomeR, "%s", E.repos[i].r.nome);
    for (q = nomeR; *q; q++) if (*q == '\t' || *q == '\n' || *q == '\r') *q = ' ';
    u += (size_t)snprintf(buf + u, cap - u, "repo\t%d\t%s\t%s\t%s\n", E.repos[i].r.ativo,
                          E.repos[i].r.tipo[0] ? E.repos[i].r.tipo : "NUVIO_JS", nomeR, E.repos[i].r.url);
  }
  for (i = 0; i < E.nEsc; i++) {
    u += (size_t)snprintf(buf + u, cap - u, "scraper\t%d\t%s\t%s\n", E.esc[i].ativo, E.esc[i].id, E.esc[i].man);
    if (E.esc[i].ultimoHit)
      u += (size_t)snprintf(buf + u, cap - u, "hit\t%lld\t%s\t%s\n",
                            (long long)E.esc[i].ultimoHit, E.esc[i].id, E.esc[i].man);
  }
  nomeEstado(ctxUsuario, ctxPerfil, nome, tamNome);
  *seq = ++gravaSeq;
  return buf;
}
static void escreverEstado(const char *nome, char *txt, unsigned seq) {
  if (!txt) return;
  pthread_mutex_lock(&gravaTrava);
  if (seq > gravaFeita) { dados_gravar(nome, txt); gravaFeita = seq; }
  pthread_mutex_unlock(&gravaTrava);
  free(txt);
}
// Atalho: serializa com a trava JA segura, solta e grava.
static void soltarEGravar(void) {
  char nome[96];
  unsigned seq;
  char *txt = serializar(nome, sizeof nome, &seq);
  pthread_mutex_unlock(&trava);
  escreverEstado(nome, txt, seq);
}

// ------------------------------------------------------------ cache
// "//nvcache <epoch>\n" na primeira linha (comentario JS: o codigo em cache
// roda como esta). Codigo e manifesto sao publicos: cache por URL.
static char *cacheLer(const char *chave, long *idade) {
  char nome[64], *t;
  snprintf(nome, sizeof nome, "plugins-c-%016llx.txt", fnv(chave));
  t = dados_ler(nome);
  *idade = -1;
  if (t && !strncmp(t, "//nvcache ", 10)) *idade = (long)(time(NULL) - atol(t + 10));
  return t;
}
static void cacheGravar(const char *chave, const char *conteudo) {
  char nome[64], *buf;
  size_t n = strlen(conteudo) + 40;
  snprintf(nome, sizeof nome, "plugins-c-%016llx.txt", fnv(chave));
  buf = malloc(n);
  if (!buf) return;
  snprintf(buf, n, "//nvcache %ld\n%s", (long)time(NULL), conteudo);
  dados_gravar_leve(nome, buf);
  free(buf);
}
static const char *semCarimbo(const char *t) {
  if (t && !strncmp(t, "//nvcache ", 10)) { const char *n = strchr(t, '\n'); return n ? n + 1 : t + strlen(t); }
  return t;
}

// GET com teto e geracao. NULL em qualquer falha; texto terminado em NUL.
static char *baixar(const char *url, size_t teto, unsigned prazoMs, RedeJob *job) {
  RedePedido q;
  RedeResposta r;
  char *corpo = NULL;
  memset(&q, 0, sizeof q);
  q.url = url; q.max_bytes = teto; q.prazo_ms = prazoMs; q.seguir = 1; q.job = job;
  if (plugrede_pedir(&q, &r) && r.status >= 200 && r.status < 300 && r.corpo &&
      memchr(r.corpo, 0, r.n_corpo) == NULL) {
    corpo = r.corpo; r.corpo = NULL;           // ja terminado em NUL pelo N01
  }
  rede_resposta_limpar(&r);
  return corpo;
}

// ------------------------------------------------------------ manifesto

// Le scrapers[] de um manifesto para o repositorio i. Com a trava.
static int aplicarManifesto(Estado *s, int i, const char *json, const char *urlMan) {
  const char *p, *f;
  int k = 0;
  char nomeRepo[96];
  { int a, b = 0;
    for (a = 0; a < s->nScr; a++) if (s->scr[a].repo != i) s->scr[b++] = s->scr[a];
    s->nScr = b; }
  if (js_texto_raiz(json, "name", nomeRepo, sizeof nomeRepo) && !s->repos[i].r.nome[0])
    snprintf(s->repos[i].r.nome, sizeof s->repos[i].r.nome, "%s", nomeRepo);
  for (p = js_array(json, NULL, "scrapers"); p && *p == '{' && s->nScr < PLUG_SCRAPERS_MAX; p = js_prox(f)) {
    Scraper *sc = &s->scr[s->nScr];
    char tipos[200] = "", en[16] = "";
    f = js_fim(p);
    if (!f) break;
    memset(sc, 0, sizeof *sc);
    js_texto_raiz_em(p, f, "id", sc->id, sizeof sc->id);
    js_texto_raiz_em(p, f, "name", sc->nome, sizeof sc->nome);
    js_texto_raiz_em(p, f, "filename", sc->arquivo, sizeof sc->arquivo);
    if (!sc->id[0] || !sc->arquivo[0]) continue;
    if (!sc->nome[0]) snprintf(sc->nome, sizeof sc->nome, "%s", sc->id);
    js_bruto(p, f, "supportedTypes", tipos, sizeof tipos);
    sc->filme = !tipos[0] || strstr(tipos, "\"movie\"") != NULL;
    sc->serie = !tipos[0] || strstr(tipos, "\"tv\"") || strstr(tipos, "\"series\"") || strstr(tipos, "\"anime\"");
    sc->anime = strstr(tipos, "\"anime\"") && !sc->filme &&
                !strstr(tipos, "\"tv\"") && !strstr(tipos, "\"series\"");
    sc->ativo = !(js_bruto(p, f, "enabled", en, sizeof en) && !strcmp(en, "false"));
    { const Escolha *e = escolhaDe(s, urlMan, sc->id); if (e) sc->ativo = e->ativo; }
    sc->repo = i;
    urlCodigo(urlMan, sc->arquivo, sc->url, sizeof sc->url);
    if (!ehHttp(sc->url)) continue;
    s->nScr++; k++;
  }
  s->repos[i].estado = k > 0 ? 1 : -1;
  return k;
}

static int achaRepo(const Estado *s, const char *url) {
  int i;
  for (i = 0; i < s->nRepos; i++) if (!strcmp(s->repos[i].r.url, url)) return i;
  return -1;
}

// Le o arquivo de estado + caches FORA da trava e troca de uma vez.
static void carregar(void) {
  char nome[96], usuario[80], *t, *l, *salva = NULL;
  int perfil, i;
  Estado *n = calloc(1, sizeof *n);
  char *mans[PLUG_REPOS_MAX] = {0};
  char urlMan[PLUG_REPOS_MAX][700];
  if (!n) return;
  snprintf(usuario, sizeof usuario, "%s", sessao_usuario());
  perfil = perfis_ativo_addons();
  nomeEstado(usuario, perfil, nome, sizeof nome);
  t = dados_ler(nome);
  for (l = t ? strtok_r(t, "\n", &salva) : NULL; l; l = strtok_r(NULL, "\n", &salva)) {
    if (!strncmp(l, "ligado\t", 7)) n->ligado = atoi(l + 7) != 0;
    else if (!strncmp(l, "rev\t", 4)) {
      char *q = strchr(l + 4, '\t');
      n->rev = (unsigned)strtoul(l + 4, NULL, 10);
      n->pendente = q ? atoi(q + 1) != 0 : 0;
    } else if (!strncmp(l, "repo\t", 5) && n->nRepos < PLUG_REPOS_MAX) {
      char *c[3]; int k; char *p = l + 5;
      for (k = 0; k < 3 && p; k++) { c[k] = p; p = strchr(p, '\t'); if (p) *p++ = 0; }
      if (k < 3 || !p || !ehHttp(p)) continue;
      memset(&n->repos[n->nRepos], 0, sizeof n->repos[n->nRepos]);
      n->repos[n->nRepos].r.ativo = atoi(c[0]) != 0;
      snprintf(n->repos[n->nRepos].r.tipo, sizeof n->repos[n->nRepos].r.tipo, "%s", c[1]);
      snprintf(n->repos[n->nRepos].r.nome, sizeof n->repos[n->nRepos].r.nome, "%s", c[2]);
      snprintf(n->repos[n->nRepos].r.url, sizeof n->repos[n->nRepos].r.url, "%s", p);
      n->nRepos++;
    } else if (!strncmp(l, "hit\t", 4)) {
      char *id = strchr(l + 4, '\t'), *man;
      if (!id) continue;
      *id++ = 0; man = strchr(id, '\t');
      if (!man) continue;
      *man++ = 0;
      Escolha *e = escolhaDe(n, man, id);
      if (e) e->ultimoHit = (time_t)strtoll(l + 4, NULL, 10);
    } else if (!strncmp(l, "scraper\t", 8)) {
      char *ativo = l + 8, *id, *man;
      id = strchr(ativo, '\t'); if (!id) continue; *id++ = 0;
      man = strchr(id, '\t'); if (!man || !*id) continue; *man++ = 0;
      if (*man) lembrarEscolha(n, man, id, atoi(ativo) != 0);
    }
  }
  free(t);
  for (i = 0; i < n->nRepos; i++) {
    long idade;
    n->repos[i].estado = naoRoda(&n->repos[i].r) ? 2 : 0;
    if (n->repos[i].estado) continue;
    urlManifesto(n->repos[i].r.url, urlMan[i], sizeof urlMan[i]);
    mans[i] = cacheLer(urlMan[i], &idade);
  }
  for (i = 0; i < n->nRepos; i++)
    if (mans[i]) { aplicarManifesto(n, i, semCarimbo(mans[i]), urlMan[i]); free(mans[i]); }
  pthread_mutex_lock(&trava);
  E = *n;
  snprintf(ctxUsuario, sizeof ctxUsuario, "%s", usuario);
  ctxPerfil = perfil;
  __atomic_store_n(&ligadoAt, E.ligado, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&trava);
  free(n);
}

void plugins_iniciar(void) {
  int lig, nr, ns;
  gerAvancar();
  carregar();
  pthread_mutex_lock(&trava);
  lig = E.ligado; nr = E.nRepos; ns = E.nScr;
  pthread_mutex_unlock(&trava);
  printf("[plugins] %s, %d repositorio(s), %d scraper(s)%s\n", lig ? "ligados" : "desligados", nr, ns,
         plugins_disponivel() ? "" : " (indisponivel neste aparelho)");
  fflush(stdout);
  if (lig && nr && plugins_disponivel()) plugins_atualizar();
}
void plugins_perfil_mudou(void) {
  int igual;
  pthread_mutex_lock(&trava);
  igual = ctxPerfil == perfis_ativo_addons() && !strcmp(ctxUsuario, sessao_usuario());
  pthread_mutex_unlock(&trava);
  if (!igual) plugins_iniciar();
}
int plugins_ligado(void) { return __atomic_load_n(&ligadoAt, __ATOMIC_ACQUIRE) && plugins_disponivel(); }
void plugins_definir_ligado(int l) {
  int precisa;
  pthread_mutex_lock(&trava);
  E.ligado = l ? 1 : 0;
  __atomic_store_n(&ligadoAt, E.ligado, __ATOMIC_RELEASE);
  precisa = E.ligado && E.nRepos > 0;
  soltarEGravar();
  if (precisa && plugins_disponivel()) plugins_atualizar();
}

// ------------------------------------------------------------ atualizar
// Baixa o manifesto (cache de 6 h, salvo `forcar`). Sem trava durante disco ou
// rede; aplica com a trava so se a geracao e o repositorio ainda sao os mesmos.
static void atualizarRepo(const char *url, unsigned ger, int forcar) {
  char man[700], *t, *corpo = NULL, nomeR[96] = "";
  long idade;
  int i, k = 0;
  RedeJob *job;
  urlManifesto(url, man, sizeof man);
  t = cacheLer(man, &idade);
  if (t && !forcar && idade >= 0 && idade < PLUG_CACHE_S) { free(t); return; }
  job = rede_job_criar(grupoDe());
  if (gerAtual() == ger) corpo = baixar(man, PLUG_MANIFESTO_MAX, 15000, job);
  rede_job_soltar(job);
  pthread_mutex_lock(&trava);
  if (gerAtual() == ger && (i = achaRepo(&E, url)) >= 0) {
    k = corpo ? aplicarManifesto(&E, i, corpo, man) : 0;
    if (k <= 0 && t) aplicarManifesto(&E, i, semCarimbo(t), man);   // fica o velho
    else if (k <= 0) E.repos[i].estado = -1;
    snprintf(nomeR, sizeof nomeR, "%s", E.repos[i].r.nome);
  }
  pthread_mutex_unlock(&trava);
  if (k > 0 && gerAtual() == ger) cacheGravar(man, corpo);
  printf("[plugins] repositorio %s: %d scraper(s)%s\n", nomeR[0] ? nomeR : "?", k, corpo ? "" : " (sem resposta)");
  free(corpo); free(t);
}

static void *fioAtualizar(void *u) {
  char (*urls)[600] = NULL;
  int i, n = 0, forcar = u != NULL;
  unsigned ger;
  for (;;) {
    pthread_mutex_lock(&trava);
    ger = gerAtual();
    free(urls);
    urls = calloc((size_t)(E.nRepos ? E.nRepos : 1), sizeof *urls);
    for (i = 0, n = 0; urls && i < E.nRepos; i++)
      if (!naoRoda(&E.repos[i].r)) snprintf(urls[n++], sizeof urls[0], "%s", E.repos[i].r.url);
    pthread_mutex_unlock(&trava);
    for (i = 0; i < n && gerAtual() == ger; i++) atualizarRepo(urls[i], ger, forcar);
    // Conta/perfil trocou no meio: relê a lista nova antes de sair.
    if (gerAtual() == ger) break;
  }
  free(urls);
  __atomic_store_n(&atualizando, 0, __ATOMIC_RELEASE);
  fflush(stdout);
  return NULL;
}
void plugins_atualizar(void) {
  pthread_t f;
  pthread_attr_t a;
  if (!plugins_disponivel() || __atomic_exchange_n(&atualizando, 1, __ATOMIC_ACQ_REL)) return;
  pthread_attr_init(&a);
  pthread_attr_setstacksize(&a, 256 * 1024);
  if (pthread_create(&f, &a, fioAtualizar, NULL) == 0) pthread_detach(f);
  else __atomic_store_n(&atualizando, 0, __ATOMIC_RELEASE);   // sem fio: nao bloqueia a UI
  pthread_attr_destroy(&a);
}
int plugins_atualizando(void) { return __atomic_load_n(&atualizando, __ATOMIC_ACQUIRE); }

// ------------------------------------------------------------ repositorios

int plugins_n_repos(void) { int n; pthread_mutex_lock(&trava); n = E.nRepos; pthread_mutex_unlock(&trava); return n; }
int plugins_repo(int i, PlugRepo *s) {
  int ok = 0;
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < E.nRepos) { *s = E.repos[i].r; ok = 1; }
  pthread_mutex_unlock(&trava);
  return ok;
}
int plugins_repo_estado(int i) {
  int v = 0;
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < E.nRepos) v = E.repos[i].estado;
  pthread_mutex_unlock(&trava);
  return v;
}
int plugins_repo_scrapers(int i, int *lig) {
  int k, n = 0, l = 0;
  pthread_mutex_lock(&trava);
  for (k = 0; k < E.nScr; k++) if (E.scr[k].repo == i) { n++; if (E.scr[k].ativo) l++; }
  pthread_mutex_unlock(&trava);
  if (lig) *lig = l;
  return n;
}
static Scraper *scraperDoRepo(int i, int j) {
  int k, q = 0;
  for (k = 0; k < E.nScr; k++) if (E.scr[k].repo == i && q++ == j) return &E.scr[k];
  return NULL;
}
int plugins_scraper(int i, int j, PlugScraper *saida) {
  Scraper *s;
  int ok = 0;
  pthread_mutex_lock(&trava);
  if ((s = scraperDoRepo(i, j)) != NULL) {
    memset(saida, 0, sizeof *saida);
    snprintf(saida->id, sizeof saida->id, "%s", s->id);
    snprintf(saida->nome, sizeof saida->nome, "%s", s->nome);
    saida->filme = s->filme; saida->serie = s->serie; saida->ativo = s->ativo;
    ok = 1;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}
int plugins_alternar_scraper(int i, int j) {
  Scraper *s;
  int v = -1;
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < E.nRepos && (s = scraperDoRepo(i, j)) != NULL) {
    char man[700];
    urlManifesto(E.repos[i].r.url, man, sizeof man);
    s->ativo = !s->ativo; v = s->ativo;
    lembrarEscolha(&E, man, s->id, v);
    soltarEGravar();
    return v;
  }
  pthread_mutex_unlock(&trava);
  return v;
}

// Edicao de repositorio: avanca a revisao e marca pendente para o sync.
static void editou(void) { E.rev++; E.pendente = 1; }

int plugins_alternar_repo(int i) {
  int v = 0;
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < E.nRepos) {
    E.repos[i].r.ativo = !E.repos[i].r.ativo; v = E.repos[i].r.ativo;
    editou();
    soltarEGravar();
    return v;
  }
  pthread_mutex_unlock(&trava);
  return v;
}
int plugins_remover_repo(int i) {
  int k, b = 0;
  pthread_mutex_lock(&trava);
  if (i < 0 || i >= E.nRepos) { pthread_mutex_unlock(&trava); return 0; }
  for (k = 0; k < E.nScr; k++)
    if (E.scr[k].repo != i) { E.scr[b] = E.scr[k]; if (E.scr[b].repo > i) E.scr[b].repo--; b++; }
  E.nScr = b;
  for (k = i; k < E.nRepos - 1; k++) E.repos[k] = E.repos[k + 1];
  E.nRepos--;
  editou();   // inclusive o ULTIMO: a lista vazia sobe (ver plugins.h)
  soltarEGravar();
  return 1;
}
int plugins_adicionar_repo(const char *url) {
  char man[700];
  int i;
  if (!ehHttp(url) || strlen(url) >= 590 || strpbrk(url, " \t\r\n")) return 0;
  urlManifesto(url, man, sizeof man);
  pthread_mutex_lock(&trava);
  for (i = 0; i < E.nRepos; i++) {
    char m2[700]; urlManifesto(E.repos[i].r.url, m2, sizeof m2);
    if (!strcmp(m2, man)) { pthread_mutex_unlock(&trava); return -1; }
  }
  if (E.nRepos >= PLUG_REPOS_MAX) { pthread_mutex_unlock(&trava); return -2; }
  memset(&E.repos[E.nRepos], 0, sizeof E.repos[E.nRepos]);
  snprintf(E.repos[E.nRepos].r.url, sizeof E.repos[E.nRepos].r.url, "%s", man);
  snprintf(E.repos[E.nRepos].r.tipo, sizeof E.repos[E.nRepos].r.tipo, "NUVIO_JS");
  E.repos[E.nRepos].r.ativo = 1;
  E.nRepos++;
  editou();
  soltarEGravar();
  plugins_atualizar();
  return 1;
}

// ------------------------------------------------------------ sync (ACK)

void plugins_retrato(PlugRetrato *s) {
  int i;
  memset(s, 0, sizeof *s);
  pthread_mutex_lock(&trava);
  for (i = 0; i < E.nRepos && i < PLUG_REPOS_MAX; i++) s->lista[i] = E.repos[i].r;
  s->n = i;
  s->rev = E.rev; s->pendente = E.pendente;
  s->geracao = gerAtual();
  pthread_mutex_unlock(&trava);
}
int plugins_pendente(void) { int p; pthread_mutex_lock(&trava); p = E.pendente; pthread_mutex_unlock(&trava); return p; }

int plugins_confirmar(unsigned rev, unsigned ger) {
  pthread_mutex_lock(&trava);
  if (ger != gerAtual() || rev != E.rev || !E.pendente) { pthread_mutex_unlock(&trava); return 0; }
  E.pendente = 0;
  soltarEGravar();
  return 1;
}

int plugins_definir_da_conta(const PlugRepo *l, int n, unsigned ger) {
  int i, mudou = 0, atualizar;
  if (n < 0 || (n > 0 && !l)) return 0;
  // PORTA DE TESTE (como /tmp/nuvio-key): com /tmp/nuvio-plugins-local a lista
  // deste aparelho fica e a da conta nao a substitui.
  { FILE *t = fopen("/tmp/nuvio-plugins-local", "r");
    if (t) { fclose(t); printf("[plugins] /tmp/nuvio-plugins-local: a lista da conta (%d) nao substitui a local\n", n); return 0; } }
  if (n > PLUG_REPOS_MAX) n = PLUG_REPOS_MAX;
  pthread_mutex_lock(&trava);
  // Geracao velha (conta/perfil trocou durante a leitura) ou edicao local
  // ainda nao confirmada: a lista da conta nao passa por cima.
  if (ger != gerAtual() || E.pendente) { pthread_mutex_unlock(&trava); return 0; }
  if (n != E.nRepos) mudou = 1;
  for (i = 0; i < n && !mudou; i++)
    if (strcmp(l[i].url, E.repos[i].r.url) || l[i].ativo != E.repos[i].r.ativo) mudou = 1;
  if (!mudou) { pthread_mutex_unlock(&trava); return 0; }
  E.nRepos = 0; E.nScr = 0;
  for (i = 0; i < n; i++) {
    if (!ehHttp(l[i].url)) continue;
    memset(&E.repos[E.nRepos], 0, sizeof E.repos[E.nRepos]);
    E.repos[E.nRepos].r = l[i];
    E.repos[E.nRepos].r.url[sizeof E.repos[0].r.url - 1] = 0;
    E.repos[E.nRepos].estado = naoRoda(&l[i]) ? 2 : 0;
    E.nRepos++;
  }
  atualizar = E.ligado && E.nRepos > 0;
  soltarEGravar();
  printf("[plugins] %d repositorio(s) vindos da conta\n", n);
  // Manifestos (cache primeiro, depois rede) num fio: nada de disco aqui.
  if (atualizar) plugins_atualizar();
  return 1;
}

int plugins_ler_conta(const char *json, PlugRepo *l, int max) {
  const char *p;
  int k = 0;
  if (!json) return -1;
  for (p = json; *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'; p++) {}
  if (*p != '[') return -1;
  for (p++; *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'; p++) {}
  if (*p == ']') return 0;                       // array vazio: lista vazia VALIDA
  if (*p != '{') return -1;
  for (p = js_raiz_array(json); p && k < max; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char b[16];
    if (!f) return -1;                           // truncado: nao e lista
    memset(&l[k], 0, sizeof l[k]);
    if (!js_texto(p, f, "url", l[k].url, sizeof l[k].url) || !ehHttp(l[k].url)) continue;
    js_texto(p, f, "name", l[k].nome, sizeof l[k].nome);
    js_texto(p, f, "repo_type", l[k].tipo, sizeof l[k].tipo);
    l[k].ativo = js_bruto(p, f, "enabled", b, sizeof b) ? (strcmp(b, "false") != 0) : 1;
    k++;
  }
  return k;
}

void plugins_esquecer(void) {
  int p;
  char nome[96], usuario[80];
  gerAvancar();
  pthread_mutex_lock(&trava);
  snprintf(usuario, sizeof usuario, "%s", ctxUsuario);
  memset(&E, 0, sizeof E);
  ctxPerfil = -1; ctxUsuario[0] = 0;
  __atomic_store_n(&ligadoAt, 0, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&trava);
  // O estado era da conta que saiu: apaga o de todos os perfis dela.
  if (usuario[0])
    for (p = 1; p <= 32; p++) { nomeEstado(usuario, p, nome, sizeof nome); dados_apagar(nome); }
}

// ------------------------------------------------------------ tmdb

typedef struct { char imdb[24]; long tmdb; int serie; } TmC;
static TmC tmCache[64];
static int tmProx;

struct PluginsConsulta {
  PlugRepo repos[PLUG_REPOS_MAX]; int nRepos;
  Scraper scr[PLUG_SCRAPERS_MAX]; int nScr;
  TmC tmdb[64];
  time_t hits[PLUG_SCRAPERS_MAX];
  char chaveTmdb[64], usuario[80], so[400];
  int perfil, ligado, disponivel, log;
  unsigned geracao;
  uint64_t sessao;
  DebridContexto debrid;
  RedeGrupo *grupo;
  RedeJob *job;
};
PluginsConsulta *plugins_consulta_capturar(void) {
  PluginsConsulta *c = calloc(1, sizeof *c);
  if (!c) return NULL;
  c->sessao = nv_perfil_sessao_ler();
  c->disponivel = plugins_disponivel();
  snprintf(c->chaveTmdb, sizeof c->chaveTmdb, "%s", desc_chave_tmdb_reserva());
  { const char *so = getenv("NUVIO_PLUGINS_SO");
    snprintf(c->so, sizeof c->so, "%s", so ? so : ""); }
  c->log = getenv("NUVIO_PLUGINS_LOG") != NULL;
  debrid_contexto_capturar(&c->debrid);
  pthread_mutex_lock(&trava);
  c->geracao = gerAtual(); c->perfil = ctxPerfil; c->ligado = E.ligado;
  snprintf(c->usuario, sizeof c->usuario, "%s", ctxUsuario);
  c->nRepos = E.nRepos; c->nScr = E.nScr;
  for (int i = 0; i < c->nRepos; i++) c->repos[i] = E.repos[i].r;
  memcpy(c->scr, E.scr, (size_t)c->nScr * sizeof *c->scr);
  for (int i = 0; i < c->nScr; i++) {
    char man[700]; urlManifesto(E.repos[E.scr[i].repo].r.url, man, sizeof man);
    const Escolha *e = escolhaDe(&E, man, E.scr[i].id);
    if (e) c->hits[i] = e->ultimoHit;
  }
  memcpy(c->tmdb, tmCache, sizeof c->tmdb);
  c->grupo = grupoDe(); c->job = rede_job_criar(c->grupo);
  pthread_mutex_unlock(&trava);
  if (!c->job || !nv_perfil_sessao_atual(c->sessao)) { plugins_consulta_liberar(c); return NULL; }
  return c;
}
int plugins_consulta_atual(const PluginsConsulta *c) {
  if (!c || !nv_perfil_sessao_atual(c->sessao) || rede_job_estado(c->job) != REDE_OK ||
      c->disponivel != plugins_disponivel() || strcmp(c->chaveTmdb, desc_chave_tmdb_reserva())) return 0;
  DebridContexto debrid;
  debrid_contexto_capturar(&debrid);
  if (c->debrid.sessao != debrid.sessao || memcmp(c->debrid.chave, debrid.chave, sizeof debrid.chave)) return 0;
  int ok;
  pthread_mutex_lock(&trava);
  ok = c->geracao == gerAtual() && c->perfil == ctxPerfil && !strcmp(c->usuario, ctxUsuario) &&
       c->ligado == E.ligado && c->nRepos == E.nRepos && c->nScr == E.nScr &&
       !memcmp(c->scr, E.scr, (size_t)c->nScr * sizeof *c->scr);
  for (int i = 0; ok && i < c->nRepos; i++)
    if (memcmp(&c->repos[i], &E.repos[i].r, sizeof c->repos[i])) ok = 0;
  pthread_mutex_unlock(&trava);
  return ok;
}
void plugins_consulta_liberar(PluginsConsulta *c) {
  if (!c) return;
  rede_job_soltar(c->job);
  memset(c, 0, sizeof *c); free(c);
}
static const char *espacoCapturado(const char *p) {
  while (*p && (unsigned char)*p <= ' ') p++;
  return p;
}
static int arrayCapturadoValido(const char *json, const char *chave) {
  if (!json) return 0;
  const char *ini = espacoCapturado(json), *fim = js_fim(ini);
  if (*ini != '{' || !fim || fim <= ini || fim[-1] != '}' || *espacoCapturado(fim)) return 0;
  int profundidade = 0;
  size_t nChave = strlen(chave);
  for (const char *p = ini; p < fim; p++) {
    if (*p == '"') {
      const char *nome = p + 1, *fecha = nome;
      while (fecha < fim && *fecha != '"') {
        if (*fecha == '\\' && fecha + 1 < fim) fecha++;
        fecha++;
      }
      if (fecha >= fim) return 0;
      const char *v = espacoCapturado(fecha + 1);
      if (profundidade == 1 && (size_t)(fecha - nome) == nChave && !memcmp(nome, chave, nChave) && *v == ':') {
        v = espacoCapturado(v + 1); const char *af = js_fim(v);
        if (*v != '[' || !af || af <= v || af > fim || af[-1] != ']') return 0;
        const char *q = espacoCapturado(v + 1);
        while (q < af - 1) {
          const char *of = js_fim(q);
          if (*q != '{' || !of || of <= q || of > af - 1 || of[-1] != '}') return 0;
          q = espacoCapturado(of);
          if (q == af - 1) break;
          if (*q != ',') return 0;
          q = espacoCapturado(q + 1); if (q == af - 1) return 0;
        }
        return q == af - 1;
      }
      p = fecha;
    } else if (*p == '{' || *p == '[') profundidade++;
    else if (*p == '}' || *p == ']') profundidade--;
  }
  return 0;
}
static long tmdbDeCapturado(const PluginsConsulta *c, const char *imdb, int serie, int *completo) {
  char url[300], *corpo;
  for (int i = 0; i < 64; i++)
    if (c->tmdb[i].tmdb && c->tmdb[i].serie == serie && !strcmp(c->tmdb[i].imdb, imdb)) return c->tmdb[i].tmdb;
  if (!c->chaveTmdb[0]) return 0;
  snprintf(url, sizeof url, "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id", imdb, c->chaveTmdb);
  corpo = baixar(url, 256 * 1024, 10000, c->job);
  if (!corpo) { *completo = 0; return 0; }
  const char *p = js_array(corpo, NULL, serie ? "tv_results" : "movie_results");
  if (!arrayCapturadoValido(corpo, serie ? "tv_results" : "movie_results")) *completo = 0;
  long id = p ? (long)js_num(p, js_fim(p), "id", 0) : 0;
  free(corpo); return id;
}

static long tmdbDe(const char *imdb, int serie, RedeJob *job) {
  char url[300], *corpo;
  const char *chave = desc_chave_tmdb_reserva();
  long id = 0;
  int k;
  pthread_mutex_lock(&trava);
  for (k = 0; k < 64; k++)
    if (tmCache[k].tmdb && tmCache[k].serie == serie && !strcmp(tmCache[k].imdb, imdb)) { id = tmCache[k].tmdb; break; }
  pthread_mutex_unlock(&trava);
  if (id || !chave || !chave[0]) return id;
  snprintf(url, sizeof url, "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id", imdb, chave);
  corpo = baixar(url, 256 * 1024, 10000, job);   // a URL leva a chave: nunca logar
  if (corpo) {
    const char *p = js_array(corpo, NULL, serie ? "tv_results" : "movie_results");
    if (p) id = (long)js_num(p, js_fim(p), "id", 0);
    free(corpo);
  }
  if (id) {
    pthread_mutex_lock(&trava);
    snprintf(tmCache[tmProx].imdb, sizeof tmCache[tmProx].imdb, "%s", imdb);
    tmCache[tmProx].tmdb = id; tmCache[tmProx].serie = serie;
    tmProx = (tmProx + 1) % 64;
    pthread_mutex_unlock(&trava);
  }
  return id;
}

// ------------------------------------------------------------ consulta

void plugins_resumo(const char *id, const char *tipo, PlugResumo *saida) {
  *saida = (PlugResumo){0};
  if (!plugins_ligado() || !id || !tipo) return;
  pthread_mutex_lock(&trava);
  if (resumoGeracao == gerAtual() && !strcmp(resumoId, id) && !strcmp(resumoTipo, tipo))
    *saida = ultimoResumo;
  else for (int k = 0; k < E.nScr; k++) {
    const Scraper *s = &E.scr[k];
    if (s->ativo && s->repo >= 0 && s->repo < E.nRepos && E.repos[s->repo].r.ativo &&
        (!strcmp(tipo, "movie") ? s->filme :
         (!strcmp(tipo, "series") || !strcmp(tipo, "anime")) && s->serie))
      saida->enfileirados++;
  }
  pthread_mutex_unlock(&trava);
}

static int tituloAnime(const char *id, const char *tipo) {
  if (!strcmp(tipo, "anime")) return 1;
  CatItem *item = malloc(sizeof *item);
  int anime = item && cat_copiar_por_id(id, tipo, item) &&
    (strstr(item->genero, "Anime") || strstr(item->genero, "anime") ||
     ((strstr(item->genero, "Animation") || strstr(item->genero, "Animação")) &&
      (strstr(item->pais, "Japan") || strstr(item->pais, "Japão"))));
  free(item);
  return anime;
}

static void ordenarScrapers(Scraper *lista, int n, int anime) {
  time_t agora = time(NULL);
  for (int i = 0; i < n; i++)
    if (lista[i].ultimoHit > agora || agora - lista[i].ultimoHit > PLUG_HIT_S)
      lista[i].ultimoHit = 0;
  // Stable insertion sort; at most PLUG_SCRAPERS_MAX entries.
  for (int i = 1; i < n; i++) {
    Scraper s = lista[i];
    int j = i, ultimo = !anime && s.anime;
    while (j > 0) {
      const Scraper *p = &lista[j - 1];
      int pUltimo = !anime && p->anime;
      if (pUltimo < ultimo || (pUltimo == ultimo && p->ultimoHit >= s.ultimoHit)) break;
      lista[j] = *p; j--;
    }
    lista[j] = s;
  }
}

typedef void (*AvisoFn)(void *u, int k, const char *nome, const char *id,
                        int estado, const void *fontes, int n);
typedef struct {
  Scraper *lista; int n, prox;
  char tmdb[24]; const char *tipo; int t, e;
  int cancel;              // atomico: 1 = descarta tudo; 2 = corte (fica o que terminou)
  int vivos, terminados;
  int prazoMs;             // Short slots for large installed pools.
  pthread_mutex_t m;
  Stream **res; int *nRes;
  AvisoFn aviso; void *avisoU;
  RedeJob *job;            // geracao desta consulta
  const PluginsConsulta *capturada;
  DebridContexto debrid;
  int incompleta;          // atomico: o preparo nao aceita resposta parcial
} Rodada;

static int rodadaParou(Rodada *r) {
  return __atomic_load_n(&r->cancel, __ATOMIC_ACQUIRE) || rede_job_estado(r->job) != REDE_OK;
}

static char *codigoDe(const Scraper *s, RedeJob *job, int prazoMs) {
  long idade;
  char *t = cacheLer(s->url, &idade), *corpo;
  if (t && idade >= 0 && idade < PLUG_CACHE_S) return t;
  corpo = baixar(s->url, PLUG_CODIGO_MAX, prazoMs, job);
  if (corpo && strlen(corpo) > 20) {
    if (rede_job_estado(job) == REDE_OK) cacheGravar(s->url, corpo);
    free(t);
    return corpo;
  }
  free(corpo);
  return t;   // a rede falhou: fica o codigo velho, se houver
}
static char *codigoDeCapturado(const Scraper *s, RedeJob *job, int prazoMs) {
  long idade;
  char *t = cacheLer(s->url, &idade);
  if (t && idade >= 0 && idade < PLUG_CACHE_S) return t;
  char *corpo = baixar(s->url, PLUG_CODIGO_MAX, prazoMs, job);
  if (corpo && strlen(corpo) > 20) { free(t); return corpo; }
  free(corpo); return t;
}

static void rodarScrapers(Rodada *r) {
  for (;;) {
    int i;
    Scraper *s;
    char *cod;
    PjPedido p;
    PjResultado res;
    pthread_mutex_lock(&r->m);
    i = r->prox < r->n ? r->prox++ : -1;
    pthread_mutex_unlock(&r->m);
    if (i < 0 || rodadaParou(r)) return;
    s = &r->lista[i];
    cod = r->capturada ? codigoDeCapturado(s, r->job, r->prazoMs) : codigoDe(s, r->job, r->prazoMs);
    if (!cod) {
      if (r->capturada) __atomic_store_n(&r->incompleta, 1, __ATOMIC_RELEASE);
      if (!rodadaParou(r)) __atomic_add_fetch(&r->terminados, 1, __ATOMIC_RELAXED);
      printf("[plugins] %s: sem codigo\n", s->nome);
      if (r->aviso && !rodadaParou(r)) r->aviso(r->avisoU, i, s->nome, s->id, 3, NULL, 0);
      continue;
    }
    memset(&p, 0, sizeof p);
    p.codigo = semCarimbo(cod); p.nCodigo = strlen(p.codigo);
    p.arquivo = s->arquivo; p.idScraper = s->id; p.nomeScraper = s->nome;
    p.tmdbId = r->tmdb; p.tipo = r->tipo; p.temporada = r->t; p.episodio = r->e;
    p.tmdbChave = r->capturada ? r->capturada->chaveTmdb : desc_chave_tmdb_reserva();
    p.ajustesJson = "{}";
    p.prazoMs = r->prazoMs; p.redeSegundos = r->prazoMs < 15000 ? r->prazoMs / 1000 : PLUG_REDE_S;
    // Eight 4 MiB stacks; JS heap/network retain the existing global caps
    // (64/16 MiB on webOS). No increase to the TV memory budget.
    p.memoriaMax = r->n > 16 ? 8L * 1024 * 1024 : 0;
    p.verLog = r->capturada ? r->capturada->log : getenv("NUVIO_PLUGINS_LOG") != NULL;
    p.cancelado = &r->cancel;
    p.job = r->job; p.grupo = r->capturada ? r->capturada->grupo : grupoDe();
    pj_executar(&p, &res);
    if (r->capturada && (!arrayCapturadoValido(res.json, "streams") || res.cancelado || res.estourou || res.semMemoria || res.erro[0]))
      __atomic_store_n(&r->incompleta, 1, __ATOMIC_RELEASE);
    free(cod);
    if (!res.cancelado && !rodadaParou(r)) __atomic_add_fetch(&r->terminados, 1, __ATOMIC_RELAXED);
    { Stream *l = NULL;
      int n = res.json && !res.cancelado ? stream_extrair(res.json, s->nome, &l) : 0;
      if (n < 0) { if (r->capturada) __atomic_store_n(&r->incompleta, 1, __ATOMIC_RELEASE); n = 0; }
      r->res[i] = l; r->nRes[i] = n;
      if (r->aviso && __atomic_load_n(&r->cancel, __ATOMIC_ACQUIRE) != 1 && rede_job_estado(r->job) == REDE_OK)
        r->aviso(r->avisoU, i, s->nome, s->id, n > 0 || !res.erro[0] ? 2 : 3, l, n);
      printf("[plugins] %s: %d fontes (%lu ms, pico %zu KB, %d fetch%s%s%s)\n", s->nome, n, res.ms,
             res.memPico / 1024, res.fetches, res.fetches == 1 ? "" : "es",
             res.erro[0] ? ", " : "", res.erro);
      fflush(stdout); }
    pj_resultado_soltar(&res);
  }
}
static void *fioScraper(void *u) {
  Rodada *r = u;
  if (r->capturada) debrid_contexto_entrar(&r->debrid);
  rodarScrapers(r);
  if (r->capturada) debrid_contexto_sair();
  pthread_mutex_lock(&r->m); r->vivos--; pthread_mutex_unlock(&r->m);
  return NULL;
}

static unsigned long agoraMsPlug(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}

int plugins_consultar(const char *id, const char *tipo, int (*cancelado)(void *), void *ctx,
                      AvisoFn aviso, void *avisoU, Stream **saida) {
  unsigned long t0 = agoraMsPlug();
  Rodada r;
  char base[64];
  int serie, k, total = 0, fios, criados = 0, descartar;
  unsigned ger = gerAtual();
  pthread_t f[PLUG_PARALELOS];
  Stream *todos = NULL;
  *saida = NULL;
  if (!plugins_ligado() || !id || !tipo) return 0;
  serie = !strcmp(tipo, "series") || !strcmp(tipo, "anime");
  if (!serie && strcmp(tipo, "movie")) return 0;
  memset(&r, 0, sizeof r);
  r.job = rede_job_criar(grupoDe());
  if (!r.job) return 0;
  if (!strncmp(id, "tmdb:", 5)) {
    snprintf(r.tmdb, sizeof r.tmdb, "%ld", atol(id + 5));
    { const char *q = strchr(id + 5, ':'); if (q) { r.t = atoi(q + 1); q = strchr(q + 1, ':'); if (q) r.e = atoi(q + 1); } }
  } else if (idbase_e_imdb(id)) {
    long tm;
    idbase_copiar(id, base, sizeof base);
    idbase_episodio(id, &r.t, &r.e);
    tm = tmdbDe(base, serie, r.job);
    if (!tm) { printf("[plugins] %s: sem id do TMDB, plugins fora\n", base); rede_job_soltar(r.job); return 0; }
    snprintf(r.tmdb, sizeof r.tmdb, "%ld", tm);
  } else { rede_job_soltar(r.job); return 0; }   // kitsu:, mal: ... ficam para depois
  if (serie && (r.t <= 0 || r.e <= 0)) { r.t = r.t > 0 ? r.t : 1; r.e = r.e > 0 ? r.e : 1; }
  if (!serie) r.t = r.e = 0;
  r.tipo = serie ? "tv" : "movie";

  pthread_mutex_lock(&trava);
  r.lista = malloc(sizeof(Scraper) * (size_t)(E.nScr ? E.nScr : 1));
  for (k = 0; r.lista && k < E.nScr; k++) {
    Scraper *s = &E.scr[k];
    if (!s->ativo || s->repo >= E.nRepos || !E.repos[s->repo].r.ativo) continue;
    if (serie ? !s->serie : !s->filme) continue;
    { const char *so = getenv("NUVIO_PLUGINS_SO");     // depuracao: so esses ids
      if (so && *so) { char alvo[80], lista[400];
        snprintf(alvo, sizeof alvo, ",%s,", s->id); snprintf(lista, sizeof lista, ",%s,", so);
        if (!strstr(lista, alvo)) continue; } }
    r.lista[r.n] = *s;
    char man[700]; urlManifesto(E.repos[s->repo].r.url, man, sizeof man);
    const Escolha *e = escolhaDe(&E, man, s->id);
    if (e) r.lista[r.n].ultimoHit = e->ultimoHit;
    r.n++;
  }
  if (gerAtual() == ger) {
    ultimoResumo = (PlugResumo){ .enfileirados = r.n };
    snprintf(resumoId, sizeof resumoId, "%s", id);
    snprintf(resumoTipo, sizeof resumoTipo, "%s", tipo);
    resumoGeracao = ger;
  }
  pthread_mutex_unlock(&trava);
  ordenarScrapers(r.lista, r.n, tituloAnime(id, tipo));
  if (!r.n) { free(r.lista); rede_job_soltar(r.job); return 0; }
  r.res = calloc((size_t)r.n, sizeof(Stream *));
  r.nRes = calloc((size_t)r.n, sizeof(int));
  if (!r.res || !r.nRes) { free(r.lista); free(r.res); free(r.nRes); rede_job_soltar(r.job); return 0; }
  pthread_mutex_init(&r.m, NULL);
  r.aviso = aviso; r.avisoU = avisoU;
  if (aviso) for (k = 0; k < r.n; k++) aviso(avisoU, k, r.lista[k].nome, r.lista[k].id, 1, NULL, 0);
  printf("[plugins] %s %s (tmdb %s): %d scraper(s)\n", serie ? "serie" : "filme", id, r.tmdb, r.n);
  fflush(stdout);

  r.prazoMs = r.n > 16 ? 3000 : PLUG_PRAZO_MS;
  fios = r.n < PLUG_PARALELOS ? r.n : PLUG_PARALELOS;
  { pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, pj_pilha());
    for (k = 0; k < fios; k++) {
      pthread_mutex_lock(&r.m); r.vivos++; pthread_mutex_unlock(&r.m);
      if (pthread_create(&f[criados], &a, fioScraper, &r) == 0) criados++;
      else { pthread_mutex_lock(&r.m); r.vivos--; pthread_mutex_unlock(&r.m); }
    }
    pthread_attr_destroy(&a); }
  // Sem fio com a pilha do motor nao roda inline: a pilha deste fio e menor.
  if (!criados) printf("[plugins] sem fio para os scrapers\n");
  for (;;) {
    int fim;
    struct timespec ts = { 0, 50 * 1000000L };
    pthread_mutex_lock(&r.m); fim = r.vivos <= 0; pthread_mutex_unlock(&r.m);
    if (fim) break;
    if (cancelado) { int c = cancelado(ctx); if (c) __atomic_store_n(&r.cancel, c == 2 ? 2 : 1, __ATOMIC_RELEASE); }
    if (!__atomic_load_n(&r.cancel, __ATOMIC_ACQUIRE) && agoraMsPlug() - t0 > PLUG_TETO_MS)
      __atomic_store_n(&r.cancel, 2, __ATOMIC_RELEASE);
    nanosleep(&ts, NULL);
  }
  for (k = 0; k < criados; k++) pthread_join(f[k], NULL);
  descartar = __atomic_load_n(&r.cancel, __ATOMIC_ACQUIRE) == 1 || rede_job_estado(r.job) != REDE_OK;
  for (k = 0; k < r.n; k++) {
    if (r.nRes[k] > 0 && !descartar) {
      Stream *x = realloc(todos, sizeof(Stream) * (size_t)(total + r.nRes[k]));
      if (x) { todos = x; memcpy(todos + total, r.res[k], sizeof(Stream) * (size_t)r.nRes[k]); total += r.nRes[k]; }
    }
    free(r.res[k]);
  }
  pthread_mutex_lock(&trava);
  if (!descartar && gerAtual() == ger) {
    ultimoResumo = (PlugResumo){r.n, r.terminados, r.n - r.terminados};
    int mudou = 0;
    for (k = 0; k < r.n; k++) if (r.nRes[k] > 0)
      for (int j = 0; j < E.nScr; j++) {
        const Scraper *s = &E.scr[j];
        if (strcmp(s->id, r.lista[k].id) || strcmp(s->url, r.lista[k].url)) continue;
        char man[700]; urlManifesto(E.repos[s->repo].r.url, man, sizeof man);
        lembrarEscolha(&E, man, s->id, s->ativo);
        escolhaDe(&E, man, s->id)->ultimoHit = time(NULL);
        mudou = 1; break;
      }
    if (mudou) soltarEGravar(); else pthread_mutex_unlock(&trava);
  } else pthread_mutex_unlock(&trava);
  free(r.res); free(r.nRes); free(r.lista);
  pthread_mutex_destroy(&r.m);
  rede_job_soltar(r.job);
  if (descartar) { free(todos); printf("[plugins] consulta descartada (cancelada ou perfil/conta trocados)\n"); return -1; }
  if (r.cancel == 2) printf("[plugins] corte aos %lu ms: fica o que terminou\n", agoraMsPlug() - t0);
  printf("[plugins] total %d\n", total);
  fflush(stdout);
  *saida = todos;
  return total;
}

int plugins_consulta_consultar(const PluginsConsulta *c, const char *id, const char *tipo,
                              int (*cancelado)(void *), void *ctx,
                              AvisoFn aviso, void *avisoU, Stream **saida) {
  if (!saida) return -1;
  *saida = NULL;
  if (!c || !id || !*id || !tipo || !*tipo || !plugins_consulta_atual(c) ||
      (cancelado && cancelado(ctx))) return -1;
  if (!c->ligado || !c->disponivel) return 0;
  int serie = !strcmp(tipo, "series") || !strcmp(tipo, "anime");
  if (!serie && strcmp(tipo, "movie")) return 0;
  Rodada r = {0};
  r.capturada = c; r.job = c->job; r.debrid = c->debrid;
  r.lista = malloc(sizeof *r.lista * (size_t)(c->nScr ? c->nScr : 1));
  if (!r.lista) return -1;
  for (int i = 0; i < c->nScr; i++) {
    const Scraper *s = &c->scr[i];
    if (!s->ativo || s->repo < 0 || s->repo >= c->nRepos || !c->repos[s->repo].ativo ||
        (serie ? !s->serie : !s->filme)) continue;
    if (c->so[0]) { char alvo[80], lista[sizeof c->so + 3];
      snprintf(alvo, sizeof alvo, ",%s,", s->id); snprintf(lista, sizeof lista, ",%s,", c->so);
      if (!strstr(lista, alvo)) continue; }
    r.lista[r.n] = *s;
    r.lista[r.n++].ultimoHit = c->hits[i];
  }
  ordenarScrapers(r.lista, r.n, tituloAnime(id, tipo));
  if (!r.n) { free(r.lista); return 0; }
  char base[64]; int completo = 1;
  if (!strncmp(id, "tmdb:", 5)) {
    snprintf(r.tmdb, sizeof r.tmdb, "%ld", atol(id + 5));
    const char *q = strchr(id + 5, ':');
    if (q) { r.t = atoi(q + 1); q = strchr(q + 1, ':'); if (q) r.e = atoi(q + 1); }
  } else if (idbase_e_imdb(id)) {
    idbase_copiar(id, base, sizeof base); idbase_episodio(id, &r.t, &r.e);
    long tm = tmdbDeCapturado(c, base, serie, &completo);
    if (!tm) { free(r.lista); return completo && plugins_consulta_atual(c) &&
        !(cancelado && cancelado(ctx)) ? 0 : -1; }
    snprintf(r.tmdb, sizeof r.tmdb, "%ld", tm);
  } else { free(r.lista); return 0; }
  if (serie) { r.t = r.t > 0 ? r.t : 1; r.e = r.e > 0 ? r.e : 1; }
  else r.t = r.e = 0;
  r.debrid.temporada = r.t; r.debrid.episodio = r.e;
  r.tipo = serie ? "tv" : "movie";
  r.res = calloc((size_t)r.n, sizeof *r.res);
  r.nRes = calloc((size_t)r.n, sizeof *r.nRes);
  if (!r.res || !r.nRes) { free(r.lista); free(r.res); free(r.nRes); return -1; }
  pthread_mutex_init(&r.m, NULL); r.aviso = aviso; r.avisoU = avisoU;
  if (aviso) for (int i = 0; i < r.n; i++) aviso(avisoU, i, r.lista[i].nome, r.lista[i].id, 1, NULL, 0);
  unsigned long inicio = agoraMsPlug();
  r.prazoMs = r.n > 16 ? 3000 : PLUG_PRAZO_MS;
  pthread_t fios[PLUG_PARALELOS]; int criados = 0;
  int nf = r.n < PLUG_PARALELOS ? r.n : PLUG_PARALELOS;
  pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, pj_pilha());
  for (int i = 0; i < nf; i++) {
    pthread_mutex_lock(&r.m); r.vivos++; pthread_mutex_unlock(&r.m);
    if (!pthread_create(&fios[criados], &a, fioScraper, &r)) criados++;
    else { pthread_mutex_lock(&r.m); r.vivos--; pthread_mutex_unlock(&r.m); }
  }
  pthread_attr_destroy(&a);
  if (!criados) __atomic_store_n(&r.incompleta, 1, __ATOMIC_RELEASE);
  for (;;) {
    pthread_mutex_lock(&r.m); int fim = r.vivos <= 0; pthread_mutex_unlock(&r.m);
    if (fim) break;
    if (!plugins_consulta_atual(c) || (cancelado && cancelado(ctx)))
      __atomic_store_n(&r.cancel, 1, __ATOMIC_RELEASE);
    if (agoraMsPlug() - inicio > PLUG_TETO_MS)
      __atomic_store_n(&r.cancel, 1, __ATOMIC_RELEASE);
    struct timespec espera = {0, 50 * 1000000L}; nanosleep(&espera, NULL);
  }
  for (int i = 0; i < criados; i++) pthread_join(fios[i], NULL);
  int falhou = __atomic_load_n(&r.cancel, __ATOMIC_ACQUIRE) ||
               __atomic_load_n(&r.incompleta, __ATOMIC_ACQUIRE) ||
               !plugins_consulta_atual(c) || (cancelado && cancelado(ctx));
  int total = 0;
  for (int i = 0; i < r.n; i++) if (r.nRes[i] > 0) total += r.nRes[i];
  Stream *todos = !falhou && total ? malloc((size_t)total * sizeof *todos) : NULL;
  if (total && !todos) falhou = 1;
  int n = 0;
  for (int i = 0; i < r.n; i++) {
    if (!falhou && r.nRes[i] > 0) { memcpy(todos + n, r.res[i], (size_t)r.nRes[i] * sizeof *todos); n += r.nRes[i]; }
    free(r.res[i]);
  }
  free(r.lista); free(r.res); free(r.nRes); pthread_mutex_destroy(&r.m);
  if (falhou) { free(todos); return -1; }
  *saida = todos; return n;
}

static int origem(const char *id, const char *tipo, int (*cancelado)(void *), void *ctx,
                  OrigemAviso aviso, void *avisoU, void *saida) {
  return plugins_consultar(id, tipo, cancelado, ctx, aviso, avisoU, (Stream **)saida);
}
static void *preparoCapturar(void) { return plugins_consulta_capturar(); }
static int preparoAtual(const void *c) { return plugins_consulta_atual(c); }
static int preparoConsultar(const void *c, const char *id, const char *tipo,
                           int (*cancelado)(void *), void *ctx, OrigemAviso aviso,
                           void *avisoU, Stream **saida) {
  return plugins_consulta_consultar(c, id, tipo, cancelado, ctx, aviso, avisoU, saida);
}
static void preparoLiberar(void *c) { plugins_consulta_liberar(c); }
void plugins_ligar_aos_addons(void) {
  static const OrigemPreparoOps ops = {preparoCapturar, preparoAtual, preparoConsultar, preparoLiberar};
  addons_definir_origem_extra(origem, plugins_ligado);
  addons_definir_origem_extra_preparo(origem, &ops);
}
