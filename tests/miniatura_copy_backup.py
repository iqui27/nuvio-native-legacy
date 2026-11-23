"""Copy-first: 1080p backups after the smaller copies and the per-source failure history (upstream cbd2e6dd).

Production C cut from src/miniatura_android.c: escolherCopias (with the 0.80-rule 1080p copies kept as backups,
heavy remuxes out, 12 attempts), the copias-fontes.txt history (3-day expiry, atomic write, keys only), and
escolherCopiaRapida/checarCopia verdict recording. Stream list, open/info/check and the device CPU profile are
stubbed; the data dir is a temp one.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
text = (root / "src/miniatura_android.c").read_text()


def cut(start, end):
    a = text.index(start)
    return text[a:text.index(end, a)]


sel = cut("static const char *EDICOES[]", "\n// ---- the timeline check")
perfil = cut("#define CPU_AMOSTRAS", "// A finished direct-mode thumbnail")
check = cut("typedef struct { Job *j; long long hB; long durB; } CheckCopia;", "\n// Everything the direct engine made")
hist = cut("// ---- copy sources that keep failing", "\nstatic void jobAposFim")

prefix = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
typedef uint32_t Uint32;
#define FONTEFALHA_CHAVE 40
#define TENTATIVAS 12
#define PESADA_MBPS 20.0
#define RAZAO_1080 0.80
#define RAZAO_MESMA_RES 0.70
#define PISO_MBPS_1080 4.0
#define PISO_MBPS_720 2.0
#define PISO_MBPS_SD 1.0
#define TELA_DUR_IGUAL_MS 30000L
#define OTIMISTA_DUR_MS 4000L
#define DUR_PLAY 7200000LL

typedef struct {
  char rotulo[192], addonChave[FONTEFALHA_CHAVE], url[4096], arquivo[1024], descricao[2048], cabecalhos[512];
  int altura, naoVideo, foraCache, emCache, cacheNativo;
  long tamanhoMB;
  uint64_t tamanhoBytes;
} Stream;
typedef struct {
  char url[4096], cab[512];
  int altura;
  unsigned long long tamanho;
  char nome[160];
  char fonte[FONTEFALHA_CHAVE];
} Copia;
typedef struct { unsigned char *jpg; int n; long pts; unsigned char prov, salvo; unsigned revision; } Tile;
typedef struct { double a, b, erro; int inliers; } Mapa;
typedef struct {
  int alturaA, auditarRede, vivo;
  long dur;
  Copia copias[TENTATIVAS];
  int nCopias;
  Tile *tiles; int n;
  char motivos[2048], escolhida[256], arq[640];
  double offset;
  int appendPronto, feitos, primeiro;
  Uint32 cob60, cob30;
  int dCopia; double dA, dB; char dUrl[4096], dCab[512]; int dAltura;
  _Atomic int dCheck; pthread_t dCheckT; int dCheckRodando;
  char dFonte[FONTEFALHA_CHAVE];
  _Atomic int parar, pararFaixas;
} Job;

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static char testDir[600];
static char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", testDir, nome);
  return dst;
}
static Uint32 fakeTicks;
static Uint32 SDL_GetTicks(void) { return fakeTicks += 100; }
static int parou(const Job *j) { return j->parar; }
static void esperarPlayer(Job *j) { (void)j; }
static void diretoRebuildCoverage(Job *j) { (void)j; }

// The source list and the copy sessions, all synthetic.
static Stream lista[64]; static int nLista, iAtual;
static int stream_n(void) { return nLista; }
static const Stream *stream_item(int i) { return &lista[i]; }
static int stream_atual(void) { return iAtual; }
static int abrirMorto[64], abertos[64], fechados[64], telaRes[64], telaPara[64], telaChamadas[64], codecOk[64];
static long long durCopia[64];
static long long mAbrir(const char *url, const char *cab, int d) {
  (void)cab; (void)d;
  for (int i = 0; i < nLista; i++) if (!strcmp(lista[i].url, url)) { abertos[i]++; return abrirMorto[i] ? 0 : i + 1; }
  return 0;
}
static void mInfo(long long h, long long *ib) {
  int i = (int)h - 1;
  ib[0] = durCopia[i]; ib[1] = 1920; ib[2] = lista[i].altura; ib[3] = 0; ib[4] = 0; ib[5] = codecOk[i];
}
static void mAlvo(long long h, const char *url, char *out, size_t n) { (void)h; snprintf(out, n, "%s", url); }
static void mFechar(long long h) { if (h >= 1 && h <= nLista) fechados[(int)h - 1]++; }
static int conferirTela(Job *j, long long h, long durB, Mapa *mapa) {
  int i = (int)h - 1; (void)durB;
  telaChamadas[i]++;
  if (telaPara[i]) j->parar = 1;
  if (mapa) { mapa->a = 1.0; mapa->b = 0.0; mapa->erro = 0; mapa->inliers = 5; }
  return telaRes[i];
}

static void cenarioReset(void) {
  memset(lista, 0, sizeof lista); nLista = iAtual = 0;
  memset(abrirMorto, 0, sizeof abrirMorto); memset(abertos, 0, sizeof abertos);
  memset(fechados, 0, sizeof fechados); memset(telaRes, 0, sizeof telaRes);
  memset(telaPara, 0, sizeof telaPara); memset(telaChamadas, 0, sizeof telaChamadas);
  memset(codecOk, 0, sizeof codecOk); memset(durCopia, 0, sizeof durCopia);
}
static int addStream(const char *url, int altura, unsigned long long bytes, const char *fonte) {
  int i = nLista++;
  Stream *s = &lista[i];
  snprintf(s->url, sizeof s->url, "%s", url);
  snprintf(s->arquivo, sizeof s->arquivo, "file-%d.mkv", i);
  s->altura = altura; s->tamanhoBytes = bytes;
  snprintf(s->addonChave, sizeof s->addonChave, "%s", fonte);
  durCopia[i] = DUR_PLAY + 10000; codecOk[i] = 1; telaRes[i] = 1;
  return i;
}
static void play4k(void) { iAtual = addStream("http://h/play-4k", 2160, 40ULL << 30, "main"); }
static void dadosLimpar(void) {
  char p[700];
  dados_caminho(p, sizeof p, "miniaturas"); mkdir(p, 0700);
  char c[720];
  snprintf(c, sizeof c, "%s/perfil-cpu.txt", p); remove(c);
  snprintf(c, sizeof c, "%s/copias-fontes.txt", p); remove(c);
}
static void fontesTeste(const char *chave, int n, long idade) {
  char p[700]; dados_caminho(p, sizeof p, "miniaturas");
  char c[720]; snprintf(c, sizeof c, "%s/copias-fontes.txt", p);
  FILE *f = fopen(c, "w");
  if (chave) fprintf(f, "%s\t%d\t%ld\n", chave, n, (long)time(NULL) - idade);
  fclose(f);
}
'''

mid = r'''
static void perfilTeste(double ms4k, int n4k) {
  dadosLimpar();
  PerfilCpu pf = { { 500.0, ms4k }, { 3, n4k } };
  perfilGravar(&pf);
}
static Job *jobEscolhido(void) {
  Job *j = calloc(1, sizeof *j);
  j->alturaA = 2160; j->dur = DUR_PLAY;
  j->nCopias = escolherCopias(&lista[iAtual], j->copias, DUR_PLAY, j->motivos, sizeof j->motivos);
  return j;
}
'''

suffix = r'''
static int falhas;
#define CHECK(c, msg) do { if (c) printf("ok   %s\n", msg); else { printf("FAIL %s\n", msg); falhas++; } } while (0)

int main(int argc, char **argv) {
  (void)argc;
  snprintf(testDir, sizeof testDir, "%s", argv[1]);
  char motivos[2048];
  Copia out[TENTATIVAS];

  // ---- selection: 1080p backups come after every smaller copy; the heavy remux stays out; cap is 12 ----
  cenarioReset(); play4k();
  for (int k = 0; k < 9; k++) { char u[64]; snprintf(u, sizeof u, "http://h/720p-%d", k);
    addStream(u, 720, (unsigned long long)(2 + k) << 30, "s720"); }
  addStream("http://h/1080p-a", 1080, 15ULL << 30, "resv");
  addStream("http://h/1080p-b", 1080, 12ULL << 30, "resv");
  addStream("http://h/1080p-c", 1080, 14ULL << 30, "resv");
  addStream("http://h/1080p-d", 1080, 13ULL << 30, "resv");
  addStream("http://h/remux", 1080, 19ULL << 30, "remux");      // ~21 Mb/s over 2 h: a disc remux, out
  int n = escolherCopias(&lista[iAtual], out, DUR_PLAY, motivos, sizeof motivos);
  CHECK(n == 12, "9 primaries + 4 backups + 1 heavy: 12 of 13 eligible are tried");
  int okOrdem = 1;
  for (int k = 0; k < 9; k++) okOrdem &= out[k].altura == 720 && out[k].tamanho == (unsigned long long)(2 + k) << 30;
  CHECK(okOrdem, "the nine 720p copies come first, smallest size first");
  CHECK(out[9].altura == 1080 && out[9].tamanho == (12ULL << 30) &&
        out[10].tamanho == (13ULL << 30) && out[11].tamanho == (14ULL << 30),
        "1080p backups follow, sorted, the largest cut by the 12-attempt cap");
  CHECK(!strcmp(out[9].fonte, "resv"), "the copy keeps its add-on key");
  CHECK(strstr(motivos, "4 of them 1080p backups") && strstr(motivos, "heavy 1"),
        "motivos reports backups and the heavy rejection");

  // ---- selection: with no 720p at all the 1080p copies are primaries, not backups ----
  cenarioReset(); play4k();
  addStream("http://h/a", 1080, 6ULL << 30, "a");
  addStream("http://h/b", 1080, 5ULL << 30, "b");
  addStream("http://h/r", 1080, 19ULL << 30, "r");
  n = escolherCopias(&lista[iAtual], out, DUR_PLAY, motivos, sizeof motivos);
  CHECK(n == 2 && out[0].tamanho == (5ULL << 30) && out[1].tamanho == (6ULL << 30),
        "no 720p: 1080p copies are tried as primaries, smallest first");
  CHECK(strstr(motivos, "0 of them 1080p backups") && strstr(motivos, "heavy 1"),
        "no backups without a 720p baseline; the remux is still out");

  // ---- selection: unknown-size 1080p is a backup too, sorted last inside the group ----
  cenarioReset(); play4k();
  addStream("http://h/720", 720, 4ULL << 30, "s");
  addStream("http://h/big", 1080, 10ULL << 30, "r");
  Stream *u = &lista[addStream("http://h/unk", 1080, 0, "r")]; u->tamanhoBytes = 0; u->tamanhoMB = 0;
  addStream("http://h/small", 1080, 8ULL << 30, "r");
  n = escolherCopias(&lista[iAtual], out, DUR_PLAY, motivos, sizeof motivos);
  CHECK(n == 4 && out[0].altura == 720 &&
        out[1].tamanho == (8ULL << 30) && out[2].tamanho == (10ULL << 30) && out[3].tamanho == 0,
        "unknown-size 1080p goes to the backup group, after the known sizes");

  // ---- history: six dead 720p of one add-on in THIS title do not skip its own 1080p backup ----
  cenarioReset(); perfilTeste(700, 5);                 // fast device: 700 ms per 4K CPU thumbnail
  fontesTeste(NULL, 0, 0);                             // no history yet
  play4k();
  int idx[7], reserva;
  for (int k = 0; k < 6; k++) { char b[64]; snprintf(b, sizeof b, "http://h/dead-%d", k);
    idx[k] = addStream(b, 720, (unsigned long long)(3 + k) << 30, "dead"); abrirMorto[idx[k]] = 1; }
  reserva = addStream("http://h/dead-1080", 1080, 12ULL << 30, "dead");
  durCopia[reserva] = DUR_PLAY + 10000; telaRes[reserva] = 0;    // opens, the screen check rejects it
  Job *j = jobEscolhido();
  CHECK(j->nCopias == 7 && j->copias[6].altura == 1080, "six 720p + their 1080p backup selected");
  int r = escolherCopiaRapida(j);
  int todos = 1;
  for (int k = 0; k < 6; k++) todos &= abertos[idx[k]] == 1;
  CHECK(r == 0 && todos, "same title: all six dead copies were tried despite the mounting failures");
  CHECK(abertos[reserva] == 1 && telaChamadas[reserva] == 1,
        "the title's own 1080p backup is still tried after six same-source failures");
  CHECK(fonteTentativas("dead") == 7, "six no-open + one screen rejection recorded against the source");
  // Next title, same device: the source is now skipped, every copy of it.
  memset(abertos, 0, sizeof abertos); memset(telaChamadas, 0, sizeof telaChamadas);
  j->motivos[0] = 0;
  r = escolherCopiaRapida(j);
  int nenhum = 1;
  for (int k = 0; k < 6; k++) nenhum &= !abertos[idx[k]];
  nenhum &= !abertos[reserva];
  CHECK(r == 0 && nenhum, "next title on a fast device: a source at 6+ failures is skipped entirely");
  CHECK(strstr(j->motivos, "source-skipped"), "the skip is recorded in motivos");
  free(j);

  // ---- history: the skip only happens on a fast device ----
  cenarioReset(); perfilTeste(3000, 5);                // slow device: 3 s per 4K CPU thumbnail
  fontesTeste("dead", 6, 0);
  play4k();
  int d1 = addStream("http://h/d1", 720, 3ULL << 30, "dead"); abrirMorto[d1] = 1;
  int g1 = addStream("http://h/g1", 720, 3500ULL << 20, "good");   // opens, accepted by the blocking check
  j = jobEscolhido();
  r = escolherCopiaRapida(j);
  CHECK(r == 1 && abertos[d1] == 1 && abertos[g1] == 1,
        "slow device: even a source at 6 failures is tried, then the good copy is accepted");
  CHECK(fonteTentativas("dead") == 7, "the failed open adds to the source's count");
  free(j);

  // ---- history: the network audit run tries everything ----
  cenarioReset(); perfilTeste(700, 5);
  fontesTeste("dead", 6, 0);
  play4k();
  d1 = addStream("http://h/d1", 720, 3ULL << 30, "dead"); abrirMorto[d1] = 1;
  g1 = addStream("http://h/g1", 720, 3500ULL << 20, "good");
  j = jobEscolhido(); j->auditarRede = 1;
  r = escolherCopiaRapida(j);
  CHECK(r == 1 && abertos[d1] == 1, "audit mode: the failing source is tried anyway");
  free(j);

  // ---- history: entries older than three days are forgotten ----
  cenarioReset(); perfilTeste(700, 5);
  fontesTeste("dead", 6, 4L * 86400);
  play4k();
  d1 = addStream("http://h/d1", 720, 3ULL << 30, "dead"); abrirMorto[d1] = 1;
  g1 = addStream("http://h/g1", 720, 3500ULL << 20, "good");
  j = jobEscolhido();
  r = escolherCopiaRapida(j);
  CHECK(r == 1 && abertos[d1] == 1, "expired history (4 days old): the source is tried again");
  free(j);

  // ---- history: an accepted copy clears its source ----
  cenarioReset(); perfilTeste(3000, 5);                // slow, so a source at 7 is still tried
  fontesTeste("good", 7, 0);
  play4k();
  g1 = addStream("http://h/g1", 720, 3ULL << 30, "good");
  j = jobEscolhido();
  r = escolherCopiaRapida(j);
  CHECK(r == 1 && fonteTentativas("good") == 0, "an accepted copy resets the source's count");
  free(j);
  perfilTeste(700, 5);                                 // fast now: count 0, it is still tried
  memset(abertos, 0, sizeof abertos);
  j = jobEscolhido();
  r = escolherCopiaRapida(j);
  CHECK(r == 1 && abertos[g1] == 1, "the cleared source is tried again on the next title");
  free(j);

  // ---- history: the optimistic check's verdict is recorded by its thread ----
  cenarioReset(); perfilTeste(700, 5);
  fontesTeste("opt", 3, 0);
  play4k();
  g1 = addStream("http://h/opt", 1080, 8ULL << 30, "opt");
  durCopia[g1] = DUR_PLAY;                             // same length: optimistic start, check on its own thread
  j = jobEscolhido();
  r = escolherCopiaRapida(j);
  CHECK(r == 1 && j->dCheckRodando, "same-length copy: optimistic start, check runs alongside");
  pthread_join(j->dCheckT, NULL);
  CHECK(j->dCheck == 2 && fonteTentativas("opt") == 0, "optimistic ACCEPT: the source's count is cleared");
  free(j);

  cenarioReset(); perfilTeste(700, 5);
  fontesTeste("opt", 3, 0);
  play4k();
  g1 = addStream("http://h/opt", 1080, 8ULL << 30, "opt");
  durCopia[g1] = DUR_PLAY; telaRes[g1] = 0;
  j = jobEscolhido();
  r = escolherCopiaRapida(j);
  pthread_join(j->dCheckT, NULL);
  CHECK(r == 1 && j->dCheck == 3 && fonteTentativas("opt") == 4, "optimistic REJECT: the failure is recorded");
  free(j);

  // ---- history: a check cut short by the title ending decides nothing ----
  cenarioReset(); perfilTeste(700, 5);
  fontesTeste("opt", 3, 0);
  play4k();
  g1 = addStream("http://h/opt", 1080, 8ULL << 30, "opt");
  durCopia[g1] = DUR_PLAY; telaRes[g1] = -1; telaPara[g1] = 1;   // screen gives no frames AND the job is canceled
  j = jobEscolhido();
  r = escolherCopiaRapida(j);
  pthread_join(j->dCheckT, NULL);
  CHECK(r == 1 && fonteTentativas("opt") == 3, "canceled without a verdict: the source is not penalized");
  free(j);

  // ... but a real no-screen verdict does count.
  cenarioReset(); perfilTeste(700, 5);
  fontesTeste("opt", 3, 0);
  play4k();
  g1 = addStream("http://h/opt", 1080, 8ULL << 30, "opt");
  durCopia[g1] = DUR_PLAY; telaRes[g1] = -1;                    // no cancel this time
  j = jobEscolhido();
  r = escolherCopiaRapida(j);
  pthread_join(j->dCheckT, NULL);
  CHECK(r == 1 && fonteTentativas("opt") == 4, "no-screen verdict without cancel: the failure counts");
  free(j);

  // ---- history: the file holds keys and counters, no URLs ----
  {
    char p[700], c[720], buf[4096]; int nl = 0;
    dados_caminho(p, sizeof p, "miniaturas");
    snprintf(c, sizeof c, "%s/copias-fontes.txt", p);
    FILE *f = fopen(c, "r");
    CHECK(f != NULL, "the history file exists after the runs");
    if (f) { nl = (int)fread(buf, 1, sizeof buf - 1, f); buf[nl] = 0; fclose(f); }
    CHECK(nl > 0 && !strstr(buf, "http") && !strstr(buf, "//"), "persisted history has no URLs or credentials");
  }

  if (falhas) { printf("FAIL: %d copy-backup checks\n", falhas); return 1; }
  puts("PASS: 1080p backups, 12-attempt cap, heavy cut, per-source history with expiry and cancel safety");
  return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="nuvio-copy-backup-") as tmp:
    c = Path(tmp) / "backup.c"
    c.write_text(prefix + sel + perfil + check + hist + mid + suffix)
    exe = Path(tmp) / "backup"
    dados = Path(tmp) / "dados"
    dados.mkdir()
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-format-truncation",
                    "-fsanitize=address,undefined", "-g",
                    "-DNV_MINI_FFMPEG", str(c), "-pthread", "-o", str(exe)], check=True)
    sys.exit(subprocess.run([str(exe), str(dados)], timeout=60).returncode)
