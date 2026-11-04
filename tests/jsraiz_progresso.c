// The array walk in jsraiz.h (elemento/depoisElemento) always terminates, through
// the real Plex and Jellyfin parsers: every case runs in a child under an alarm,
// so a hang is reported as a FAIL instead of blocking the suite.
#include "plex.h"
#include "jellyfin.h"
#include "servidores.h"
#include "srvidx.h"
#include "stub_perfisessao.h"
#include <assert.h>
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
  emFilho("results", resultados, 3);
  emFilho("sweep-plex", varrerPlex, 30);
  emFilho("sweep-jellyfin", varrerJellyfin, 30);
  if (falhas) { printf("jsraiz_progresso: %d failed\n", falhas); return 1; }
  printf("jsraiz_progresso: all ok\n");
  return 0;
}
