// The array walk in jsraiz.h (elemento/depoisElemento) always terminates, through
// the real Plex and Jellyfin parsers: every case runs in a child under an alarm,
// so a hang is reported as a FAIL instead of blocking the suite.
#include "plex.h"
#include "jellyfin.h"
#include "guialembrete.h"
#include <SDL2/SDL.h>
#include "servidores.h"
#include "srvidx.h"
#include "stub_perfisessao.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PX_ITEM(guid) "{\"MediaContainer\":{\"size\":1,\"Metadata\":[{\"type\":\"movie\",\"ratingKey\":\"5\",\"Guid\":" guid "}]}}"
#define CASO(s) { s, sizeof s - 1 }

static const struct { const char *corpo; size_t n; } MALFORMADOS[] = {
  CASO(PX_ITEM("[1}]")),                  // the reported reproductions
  CASO(PX_ITEM("[{}6}]")),
  CASO(PX_ITEM("{}")),                    // valid JSON, object where an array belongs
  CASO(PX_ITEM("{\"id\":\"tmdb://9\"}")),
  CASO("{\"MediaContainer\":{\"Metadata\":[1}]}}"),
  CASO("{\"MediaContainer\":{\"Directory\":[1}]}}"),
  CASO("{\"MediaContainer\":{\"Metadata\":[{\"ratingKey\":\"5\",\"type\":\"movie\",\"Genre\":[1}],\"Image\":{}}]}}"),
  CASO("{\"MediaContainer\":{\"Metadata\":[{\"ratingKey\":\"5\",\"type\":\"movie\",\"Role\":[1}],\"Director\":{}}]}}"),
  CASO("{\"MediaContainer\":{\"Metadata\":[{\"ratingKey\":\"5\",\"Media\":[{\"Part\":[{\"Stream\":[1}]}]}]}]}}"),
  CASO("[1}]"),
  CASO("[\0x]"),                          // a NUL inside the body the caller still counts
  CASO("[{\"a\":1\0x]"),
  CASO("{\"Items\":[1}]}"),
  CASO("{\"Items\":{}}"),
  CASO("{\"Items\":[{\"Type\":\"Movie\",\"Id\":\"a\",\"Genres\":[1}],\"People\":{}}]}"),
  CASO("{\"People\":[1}],\"Items\":[1}]}"),
  CASO("{\"MediaSources\":[1}]}"),
  CASO("{\"MediaSources\":[{\"MediaStreams\":[1}]}]}"),
};

static const char PLEX[] =
  "{\"MediaContainer\":{\"size\":1,\"totalSize\":1,\"Directory\":[{\"key\":\"1\",\"type\":\"movie\",\"title\":\"D\"}],"
  "\"Metadata\":[{\"type\":\"movie\",\"ratingKey\":\"5\",\"title\":\"T\",\"parentIndex\":1,\"index\":1,"
  "\"Guid\":[{\"id\":\"imdb://tt1234567\"},{\"id\":\"tmdb://9\"}],\"Genre\":[{\"tag\":\"G\"}],"
  "\"Image\":[{\"type\":\"clearLogo\",\"url\":\"/l\"}],\"Role\":[{\"tag\":\"R\"}],\"Director\":[{\"tag\":\"D\"}],"
  "\"Media\":[{\"id\":1,\"height\":1080,\"Part\":[{\"key\":\"/k\",\"decision\":\"directplay\",\"Stream\":[{\"streamType\":1}]}]}]}]}}";
static const char JELLYFIN[] =
  "{\"TotalRecordCount\":1,\"Items\":[{\"Type\":\"Movie\",\"Id\":\"a\",\"Name\":\"N\",\"Genres\":[\"G\"],"
  "\"BackdropImageTags\":[\"b\"],\"ProviderIds\":{\"Imdb\":\"tt1\"}}],\"People\":[{\"Name\":\"P\"}],"
  "\"MediaSources\":[{\"Id\":\"m\",\"MediaStreams\":[{\"Type\":\"Subtitle\",\"Codec\":\"srt\"}]}]}";

