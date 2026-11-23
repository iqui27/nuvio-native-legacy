"""Copy-first verdicts and history writes under failure (PR 729, revisão r2 F76).

Production C cut from src/miniatura_android.c. Program "cancel" runs the REAL conferirTela (screen
stubs decide when the job's parar goes up) through escolherCopiaRapida on both paths - the optimistic
check thread and the blocking check - over seven titles on a fast device: a check cut short by the
title ending must decide NOTHING, so the source is never penalized and every copy is still tried.
Real rejections and real no-screen verdicts must still count. Program "io" forces a short write with
RLIMIT_FSIZE (buffered and unbuffered) and a failing rename on the real copias-fontes.txt writer: no
descriptor may leak, the .tmp must always be removed and the previous history must stay byte for byte.
"""
from pathlib import Path
import re
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
tela = cut("static int conferirTela(", "\n// One re-check")
tela_consts = "\n".join(
    re.findall(r"^#define TELA_(?:ANCORAS|MIN_CORR|ESPACO_MS|ACORDO_MS|PRAZO_MS) .+$", text, re.MULTILINE))
hist_io = hist[:hist.index("static int aparelhoRapido")]

prefix = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <math.h>
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
#define SIG_N 1
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
  long dur, ultimoBom;
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
static int abrirMorto[64], abertos[64], fechados[64], codecOk[64];
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

