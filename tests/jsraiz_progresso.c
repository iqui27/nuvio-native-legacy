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
int teste_px_episodios_preparar(const PxConta *, RedeJob *, const char *, CatEp **, int, CatEpPreparar, void *);
int teste_plex_esperando(void);
int perfis_ativo(void) { return 1; }
const char *i18n(const char *s) { return s; }
Uint32 SDL_GetTicks(void) { return 0; }
FILE *teste_tmpfile(void) { return NULL; }
static int falharMinimos;
void *teste_plex_min_realloc(void *p, size_t n) { return falharMinimos ? NULL : realloc(p, n); }
static int modoHttp, modoCallback, chamouCallback, totalSimulado, pedidosSimulados, extraSimulado;
static int invalidoSimulado, paddingSimulado, aleatorioSimulado;
static pthread_mutex_t concorrenciaTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t concorrenciaCond = PTHREAD_COND_INITIALIZER;
static int concorrente, chegaram, liberarConcorrentes;
static const char *respostaHttp;
int teste_rede_pedir(const RedePedido *pedido, RedeResposta *resposta) {
  memset(resposta, 0, sizeof *resposta);
  resposta->status = 200;
  if (concorrente) {
    pthread_mutex_lock(&concorrenciaTrava);
    chegaram++; pthread_cond_broadcast(&concorrenciaCond);
    while (!liberarConcorrentes) pthread_cond_wait(&concorrenciaCond, &concorrenciaTrava);
    pthread_mutex_unlock(&concorrenciaTrava);
  }
  if (totalSimulado) {
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
  } else resposta->corpo = strdup(respostaHttp);
  assert(resposta->corpo);
  resposta->n_corpo = strlen(resposta->corpo);
  if (modoHttp && (!totalSimulado || pedidosSimulados == modoHttp)) rede_job_cancelar(pedido->job);
  return 0;
}
static CatEp *prepararTeste(void *ctx, int n) {
  RedeJob *job = ctx;
  __atomic_add_fetch(&chamouCallback, 1, __ATOMIC_RELAXED);
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
typedef struct { PxConta *conta; RedeJob *job; CatEp *out; int resultado; } PedidoConcorrente;
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
    PedidoConcorrente a = { .conta = &pc, .job = rede_job_criar(grupo) };
    PedidoConcorrente b = { .conta = &pc, .job = rede_job_criar(grupo) };
    PedidoConcorrente c = { .conta = &pc, .job = rede_job_criar(grupo) };
    pthread_t fa, fb, fc;
    respostaHttp = px; concorrente = 1; chegaram = liberarConcorrentes = chamouCallback = 0;
    assert(!pthread_create(&fa, NULL, buscarConcorrente, &a));
    assert(!pthread_create(&fb, NULL, buscarConcorrente, &b));
    pthread_mutex_lock(&concorrenciaTrava);
    while (chegaram < 2) pthread_cond_wait(&concorrenciaCond, &concorrenciaTrava);
    pthread_mutex_unlock(&concorrenciaTrava);
    assert(!pthread_create(&fc, NULL, buscarConcorrente, &c));
    while (!teste_plex_esperando()) sched_yield();
    rede_job_cancelar(c.job);
    pthread_join(fc, NULL);
    assert(c.resultado < 0 && !c.out && chegaram == 2);
    rede_job_cancelar(a.job);
    pthread_mutex_lock(&concorrenciaTrava);
    liberarConcorrentes = 1; pthread_cond_broadcast(&concorrenciaCond);
    pthread_mutex_unlock(&concorrenciaTrava);
    pthread_join(fa, NULL); pthread_join(fb, NULL);
    assert(a.resultado < 0 && !a.out && b.resultado == 2 && b.out);
    concorrente = 0;
    rede_job_soltar(c.job); c.job = rede_job_criar(grupo);
    buscarConcorrente(&c);
    assert(c.resultado == 2 && c.out);
    free(b.out); free(c.out); rede_job_soltar(a.job); rede_job_soltar(b.job); rede_job_soltar(c.job);
    a = (PedidoConcorrente){ .conta = &pc, .job = rede_job_criar(grupo) };
    b = (PedidoConcorrente){ .conta = &pc, .job = rede_job_criar(grupo) };
    c = (PedidoConcorrente){ .conta = &pc, .job = rede_job_criar(grupo) };
    concorrente = 1; chegaram = liberarConcorrentes = 0;
    assert(!pthread_create(&fa, NULL, buscarConcorrente, &a));
    assert(!pthread_create(&fb, NULL, buscarConcorrente, &b));
    pthread_mutex_lock(&concorrenciaTrava);
    while (chegaram < 2) pthread_cond_wait(&concorrenciaCond, &concorrenciaTrava);
    pthread_mutex_unlock(&concorrenciaTrava);
    assert(!pthread_create(&fc, NULL, buscarConcorrente, &c));
    while (!teste_plex_esperando()) sched_yield();
    rede_job_cancelar(a.job);
    pthread_mutex_lock(&concorrenciaTrava);
    liberarConcorrentes = 1; pthread_cond_broadcast(&concorrenciaCond);
    pthread_mutex_unlock(&concorrenciaTrava);
    pthread_join(fa, NULL); pthread_join(fb, NULL); pthread_join(fc, NULL);
    assert(a.resultado < 0 && b.resultado == 2 && c.resultado == 2);
    free(b.out); free(c.out);
    rede_job_soltar(a.job); rede_job_soltar(b.job); rede_job_soltar(c.job);
    concorrente = 0; rede_grupo_soltar(grupo);
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
