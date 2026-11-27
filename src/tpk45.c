// Tizen 4-5.5 .tpk: memoria da conversao DTS/TrueHD. Contrato em tpk45.h.
#include "tpk45.h"
#include <stdio.h>
#include <string.h>

#ifndef NV_TPK40

// Fora da .so do NuvioTpk40 (hosts 6+, LG, Android, desktop): nada muda.
int tpk45_ativo(void) { return 0; }
int tpk45_perfil(void) { return TPK45_PERFIL_NORMAL; }
int tpk45_perfil_efetivo(void) { return TPK45_PERFIL_NORMAL; }
int tpk45_entrega(void) { return TPK45_ENTREGA_HLS; }
const char *tpk45_marca(void) { return ""; }
int tpk45_anel_mib(void) { return 0; }
int tpk45_fila_mib(void) { return 0; }
int tpk45_aperto(long livreKb, int *apertado) { (void)livreKb; (void)apertado; return 0; }
void tpk45_tv(void) {}
void tpk45_tick(int convertendo, const char *modo) { (void)convertendo; (void)modo; }

#else

#include "dts/dts_engine.h"
#include "dts/dts_remux.h"
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

// fcntl64 DENTRO da lib do 4/5. O FFmpeg e compilado com arquivos de 64 bits,
// e na libc da build (2.28) isso troca cada fcntl() dele por fcntl64, funcao
// que nasceu nessa mesma 2.28. A libc das TVs Tizen 4/5 e mais antiga: nao tem
// esse nome, o carregador ELF do Program40.cs nao acharia o simbolo e o app nem
// abriria (tools/tpk.sh barra a build por isso). Definida aqui, a referencia
// do FFmpeg fecha dentro da propria lib e nada novo e pedido a TV.
// E o que a libc faz: a syscall fcntl64 do kernel ARM de 32 bits, que existe
// desde o 2.4 (o resto do app segue no fcntl de sempre). O FFmpeg so a usa com
// F_SETFD, F_SETFL/F_GETFL e F_DUPFD_CLOEXEC.
#ifdef SYS_fcntl64
__attribute__((visibility("hidden"))) int fcntl64(int fd, int cmd, ...) {
  va_list ap;
  void *arg;
  va_start(ap, cmd);
  arg = va_arg(ap, void *);
  va_end(ap);
  return (int)syscall(SYS_fcntl64, fd, cmd, arg);
}
#endif

// Os tres perfis. "Normal" e o que as TVs Tizen 6-7 ja usam (dts_hls_tpk.c:
// anel 16 MiB, fila 128 MiB). O anel aceita
// 8..32 MiB e a fila 32..512 MiB (dts_engine.c, dts_remux.c).
// O ANEL e a memoria de verdade (um por leitor vivo, ate tres). A FILA e so um
// teto: no uso normal ela guarda poucos MB (o intervalo entre audio e video no
// arquivo) e bater no teto e ERRO do conversor ("Queue memory budget reached"),
// nao pausa. Por isso os perfis baixam o anel a serio e a fila com cuidado.
static const struct {
  const char *nome;
  int anel, fila;        // conversor, MiB
} P[TPK45_PERFIS] = {
  [TPK45_PERFIL_AUTO]   = { "auto",    0,   0 },
  [TPK45_PERFIL_NORMAL] = { "normal", 16, 128 },
  [TPK45_PERFIL_LEVE]   = { "light",   8,  96 },
  [TPK45_PERFIL_MINIMO] = { "minimum", 8,  64 },
};
static const char *const E[TPK45_ENTREGAS] = { "auto", "HLS only", "continuous TS only" };

// AUTO pela memoria LIVRE (MemAvailable) de agora. Na S95C o conversor rodou
// bem com 201-375 MB livres e valores maiores que "normal"; aqui a escada desce
// antes. A TV de 2020 do primeiro teste (1337 MB) ficou em 300-460 MB livres.
#define AUTO_NORMAL_MB 400
#define AUTO_LEVE_MB   250
// Aperto: o piso do app (memlog.h, 150 MB); sai dele com folga para nao oscilar.
#define APERTO_MB  150
#define FOLGA_MB   220

static long lerKb(const char *arq, const char *chave) {
  char l[160];
  size_t k = strlen(chave);
  long v = -1;
  FILE *f = fopen(arq, "r");
  if (!f) return -1;
  while (fgets(l, sizeof l, f))
    if (!strncmp(l, chave, k) && l[k] == ':') { v = atol(l + k + 1); break; }
  fclose(f);
  return v;
}
static long livreMb(void) { long v = lerKb("/proc/meminfo", "MemAvailable"); return v < 0 ? -1 : v / 1024; }

int tpk45_ativo(void) { return 1; }
int tpk45_perfil(void) { return TPK45_PERFIL_AUTO; }
int tpk45_entrega(void) { return TPK45_ENTREGA_AUTO; }

// O Auto resolvido. A leitura vale 5 s: quem pergunta muitas vezes (o menu)
// nao reabre o /proc a cada vez, e o perfil nao troca no meio de uma conta.
int tpk45_perfil_efetivo(void) {
  static atomic_int cache = TPK45_PERFIL_LEVE;
  static atomic_uint lidoEm;
  int p = tpk45_perfil();
  unsigned agora = SDL_GetTicks();
  if (p != TPK45_PERFIL_AUTO) return p;
  if (!atomic_load(&lidoEm) || (int)(agora - atomic_load(&lidoEm)) > 5000) {
    long mb = livreMb();
    atomic_store(&cache, mb < 0 ? TPK45_PERFIL_LEVE
                       : mb >= AUTO_NORMAL_MB ? TPK45_PERFIL_NORMAL
                       : mb >= AUTO_LEVE_MB ? TPK45_PERFIL_LEVE : TPK45_PERFIL_MINIMO);
    atomic_store(&lidoEm, agora ? agora : 1u);
  }
  return atomic_load(&cache);
}