// Every public parser that walks an array through jsraiz.h.
static void todosParsers(const char *corpo, size_t n) {
  static PxConta pc;
  static JfConta jc;
  // A usable account: an empty one makes the item parsers give up before the arrays.
  snprintf(pc.base, sizeof pc.base, "http://plex.example:32400");
  snprintf(pc.servidorId, sizeof pc.servidorId, "0123456789abcdef0123456789abcdef01234567");
  snprintf(pc.token, sizeof pc.token, "tok");
  snprintf(jc.base, sizeof jc.base, "http://jf.example:8096");
  snprintf(jc.servidorId, sizeof jc.servidorId, "0123456789abcdef0123456789abcdef");
  snprintf(jc.token, sizeof jc.token, "tok");
  static PxServidor srv[2];
  static PxBiblioteca pb[2];
  static JfBiblioteca jb[2];
  static CatItem it[2];
  static CatEp ep[2];
  static PxMidiaInfo mi[2];
  static PxPlayback pp;
  static JfPlayback jp;
  static Legenda leg[2];
  static SrvIdx x;
  char rk[32];
  srvidx_iniciar(&x);
  px_ler_recursos(corpo, n, srv, 2);
  px_ler_bibliotecas(corpo, n, pb, 2);
  px_ler_indice(corpo, n, "201", &x, NULL);
  px_ler_itens(&pc, corpo, n, it, 2, NULL);
  px_ler_detalhe(&pc, corpo, n, it);
  px_ler_episodios(&pc, corpo, n, ep, 2);
  px_ler_episodio_rk(corpo, n, 2, 2, rk, sizeof rk);
  px_ler_midias(corpo, n, mi, 2);
  px_ler_decisao(corpo, n);
  px_ler_fontes(&pc, "5", corpo, n, NULL, &pp);
  jf_ler_bibliotecas(corpo, n, jb, 2);
  jf_ler_indice(corpo, n, "s", &x, NULL);
  jf_ler_itens(&jc, corpo, n, it, 2, NULL);
  jf_ler_detalhe(&jc, corpo, n, it);
  jf_ler_episodios(&jc, corpo, n, ep, 2);
  jf_ler_playbackinfo(&jc, "a", corpo, n, &jp);
  jf_ler_legendas(&jc, "a", "m", corpo, n, leg, 2);
  srvidx_limpar(&x);
}

// Every truncation of a well-formed document, and every byte replaced by every
// character the walk treats specially. The buffer stays NUL-terminated, as rede.c.
static void varrer(const char *doc) {
  static const char TROCAS[] = "}]{[\",:1";
  size_t n = strlen(doc);
  char *b = malloc(n + 1);
  assert(b);
  for (size_t i = 0; i <= n; i++) {
    memcpy(b, doc, i);
    b[i] = 0;
    todosParsers(b, i);
    if (i == n) break;
    memcpy(b, doc, n + 1);
    for (size_t t = 0; t < sizeof TROCAS; t++) {   // the array's own NUL included
      b[i] = TROCAS[t];
      todosParsers(b, n);
    }
  }
  free(b);
}

static void varrerPlex(void) { varrer(PLEX); }
static void varrerJellyfin(void) { varrer(JELLYFIN); }

static const SrvIdxItem *indice(SrvIdx *x, const char *corpo, int vistos, int indexados) {
  srvidx_iniciar(x);
  assert(px_ler_indice(corpo, strlen(corpo), NULL, x, NULL) == vistos);
  assert(x->n == indexados);
  return x->v;
}

// Well-formed answers keep parsing; a stray brace ends that one array and only it.
static void resultados(void) {
  static const char DOIS[] =
    "{\"MediaContainer\":{\"size\":2,\"Metadata\":["
    "{\"type\":\"movie\",\"ratingKey\":\"5\",\"Guid\":[1,\"x\",null,{\"id\":\"imdb://tt1234567\"},{\"id\":\"tmdb://9\"}]},"
    "{\"type\":\"movie\",\"ratingKey\":\"6\"}]}}";
  SrvIdx x;
  const SrvIdxItem *e = indice(&x, DOIS, 2, 2);
  assert(!strcmp(e[0].imdb, "tt1234567") && e[0].tmdb == 9 && !strcmp(e[1].item, "6"));
  srvidx_limpar(&x);
  e = indice(&x, PX_ITEM("[1}]"), 1, 1);
  assert(!strcmp(e[0].item, "5") && !e[0].imdb[0] && !e[0].tmdb);
  srvidx_limpar(&x);
  // The stray brace also closes the item, so the last object is a second, untyped row.
  e = indice(&x, PX_ITEM("[{\"id\":\"tmdb://9\"},1},{\"id\":\"imdb://tt1234567\"}]"), 2, 1);
  assert(e[0].tmdb == 9 && !e[0].imdb[0]);
  srvidx_limpar(&x);
}