// The screen, the copy's frames and the anchor search, all synthetic. telaProximo makes each screen
// frame 10 s later than the last so TELA_ESPACO_MS never rejects it. What each flag does is in main().
typedef struct { double pa, pb; } Par;
static int scrubbing, anchors, telaMorta, rejeita, dormirCancela;
static long telaProximo = 10000;
static void dormir(Job *j, int ms) { (void)ms; if (dormirCancela) j->parar = 1; }
static int mTela(long *pts, float *sig, int *ok) {
  if (telaMorta) return 0;
  *pts = telaProximo; telaProximo += 10000; sig[0] = 1; *ok = 1; return 1;
}
static void mPreparar(long long h, long *ts, int n) { (void)h; (void)ts; (void)n; }
static int mQuadro(Job *j, long long h, long req, int s, int max, long *p, float *sig, int *ok, void *a, void *b) {
  (void)j; (void)h; (void)s; (void)max; (void)a; (void)b;
  *p = req; sig[0] = 1; *ok = 1; return 1;
}
static int falhou(Job *j, int lido, long long h) { (void)j; (void)lido; (void)h; return 0; }
static int picoExato(Job *j, long long h, float *sig, double p, const long *off, int n, double min, double *t, double *r) {
  (void)h; (void)sig; (void)p; (void)off; (void)n; (void)min;
  *t = 10000; *r = 1; ++anchors;
  if (rejeita) return 0;
  j->parar = 1;  // the title ends right after the first matching anchor, before the next iteration
  return 1;
}
static double mediana(double *values, int n) { return values[n / 2]; }
'''

tail = r'''
static void cenarioReset(void) {
  memset(lista, 0, sizeof lista); nLista = iAtual = 0;
  memset(abrirMorto, 0, sizeof abrirMorto); memset(abertos, 0, sizeof abertos);
  memset(fechados, 0, sizeof fechados); memset(codecOk, 0, sizeof codecOk);
  memset(durCopia, 0, sizeof durCopia);
  anchors = telaMorta = rejeita = dormirCancela = 0; telaProximo = 10000;
}
static int addStream(const char *url, int altura, unsigned long long bytes, const char *fonte) {
  int i = nLista++;
  Stream *s = &lista[i];
  snprintf(s->url, sizeof s->url, "%s", url);
  snprintf(s->arquivo, sizeof s->arquivo, "file-%d.mkv", i);
  s->altura = altura; s->tamanhoBytes = bytes;
  snprintf(s->addonChave, sizeof s->addonChave, "%s", fonte);
  durCopia[i] = DUR_PLAY + 10000; codecOk[i] = 1;
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

main_cancel = r'''
static int falhas;
#define CHECK(c, msg) do { if (c) printf("ok   %s\n", msg); else { printf("FAIL %s\n", msg); falhas++; } } while (0)

int main(int argc, char **argv) {
  (void)argc;
  snprintf(testDir, sizeof testDir, "%s", argv[1]);

  // ---- the optimistic path: the check THREAD is cut by the title ending, seven titles in a row ----
  // Same runtime -> the copy starts now and conferirTela() runs on its own thread. picoExato sets
  // parar after the first matching anchor, so the check returns 0 (only one anchor) WITH parou: no
  // verdict, nothing may be recorded. On a fast device six bogus failures would skip the 7th copy.
  perfilTeste(700, 5);                        // fast device: a source at 6 failures is skipped
  int launched = 0, abriu7 = 1;
  for (int i = 0; i < 7; ++i) {
    cenarioReset(); play4k();
    char url[100]; snprintf(url, sizeof url, "http://h/title-%d-copy", i);
    int k = addStream(url, 1080, 8ULL << 30, "healthy");
    durCopia[k] = DUR_PLAY;                   // same length: optimistic start, check on its own thread
    Job *j = jobEscolhido();
    escolherCopiaRapida(j);
    abriu7 &= abertos[k] == 1;
    if (j->dCheckRodando) { pthread_join(j->dCheckT, NULL); ++launched; }
    printf("title=%d open=%d source_failures=%d\n", i + 1, abertos[k], fonteTentativas("healthy"));
    free(j);
  }
  CHECK(launched == 7 && abriu7, "optimistic check canceled after one anchor: all seven copies still tried");
  CHECK(fonteTentativas("healthy") == 0, "canceled without a verdict (r=0): the source is not penalized");

  // ---- the blocking path: conferirTela() returns 0 with parou inside escolherCopiaRapida itself ----
  perfilTeste(700, 5);
  abriu7 = 1;
  for (int i = 0; i < 7; ++i) {
    cenarioReset(); play4k();
    char url[100]; snprintf(url, sizeof url, "http://h/title-%d-copy", i);
    int k = addStream(url, 1080, 8ULL << 30, "healthy");      // dur = DUR_PLAY + 10 s: blocking check
    Job *j = jobEscolhido();
    escolherCopiaRapida(j);
    abriu7 &= abertos[k] == 1;
    printf("title=%d open=%d source_failures=%d\n", i + 1, abertos[k], fonteTentativas("healthy"));
    free(j);
  }
  CHECK(abriu7, "blocking check canceled after one anchor: all seven copies still tried");
  CHECK(fonteTentativas("healthy") == 0, "blocking r=0 under cancel: the source is not penalized");

  // ---- the blocking path with -1 after cancel: the screen dies and the title ends together ----
  perfilTeste(700, 5);
  abriu7 = 1;
  for (int i = 0; i < 7; ++i) {
    cenarioReset(); play4k();
    telaMorta = 1; dormirCancela = 1;          // no screen frame ever; the first wait ends the title
    char url[100]; snprintf(url, sizeof url, "http://h/title-%d-copy", i);
    int k = addStream(url, 1080, 8ULL << 30, "healthy");
    Job *j = jobEscolhido();
    escolherCopiaRapida(j);
    abriu7 &= abertos[k] == 1;
    printf("title=%d open=%d source_failures=%d\n", i + 1, abertos[k], fonteTentativas("healthy"));
    free(j);
  }
  CHECK(abriu7, "blocking check canceled with no screen frames: all seven copies still tried");
  CHECK(fonteTentativas("healthy") == 0, "blocking r=-1 under cancel: the source is not penalized");

  // ---- real verdicts still count: an uncanceled rejection and an uncanceled no-screen DO record ----
  perfilTeste(700, 5);
  cenarioReset(); play4k(); rejeita = 1;       // every anchor search comes back empty-handed
  addStream("http://h/bad-copy", 1080, 8ULL << 30, "healthy");
  Job *j = jobEscolhido();
  escolherCopiaRapida(j);
  CHECK(fonteTentativas("healthy") == 1, "a real rejection without cancel still counts against the source");
  free(j);

  cenarioReset(); play4k(); telaMorta = 1;     // screen gives no frames, no cancel this time
  addStream("http://h/bad-copy2", 1080, 8ULL << 30, "healthy");
  j = jobEscolhido();
  escolherCopiaRapida(j);
  CHECK(fonteTentativas("healthy") == 2, "a real no-screen verdict without cancel still counts");
  free(j);

  if (falhas) { printf("FAIL: %d verdict checks\n", falhas); return 1; }
  puts("PASS: a canceled check records nothing; real rejections still count");
  return 0;
}
'''

io_stubs = r'''
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
static int unbuffered, fail_rename;
static FILE *probe_fopen(const char *path, const char *mode) {
  FILE *f = fopen(path, mode);
  if (f && unbuffered && strstr(path, ".tmp")) assert(setvbuf(f, NULL, _IONBF, 0) == 0);
  return f;
}
static int probe_rename(const char *from, const char *to) {
  if (fail_rename) { errno = EIO; return -1; }
  return rename(from, to);
}
#define fopen probe_fopen
#define rename probe_rename
'''

main_io = r'''
#undef fopen
#undef rename
static int falhas;
#define CHECK(c, msg) do { if (c) printf("ok   %s\n", msg); else { printf("FAIL %s\n", msg); falhas++; } } while (0)
static int fd_count(void) {
  DIR *d = opendir("/proc/self/fd"); assert(d);
  int n = 0;
  while (readdir(d)) n++;
  closedir(d);
  return n;
}
static void contents(const char *path, char *buf, size_t len) {
  FILE *f = fopen(path, "r"); assert(f);
  size_t n = fread(buf, 1, len - 1, f); buf[n] = 0;
  assert(fclose(f) == 0);
}
int main(int argc, char **argv) {
  (void)argc;
  snprintf(testDir, sizeof testDir, "%s", argv[1]);
  char path[720], tmp[730], old[128], actual[128], dir[700];
  dados_caminho(dir, sizeof dir, "miniaturas");
  assert(mkdir(dir, 0700) == 0);
  snprintf(path, sizeof path, "%s/copias-fontes.txt", dir);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  fonteAnotar("source", 0);
  assert(fonteTentativas("source") == 1);
  contents(path, old, sizeof old);
  struct rlimit current, cap;
  assert(getrlimit(RLIMIT_FSIZE, &current) == 0);
  cap = current; cap.rlim_cur = 4;
  assert(signal(SIGXFSZ, SIG_IGN) != SIG_ERR);

  assert(setrlimit(RLIMIT_FSIZE, &cap) == 0);
  fonteAnotar("source", 0);                       // buffered: the failure comes from fclose()
  contents(path, actual, sizeof actual);
  int close_ok = !strcmp(old, actual) && access(tmp, F_OK) != 0;
  CHECK(close_ok, "fclose failure: the old history is preserved and the .tmp is removed");

  unbuffered = 1;                                 // the short write now fails inside fprintf()
  int before = fd_count();
  for (int i = 0; i < 12; ++i) fonteAnotar("source", 0);
  int leaked = fd_count() - before;
  contents(path, actual, sizeof actual);
  int write_ok = !strcmp(old, actual) && access(tmp, F_OK) != 0;
  CHECK(write_ok, "12 mid-write failures: the old history is preserved and the .tmp is removed");
  CHECK(leaked == 0, "12 mid-write failures: no FILE*/fd leaks (each attempt closed its file)");
  assert(setrlimit(RLIMIT_FSIZE, &current) == 0);
  unbuffered = 0;

  fail_rename = 1;
  fonteAnotar("source", 0);
  contents(path, actual, sizeof actual);
  CHECK(!strcmp(old, actual) && access(tmp, F_OK) != 0,
        "rename failure: the old history is preserved and the .tmp is removed");

  if (falhas) { printf("FAIL: %d history-write checks\n", falhas); return 1; }
  puts("PASS: history writes never leak a descriptor and the .tmp is always cleaned");
  return 0;
}
'''


def build_and_run(name, source, tmp):
    rc = 0
    for mode, flags in (("normal", []), ("asan", ["-fsanitize=address,undefined"])):
        c = Path(tmp) / f"{name}-{mode}.c"
        exe = Path(tmp) / f"{name}-{mode}"
        dados = Path(tmp) / f"dados-{name}-{mode}"
        c.write_text(source)
        dados.mkdir()
        subprocess.run(["cc", "-std=c11", "-g", "-Wall", "-Wextra",
                        "-Wno-unused-function", "-Wno-unused-variable",
                        "-DNV_MINI_FFMPEG", *flags, str(c), "-pthread", "-lm",
                        "-o", str(exe)], check=True)
        r = subprocess.run([str(exe), str(dados)], timeout=60)
        print(f"{name} ({mode}): rc={r.returncode}")
        rc = rc or r.returncode
    return rc


with tempfile.TemporaryDirectory(prefix="nuvio-copy-verdict-") as tmp:
    cancel_src = prefix + tela_consts + "\n" + tela + tail + sel + perfil + check + hist + mid + main_cancel
    io_src = prefix + tail + io_stubs + hist_io + main_io
    rc = build_and_run("cancel", cancel_src, tmp) | build_and_run("io", io_src, tmp)
    sys.exit(rc)