// Todas as combinacoes montadas uma vez: o ponteiro devolvido vale para sempre,
// em qualquer fio (o fio da memoria do conversor tambem loga com ele).
static char marcas[TPK45_PERFIS][TPK45_PERFIS][TPK45_ENTREGAS][64];
static pthread_once_t marcasUma = PTHREAD_ONCE_INIT;
static void marcasMontar(void) {
  for (int p = 0; p < TPK45_PERFIS; p++)
    for (int ef = 1; ef < TPK45_PERFIS; ef++)
      for (int e = 0; e < TPK45_ENTREGAS; e++)
        snprintf(marcas[p][ef][e], sizeof marcas[p][ef][e], "profile %s%s, delivery %s", P[ef].nome,
                 p == TPK45_PERFIL_AUTO ? " (auto)" : "", E[e]);
}
const char *tpk45_marca(void) {
  pthread_once(&marcasUma, marcasMontar);
  return marcas[tpk45_perfil()][tpk45_perfil_efetivo()][tpk45_entrega()];
}

int tpk45_anel_mib(void) { return P[tpk45_perfil_efetivo()].anel; }
int tpk45_fila_mib(void) { return P[tpk45_perfil_efetivo()].fila; }
int tpk45_aperto(long livreKb, int *apertado) {
  long mb = livreKb < 0 ? -1 : livreKb / 1024;
  if (mb >= 0 && !*apertado && mb < APERTO_MB) { *apertado = 1; return 1; }
  if (*apertado && mb >= FOLGA_MB) { *apertado = 0; return -1; }
  return 0;
}


// --- log ---------------------------------------------------------------------
static void sysinfo(const char *chave, char *dst, size_t n) {
  static int (*get)(const char *, char **);
  static int tentou;
  char *t = NULL;
  snprintf(dst, n, "?");
  if (!tentou) {
    static const char *const libs[] = { "libcapi-system-info.so.0", "libcapi-system-info.so", NULL };
    void *h = NULL;
    tentou = 1;
    for (int i = 0; libs[i] && !h; i++) h = dlopen(libs[i], RTLD_NOW);
    if (h) *(void **)&get = dlsym(h, "system_info_get_platform_string");
  }
  if (get && get(chave, &t) == 0 && t) { snprintf(dst, n, "%s", t); free(t); }
}
// Como a lib entrou (Program40.cs): memfd aparece no mapa do processo; o
// carregador ELF proprio so deixa memoria anonima.
static const char *carregador(void) {
  char l[512];
  const char *r = "ELF loader (anonymous memory)";
  FILE *f = fopen("/proc/self/maps", "r");
  if (!f) return "?";
  while (fgets(l, sizeof l, f)) if (strstr(l, "memfd:")) { r = "memfd"; break; }
  fclose(f);
  return r;
}
// So o fio do desenho chama (tpk45_tick): uma vez por execucao.
void tpk45_tv(void) {
  static int feito;
  char ver[32], modelo[64], build[96];
  if (feito) return;
  feito = 1;
  sysinfo("http://tizen.org/feature/platform.version", ver, sizeof ver);
  sysinfo("http://tizen.org/system/model_name", modelo, sizeof modelo);
  sysinfo("http://tizen.org/system/build.string", build, sizeof build);
  printf("[tpk45] tv: tizen %s, model %s, build %s, RAM %ld MB, free %ld MB, %ld core(s), library by %s, FFmpeg %s; %s\n",
         ver, modelo, build, lerKb("/proc/meminfo", "MemTotal") / 1024, livreMb(), sysconf(_SC_NPROCESSORS_ONLN),
         carregador(), dts_remux_available() ? "linked" : "missing", tpk45_marca());
  printf("[tpk45] profiles: normal ring 16 queue cap 128 MiB | light 8 / 96 | minimum 8 / 64 | auto: free >= %d MB normal, "
         ">= %d MB light, else minimum | low-memory line under %d MB free | seek-bar thumbnails on (slow-TV limit, [miniatura])\n",
         AUTO_NORMAL_MB, AUTO_LEVE_MB, APERTO_MB);
  fflush(stdout);
}
void tpk45_tick(int convertendo, const char *modo) {
  static Uint32 ultimo;
  Uint32 agora = SDL_GetTicks();
  DtsEngineRings rg;
  long rss, swap;
  if (ultimo && (Sint32)(agora - ultimo) < 10000) return;
  ultimo = agora ? agora : 1u;
  tpk45_tv();
  rss = lerKb("/proc/self/status", "VmRSS");
  swap = lerKb("/proc/meminfo", "SwapFree");
  dts_engine_rings(&rg, 0);
  printf("[tpk45] 10s: free %ld MB, swap free %ld MB, app %ld MB; converter %s%s%s, rings %d = %lld MiB, queue %lld of %d MiB; "
         "%s\n",
         livreMb(), swap < 0 ? -1 : swap / 1024, rss < 0 ? -1 : rss / 1024,
         convertendo ? "on" : "off", convertendo && modo ? ", " : "", convertendo && modo ? modo : "",
         rg.alive, rg.bytes >> 20, (long long)(dts_remux_queued_bytes() >> 20), dts_remux_queue_budget_mib(), tpk45_marca());
  fflush(stdout);
}
#endif