int teste_jf_episodios_preparar(const JfConta *, RedeJob *, const char *, CatEp **, int, CatEpPreparar, void *);
int teste_jf_episodios_cancelavel(const JfConta *, RedeJob *, const char *, CatEp **, int, CatEpPreparar, void *, int (*)(void *), void *);
int teste_px_episodios_preparar(const PxConta *, RedeJob *, const char *, CatEp **, int, CatEpPreparar, void *);
int teste_px_episodios_cancelavel(const PxConta *, RedeJob *, const char *, CatEp **, int, CatEpPreparar, void *, int (*)(void *), void *);
int teste_plex_esperando(void);
int perfis_ativo(void) { return 1; }
void cat_historico_definir_tempo(const char *id, const char *tipo, int visto, long long ms) { (void)id; (void)tipo; (void)visto; (void)ms; }
char *dados_caminho(char *dst, unsigned n, const char *nome) { (void)dst; (void)n; (void)nome; return NULL; }
void dados_fs_liberar(void) { }
void dados_fs_travar(void) { }
int dados_gravar_leve(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 0; }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
const char *i18n(const char *s) { return s; }
Uint32 SDL_GetTicks(void) { return 0; }
FILE *teste_tmpfile(void) { return NULL; }
static int falharMinimos;
void *teste_plex_min_realloc(void *p, size_t n) { return falharMinimos ? NULL : realloc(p, n); }
static int modoHttp, modoCallback, chamouCallback, totalSimulado, pedidosSimulados, extraSimulado;
static int invalidoSimulado, paddingSimulado, aleatorioSimulado;
static int jfRico, jfPedidos, jfObsoleto;
static size_t jfMemoria, jfPico, jfResposta;
static struct { void *p; size_t n; } jfAlocacoes[4];
static void jfSomar(size_t n) {
  jfMemoria += n;
  if (jfMemoria > jfPico) jfPico = jfMemoria;
}
void *teste_jf_malloc(size_t n) {
  void *p = malloc(n);
  if (jfRico && p) {
    int i;
    for (i = 0; i < 4 && jfAlocacoes[i].p; i++);
    assert(i < 4); jfAlocacoes[i].p = p; jfAlocacoes[i].n = n; jfSomar(n);
  }
  return p;
}
void teste_jf_free(void *p) {
  for (int i = 0; i < 4; i++) if (jfAlocacoes[i].p == p && p) {
    jfMemoria -= jfAlocacoes[i].n; jfAlocacoes[i].p = NULL; break;
  }
  free(p);
}
void teste_jf_resposta_limpar(RedeResposta *r) {
  if (jfRico && r->corpo && jfResposta) { jfMemoria -= jfResposta; jfResposta = 0; }
  rede_resposta_limpar(r);
}
static int jfCancelado(void *ctx) { (void)ctx; return jfObsoleto > 0; }
static const void *jfCorpoAnterior;
extern int __asan_address_is_poisoned(const void *);
static pthread_mutex_t concorrenciaTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t concorrenciaCond = PTHREAD_COND_INITIALIZER;
static int concorrente, chegaram, liberarConcorrentes;
static int testarGeracao, geracaoAtual, chamadasGeracao[4];
static size_t workspaceMedido, picoMedido;
static int testarOrcamento;
static RedeJob *jobsGeracao[4];
static const char *respostaHttp;
int teste_rede_pedir(const RedePedido *pedido, RedeResposta *resposta) {
  memset(resposta, 0, sizeof *resposta);
  resposta->status = 200;
  if (concorrente) {
    pthread_mutex_lock(&concorrenciaTrava);
    assert(workspaceMedido == 0);
    if (testarGeracao) for (int i = 0; i < 4; i++)
      if (pedido->job == jobsGeracao[i]) chamadasGeracao[i]++;
    chegaram++; pthread_cond_broadcast(&concorrenciaCond);
    while (!liberarConcorrentes && (!testarGeracao || pedido->job != jobsGeracao[3])) {
      struct timespec limite;
      clock_gettime(CLOCK_REALTIME, &limite);
      limite.tv_nsec += 10000000;
      if (limite.tv_nsec >= 1000000000) { limite.tv_sec++; limite.tv_nsec -= 1000000000; }
      if (pedido->parar && pedido->parar(pedido->parar_usuario)) {
        pthread_mutex_unlock(&concorrenciaTrava);
        resposta->erro = REDE_CANCELADO;
        return 0;
      }
      pthread_cond_timedwait(&concorrenciaCond, &concorrenciaTrava, &limite);
    }
    pthread_mutex_unlock(&concorrenciaTrava);
  }
  if (jfRico) {
    int inicio = -1, limite = 0;
    const char *q = strstr(pedido->url, "&StartIndex=");
    assert(q && sscanf(q, "&StartIndex=%d&Limit=%d", &inicio, &limite) == 2);
    assert(inicio == jfPedidos * 100 && limite == 100);
    jfPedidos++;
    if (jfObsoleto == -1 && jfPedidos == 2) {
      jfObsoleto = 1;
      assert(pedido->parar && pedido->parar(pedido->parar_usuario));
      resposta->erro = REDE_CANCELADO;
      return 0;
    }
    size_t cap = 100000, pos = 0;
    resposta->corpo = malloc(cap); assert(resposta->corpo);
    assert(!jfResposta); jfResposta = cap; jfSomar(cap);
    pos += (size_t)snprintf(resposta->corpo, cap, "{\"TotalRecordCount\":5000,\"Items\":[");
    for (int i = 0; i < 100; i++) {
      char texto[420];
      if (jfRico == 2) {
        unsigned x = (unsigned)(inicio + i + 1) * 2654435761u;
        const char letras[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        for (size_t j = 0; j + 1 < sizeof texto; j++) {
          x ^= x << 13; x ^= x >> 17; x ^= x << 5;
          texto[j] = letras[x % (sizeof letras - 1)];
        }
        texto[sizeof texto - 1] = 0;
      } else snprintf(texto, sizeof texto, "Rich synopsis %d", inicio + i + 1);
      pos += (size_t)snprintf(resposta->corpo + pos, cap - pos,
        "%s{\"Id\":\"%x\",\"ParentIndexNumber\":1,\"IndexNumber\":%d,\"Name\":\"Episode %d\",\"Overview\":\"%s\",\"RunTimeTicks\":36000000000,\"PremiereDate\":\"2025-01-23\",\"ImageTags\":{\"Primary\":\"image\"}}",
        i ? "," : "", inicio + i + 1, inicio + i + 1, inicio + i + 1, texto);
    }
    assert(pos + 3 < cap); memcpy(resposta->corpo + pos, "]}", 3);
    jfCorpoAnterior = resposta->corpo;
  } else if (totalSimulado) {
    int inicio = -1, tamanho = 0;
    const char *q = strstr(pedido->url, "X-Plex-Container-Start=");
    assert(q && sscanf(q, "X-Plex-Container-Start=%d&X-Plex-Container-Size=%d", &inicio, &tamanho) == 2);
    assert(inicio == pedidosSimulados * 2000 && tamanho > 0 && tamanho <= 2000);
    pedidosSimulados++;
    int n = totalSimulado - inicio; if (n > tamanho) n = tamanho;
    n += extraSimulado;
    size_t cap = (size_t)n * (100 + (size_t)paddingSimulado + (aleatorioSimulado ? 560u : 0u)) + 200, pos;
    resposta->corpo = malloc(cap); assert(resposta->corpo);
    pos = (size_t)snprintf(resposta->corpo, cap, "{\"MediaContainer\":{\"totalSize\":%d,\"Metadata\":[", totalSimulado);
    for (int i = 0; i < n; i++) {
      if (invalidoSimulado && inicio + i == 0)
        pos += (size_t)snprintf(resposta->corpo + pos, cap - pos, "{\"invalid\":1}");
      else if (aleatorioSimulado) {
        char texto[420]; unsigned x = (unsigned)(inicio + i + 1) * 2654435761u;
        const char letras[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        for (size_t j = 0; j + 1 < sizeof texto; j++) {
          x ^= x << 13; x ^= x >> 17; x ^= x << 5;
          texto[j] = letras[x % (sizeof letras - 1)];
        }
        texto[sizeof texto - 1] = 0;
        pos += (size_t)snprintf(resposta->corpo + pos, cap - pos,
          "%s{\"ratingKey\":\"%d\",\"parentIndex\":1,\"index\":%d,\"title\":\"Title %d\",\"summary\":\"%s\"}",
          i ? "," : "", inicio + i + 1, inicio + i + 1, inicio + i + 1, texto);
      } else
        pos += (size_t)snprintf(resposta->corpo + pos, cap - pos,
          "%s{\"ratingKey\":\"%d\",\"parentIndex\":1,\"index\":%d,\"padding\":\"%0*d\"}",
          i ? "," : "", inicio + i + 1, inicio + i + 1, paddingSimulado, 0);
    }
    assert(pos + 4 < cap);
    snprintf(resposta->corpo + pos, cap - pos, "]}}");
  } else if (testarOrcamento) {
    size_t cap = 6u * 1024u * 1024u, n = strlen(respostaHttp);
    resposta->corpo = malloc(cap); assert(resposta->corpo && n < cap);
    memcpy(resposta->corpo, respostaHttp, n);
    memset(resposta->corpo + n, ' ', cap - n - 1);
    resposta->corpo[cap - 1] = 0;
  } else resposta->corpo = strdup(respostaHttp);
  assert(resposta->corpo);
  resposta->n_corpo = strlen(resposta->corpo);
  if (concorrente) {
    size_t usados;
    pthread_mutex_lock(&concorrenciaTrava);
    usados = workspaceMedido + resposta->n_corpo + 1;
    if (usados > picoMedido) picoMedido = usados;
    pthread_mutex_unlock(&concorrenciaTrava);
  }
  if (modoHttp && (jfRico ? jfPedidos == modoHttp : !totalSimulado || pedidosSimulados == modoHttp)) rede_job_cancelar(pedido->job);
  return 0;
}
static CatEp *prepararTeste(void *ctx, int n) {
  RedeJob *job = ctx;
  __atomic_add_fetch(&chamouCallback, 1, __ATOMIC_RELAXED);
  if (jfRico) {
    assert(n == 5000 && jfPedidos == 50);
    assert(__asan_address_is_poisoned(jfCorpoAnterior));
    jfSomar((size_t)n * sizeof(CatEp));
    return calloc((size_t)n, sizeof(CatEp));
  }
  if (n != (totalSimulado ? totalSimulado - invalidoSimulado : 2))
    fprintf(stderr, "unexpected provider episode count %d total %d invalid %d\n", n, totalSimulado, invalidoSimulado);
  assert(n == (totalSimulado ? totalSimulado - invalidoSimulado : 2));
  if (totalSimulado) assert(pedidosSimulados == (totalSimulado == 4650 ? 3 : 1));
  if (modoCallback == 1) return NULL;
  CatEp *out = calloc((size_t)n, sizeof *out);
  assert(out);
  if (modoCallback == 2) rede_job_cancelar(job);
  return out;
}
typedef struct { PxConta *conta; RedeJob *job; CatEp *out; int resultado; int geracao, vaga; } PedidoConcorrente;
static int pedidoObsoleto(void *ctx) {
  PedidoConcorrente *p = ctx;
  return p->geracao != __atomic_load_n(&geracaoAtual, __ATOMIC_ACQUIRE);
}
static CatEp *prepararGeracao(void *ctx, int n) { (void)ctx; return calloc((size_t)n, sizeof(CatEp)); }
static void *buscarFichaCancelada(void *arg) {
  PedidoConcorrente *p = arg;
  p->resultado = plex_ficha_preparar(NULL, &p->out, 4, prepararGeracao, NULL, pedidoObsoleto, p, &p->vaga);
  return NULL;
}
static void *buscarGeracao(void *arg) {
  PedidoConcorrente *p = arg;
  p->resultado = teste_px_episodios_cancelavel(p->conta, p->job, "1", &p->out, 4, prepararGeracao, NULL, pedidoObsoleto, p);
  return NULL;
}
static void *buscarConcorrente(void *arg) {
  PedidoConcorrente *p = arg;
  p->resultado = teste_px_episodios_preparar(p->conta, p->job, "1", &p->out, 2, prepararTeste, p->job);
  return NULL;
}
static void testarPreparo(void) {
  static const char jf[] = "{\"Items\":[{\"Id\":\"a\",\"ParentIndexNumber\":1,\"IndexNumber\":1},{\"Id\":\"b\",\"ParentIndexNumber\":1,\"IndexNumber\":2}]}";
  static const char px[] = "{\"MediaContainer\":{\"Metadata\":[{\"ratingKey\":\"1\",\"parentIndex\":1,\"index\":1,\"thumb\":\"/thumb\"},{\"ratingKey\":\"2\",\"parentIndex\":1,\"index\":2}]}}";
  JfConta jc = {0}; PxConta pc = {0};
  snprintf(jc.base, sizeof jc.base, "http://local.invalid");
  snprintf(jc.servidorId, sizeof jc.servidorId, "0123456789abcdef0123456789abcdef");
  snprintf(pc.base, sizeof pc.base, "http://local.invalid");
  snprintf(pc.servidorId, sizeof pc.servidorId, "0123456789abcdef0123456789abcdef01234567");
  snprintf(pc.token, sizeof pc.token, "synthetic-token");
  for (int provedor = 0; provedor < 3; provedor++) for (int caso = 0; caso < 4; caso++) {
    RedeGrupo *grupo = rede_grupo_criar();
    RedeJob *job = rede_job_criar(grupo);
    CatEp *out = NULL;
    assert(grupo && job);
    jc.tipo = provedor == 1 ? JF_TIPO_EMBY : JF_TIPO_JELLYFIN;
    respostaHttp = provedor == 2 ? px : jf;
    modoHttp = caso == 1;
    modoCallback = caso == 2 ? 1 : caso == 3 ? 2 : 0;
    chamouCallback = 0;
    int n = provedor == 2 ? teste_px_episodios_preparar(&pc, job, "1", &out, 2, prepararTeste, job)
                           : teste_jf_episodios_preparar(&jc, job, "a", &out, 2, prepararTeste, job);
    assert(chamouCallback == (caso != 1));
    if (!caso) {
      assert(n == 2 && out && out[0].episodio == 1 && out[1].episodio == 2);
      if (provedor == 2) assert(strstr(out[0].thumb, "&X-Plex-Token=synthetic-token"));
    } else assert(n < 0);
    free(out);
    rede_job_soltar(job);
    rede_grupo_soltar(grupo);
  }
  for (int tipo = 0; tipo < 2; tipo++) {
    RedeGrupo *grupo = rede_grupo_criar();
    RedeJob *job = rede_job_criar(grupo);
    CatEp *out = NULL;
    jc.tipo = tipo ? JF_TIPO_EMBY : JF_TIPO_JELLYFIN;
    jfRico = 1; jfPedidos = modoHttp = modoCallback = 0;
    int n = teste_jf_episodios_preparar(&jc, job, "a", &out, 5000, prepararTeste, job);
    assert(n == 5000 && jfPedidos == 50);
    assert(jfPico <= (size_t)5000 * sizeof(CatEp) + 512u * 1024u);
    printf("jellyfin staged peak: %zu bytes (excluding zlib internals)\n", jfPico);
    for (int i = 0; i < n; i++) {
      char nome[80], sinopse[80];
      snprintf(nome, sizeof nome, "Episode %d", i + 1);
      snprintf(sinopse, sizeof sinopse, "Rich synopsis %d", i + 1);
      assert(out[i].episodio == i + 1 && !strcmp(out[i].nome, nome));
      assert(!strcmp(out[i].sinopse, sinopse) && !strcmp(out[i].duracao, "60 min"));
      assert(!strcmp(out[i].dataIso, "2025-01-23") && out[i].thumb[0]);
    }
    free(out); jfMemoria = jfPico = 0; rede_job_soltar(job); rede_grupo_soltar(grupo);
    grupo = rede_grupo_criar(); job = rede_job_criar(grupo); out = NULL;
    jfPedidos = chamouCallback = 0; modoHttp = 2;
    n = teste_jf_episodios_preparar(&jc, job, "a", &out, 5000, prepararTeste, job);
    assert(n < 0 && jfPedidos == 2 && !chamouCallback && !out);
    rede_job_soltar(job); rede_grupo_soltar(grupo);
    grupo = rede_grupo_criar(); job = rede_job_criar(grupo); out = NULL;
    jfPedidos = chamouCallback = modoHttp = 0; jfObsoleto = -1;
    n = teste_jf_episodios_cancelavel(&jc, job, "a", &out, 5000, prepararTeste, job, jfCancelado, NULL);
    assert(n < 0 && jfPedidos == 2 && !chamouCallback && !out);
    rede_job_soltar(job); rede_grupo_soltar(grupo);
    grupo = rede_grupo_criar(); job = rede_job_criar(grupo); out = NULL;
    jfRico = 2; jfObsoleto = jfPedidos = chamouCallback = 0;
    n = teste_jf_episodios_preparar(&jc, job, "a", &out, 5000, prepararTeste, job);
    assert(n < 0 && jfPedidos > 1 && !chamouCallback && !out);
    jfObsoleto = jfRico = 0; free(out); rede_job_soltar(job); rede_grupo_soltar(grupo);
  }
  {
    RedeGrupo *grupo = rede_grupo_criar();
    RedeJob *job = rede_job_criar(grupo);
    CatEp *out = NULL;
    respostaHttp = px; modoHttp = modoCallback = 0; falharMinimos = 1;
    int n = teste_px_episodios_preparar(&pc, job, "1", &out, 2, prepararTeste, job);
    assert(n == 2 && out && out[0].episodio == 1 && strstr(out[0].thumb, "&X-Plex-Token=synthetic-token"));
    falharMinimos = 0; free(out); rede_job_soltar(job); rede_grupo_soltar(grupo);
  }
  {
    RedeGrupo *grupo = rede_grupo_criar();
    RedeJob *job = rede_job_criar(grupo);
    CatEp *out = NULL;
    totalSimulado = 4650; pedidosSimulados = modoHttp = modoCallback = chamouCallback = 0;
    int n = teste_px_episodios_preparar(&pc, job, "1", &out, 5000, prepararTeste, job);
    assert(n == 4650 && pedidosSimulados == 3 && chamouCallback == 1);
    assert(out[0].episodio == 1 && out[4649].episodio == 4650);
    free(out); rede_job_soltar(job); rede_grupo_soltar(grupo);
    grupo = rede_grupo_criar(); job = rede_job_criar(grupo); out = NULL;
    pedidosSimulados = chamouCallback = 0; modoHttp = 2;
    n = teste_px_episodios_preparar(&pc, job, "1", &out, 5000, prepararTeste, job);
    assert(n < 0 && pedidosSimulados == 2 && !chamouCallback && !out);
    rede_job_soltar(job); rede_grupo_soltar(grupo);
    grupo = rede_grupo_criar(); job = rede_job_criar(grupo); out = NULL;
    totalSimulado = 2000; pedidosSimulados = chamouCallback = modoHttp = 0; extraSimulado = 1;
    n = teste_px_episodios_preparar(&pc, job, "1", &out, 5000, prepararTeste, job);
    assert(n == 2000 && pedidosSimulados == 1 && out[1999].episodio == 2000);
    free(out); rede_job_soltar(job); rede_grupo_soltar(grupo);
    grupo = rede_grupo_criar(); job = rede_job_criar(grupo); out = NULL;
    totalSimulado = 4650; pedidosSimulados = chamouCallback = modoHttp = 0;
    extraSimulado = invalidoSimulado = 1;
    n = teste_px_episodios_preparar(&pc, job, "1", &out, 5000, prepararTeste, job);
    assert(n == 4649 && pedidosSimulados == 3);
    for (int i = 0; i < n; i++) assert(out[i].episodio == i + 2);
    free(out); rede_job_soltar(job); rede_grupo_soltar(grupo);
    grupo = rede_grupo_criar(); job = rede_job_criar(grupo); out = NULL;
    pedidosSimulados = chamouCallback = 0; paddingSimulado = 1600;
    n = teste_px_episodios_preparar(&pc, job, "1", &out, 5000, prepararTeste, job);
    assert(n == 4649 && pedidosSimulados == 3);
    for (int i = 0; i < n; i++) assert(out[i].episodio == i + 2);
    free(out); rede_job_soltar(job); rede_grupo_soltar(grupo);
    grupo = rede_grupo_criar(); job = rede_job_criar(grupo); out = NULL;
    pedidosSimulados = chamouCallback = 0;
    extraSimulado = invalidoSimulado = paddingSimulado = 0; aleatorioSimulado = 1;
    assert(!plex_tomar_aviso_metadata(0));
    n = teste_px_episodios_preparar(&pc, job, "1", &out, 5000, prepararTeste, job);
    assert(n == 4650 && chamouCallback == 1 && out);
    for (int i = 0; i < n; i++) assert(out[i].episodio == i + 1 && out[i].vid[0]);
    assert(!strcmp(out[0].nome, "Title 1") && !strcmp(out[4649].nome, "Title 4650"));
    assert(!out[0].sinopse[0]);
    assert(!plex_tomar_aviso_metadata(0));
    assert(plex_tomar_reduzido() && !plex_tomar_reduzido());
    plex_publicar_aviso_metadata();
    glem_teste_cartao("Program", "Channel", 0);
    assert(glem_cartao_aberto());
    if (plex_tomar_aviso_metadata(glem_cartao_aberto())) glem_aviso_curto("Plex");
    assert(glem_cartao_aberto());
    glem_teste_cartao("Program", "Channel", 1);
    assert(!glem_cartao_aberto());
    assert(plex_tomar_aviso_metadata(glem_cartao_aberto()));
    glem_aviso_curto("Plex");
    assert(!plex_tomar_aviso_metadata(glem_cartao_aberto()));
    free(out); rede_job_soltar(job); rede_grupo_soltar(grupo);
    totalSimulado = modoHttp = aleatorioSimulado = 0;
  }
  {
    RedeGrupo *grupo = rede_grupo_criar();
    PedidoConcorrente atual = { .conta = &pc, .job = rede_job_criar(grupo) };
    PedidoConcorrente obsoleto = { .geracao = 1 };
    CatEp *saida = NULL, *workspace;
    int vaga = 0; pthread_t fio, fioAntigo;
    /* Failed detail still transfers its permit until caller has freed workspace. */
    assert(plex_ficha_preparar(NULL, &saida, 5000, prepararTeste, NULL, NULL, NULL, &vaga) < 0 && vaga);
    workspace = malloc(5000u * sizeof *workspace); assert(workspace);
    workspaceMedido = 5000u * sizeof *workspace;
    picoMedido = workspaceMedido;
    respostaHttp = px; concorrente = testarOrcamento = 1; chegaram = 0; liberarConcorrentes = 1;
    __atomic_store_n(&geracaoAtual, 1, __ATOMIC_RELEASE);
    assert(!pthread_create(&fioAntigo, NULL, buscarFichaCancelada, &obsoleto));
    while (!teste_plex_esperando()) sched_yield();
    __atomic_store_n(&geracaoAtual, 2, __ATOMIC_RELEASE);
    pthread_join(fioAntigo, NULL);
    assert(obsoleto.resultado < 0 && !obsoleto.vaga);
    assert(!pthread_create(&fio, NULL, buscarConcorrente, &atual));
    while (!teste_plex_esperando()) sched_yield();
    pthread_mutex_lock(&concorrenciaTrava);
    assert(chegaram == 0); /* 5000 records live: no second response allocation. */
    pthread_mutex_unlock(&concorrenciaTrava);
    assert(picoMedido == 5000u * sizeof *workspace);
    free(workspace); workspaceMedido = 0; plex_vaga_liberar();
    pthread_join(fio, NULL);
    assert(atual.resultado == 2 && atual.out && chegaram == 1);
    assert(picoMedido == (5000u * sizeof *workspace > 6u * 1024u * 1024u
                          ? 5000u * sizeof *workspace : 6u * 1024u * 1024u));
    free(atual.out); rede_job_soltar(atual.job); rede_grupo_soltar(grupo);
    concorrente = testarOrcamento = 0;
  }
  {
    static const char pagina[] = "{\"MediaContainer\":{\"totalSize\":4,\"Metadata\":[{\"ratingKey\":\"1\",\"parentIndex\":1,\"index\":1},{\"ratingKey\":\"2\",\"parentIndex\":1,\"index\":2}]}}";
    RedeGrupo *grupo = rede_grupo_criar();
    PedidoConcorrente p[4] = {0}; pthread_t t[4];
    for (int i = 0; i < 4; i++) {
      p[i].conta = &pc; p[i].job = rede_job_criar(grupo);
      p[i].geracao = i == 3 ? 2 : 1; jobsGeracao[i] = p[i].job;
    }
    respostaHttp = pagina; concorrente = testarGeracao = 1;
    chegaram = liberarConcorrentes = 0; __atomic_store_n(&geracaoAtual, 1, __ATOMIC_RELEASE);
    assert(!pthread_create(&t[0], NULL, buscarGeracao, &p[0]));
    pthread_mutex_lock(&concorrenciaTrava);
    while (chegaram < 1) pthread_cond_wait(&concorrenciaCond, &concorrenciaTrava);
    pthread_mutex_unlock(&concorrenciaTrava);
    assert(!pthread_create(&t[1], NULL, buscarGeracao, &p[1]));
    assert(!pthread_create(&t[2], NULL, buscarGeracao, &p[2]));
    while (teste_plex_esperando() < 2) sched_yield();
    __atomic_store_n(&geracaoAtual, 2, __ATOMIC_RELEASE);
    assert(!pthread_create(&t[3], NULL, buscarGeracao, &p[3]));
    pthread_join(t[2], NULL);
    assert(p[2].resultado < 0 && chamadasGeracao[2] == 0);
    /* Old transports remain stalled unless their cancellation callback fires. */
    pthread_join(t[0], NULL); pthread_join(t[1], NULL); pthread_join(t[3], NULL);
    assert(p[0].resultado < 0 && p[1].resultado < 0 && p[3].resultado == 4);
    assert(chamadasGeracao[0] == 1 && chamadasGeracao[1] == 0 && chamadasGeracao[2] == 0 && chamadasGeracao[3] == 2);
    free(p[3].out);
    for (int i = 0; i < 4; i++) rede_job_soltar(p[i].job);
    concorrente = testarGeracao = 0; rede_grupo_soltar(grupo);
  }
}

static int falhas;

static void relatar(const char *nome, size_t i, pid_t pid) {
  int st = 0;
  assert(waitpid(pid, &st, 0) == pid);
  if (WIFEXITED(st) && WEXITSTATUS(st) == 0) return;
  falhas++;
  printf("FAIL %s %zu: %s (status %d)\n", nome, i,
         WIFSIGNALED(st) && WTERMSIG(st) == SIGALRM ? "did not terminate" : "wrong result or crash", st);
}

static void emFilho(const char *nome, void (*f)(void), unsigned prazo) {
  pid_t pid = fork();
  assert(pid >= 0);
  if (!pid) { alarm(prazo); f(); _exit(0); }
  relatar(nome, 0, pid);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  for (size_t i = 0; i < sizeof MALFORMADOS / sizeof *MALFORMADOS; i++) {
    pid_t pid = fork();
    assert(pid >= 0);
    if (!pid) { alarm(3); todosParsers(MALFORMADOS[i].corpo, MALFORMADOS[i].n); _exit(0); }
    relatar("malformed", i, pid);
  }
  emFilho("deferred-provider", testarPreparo, 30);
  emFilho("results", resultados, 3);
  emFilho("sweep-plex", varrerPlex, 30);
  emFilho("sweep-jellyfin", varrerJellyfin, 30);
  if (falhas) { printf("jsraiz_progresso: %d failed\n", falhas); return 1; }
  printf("jsraiz_progresso: all ok\n");
  return 0;
}
