// FAROL38C — varredura e gravacoes fora do fio da UI.
//
// Cenario da revisao r2: 700 entradas com o MESMO `quando`. O corte antigo
// do cbVar so segurava a leva quando o instante seguinte era diferente —
// 700 callbacks numa passada so, no fio chamador, e o mutex do Diario preso
// a iteracao toda. Depois da correcao:
//   * cada passada entrega NO MAXIMO 513 (teto 512 + a entrega recusada
//     que o para), mesmo em empate de `quando` — o cursor composto
//     (quando + posicao no grupo) avanca dentro da leva;
//   * a varredura termina nas passadas seguintes, sem perder nem repetir
//     entrada (cada id passa pelo cat_indice exatamente uma vez);
//   * nenhum fopen de ESCRITA volta ao fio chamador — conferidas, largada
//     e gravacoes rolam no fio de tras;
//   * um estado com linha "p" truncada depois de uma valida e rejeitado
//     INTEIRO (ressalva barata da r2).
// Reusa os falsos do farol_sinal.c por inclusao, como a sonda extra da
// revisao r2 — o que aqui e main la vira farol_suite_main.
#define main farol_suite_main
#define diario_percorrer sonda_percorrer_fake
#define cat_indice_por_imdb sonda_cat_idx_fake
#include "farol_sinal.c"
#undef cat_indice_por_imdb
#undef diario_percorrer
#undef main

#define VEZ_MAX 512            // FSIN_VEZ_MAX de src/farolsinal.c

static pthread_t V_fioUi;      // o fio chamador (a "UI" do teste)
static _Atomic int V_maior;    // maior entrega de uma so chamada a percorrer
static _Atomic int V_fopenUi;  // fopen de escrita no fio chamador
static pthread_mutex_t V_mtx = PTHREAD_MUTEX_INITIALIZER;
static char V_ids[1024][DIARIO_ID_MAX];
static int V_nIds;             // ids que passaram por cat_indice (1 por entrada)

// fopen de escrita no fio chamador e contado. No ELF entra por
// -Wl,--wrap,fopen; o ld64 do Mach-O nao tem --wrap, entao no __APPLE__ o
// proprio binario define fopen (interposicao para as chamadas dos nossos
// objetos) e o RTLD_NEXT alcanca o fopen de verdade da libc — a mesma
// receita do srand interposto em tests/uuid_concorrente.c.
static FILE *portaoFopen(FILE *(*real)(const char *, const char *),
                       const char *c, const char *m) {
  if (m && (m[0] == 'w' || m[0] == 'a' || strchr(m, '+')) &&
      pthread_equal(pthread_self(), V_fioUi))
    V_fopenUi++;
  return real(c, m);
}
#if defined(__APPLE__)
#include <dlfcn.h>
FILE *fopen(const char *c, const char *m) {
  FILE *(*real)(const char *, const char *);
  real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen");
  return portaoFopen(real, c, m);
}
#else
FILE *__real_fopen(const char *c, const char *m);
FILE *__wrap_fopen(const char *c, const char *m) { return portaoFopen(__real_fopen, c, m); }
#endif
int diario_percorrer(int perfil, const DiarioFiltro *f, DiarioVisita cb,
                     void *ctx) {
  int k = sonda_percorrer_fake(perfil, f, cb, ctx);
  int v;
  do { v = V_maior; } while (k > v &&
    !atomic_compare_exchange_weak(&V_maior, &v, k));
  return k;
}
int cat_indice_por_imdb(const char *imdb) {
  pthread_mutex_lock(&V_mtx);
  if (V_nIds < 1024)
    { snprintf(V_ids[V_nIds], DIARIO_ID_MAX, "%s", imdb ? imdb : "");
      V_nIds++; }
  pthread_mutex_unlock(&V_mtx);
  return sonda_cat_idx_fake(imdb);
}

int main(void) {
  DiarioEntrada e;
  int i, v1, v2, antes;
  char cam[640], id[64];
  FILE *fp;
  struct stat st;

  V_fioUi = pthread_self();
  snprintf(T_dir, sizeof T_dir, "%s/nuvio-farol-varre-XXXXXX",
           getenv("TMPDIR") && *getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  OK(mkdtemp(T_dir) != NULL, "mkdtemp");
  T_now = 1767225600LL + 40LL * 86400LL;
  T_ms = 10u * 1000u * 1000u;
  farolsinal_relogio(fakeS, fakeMs);
  farolanel_relogio(fakeMs);
  farolsinal_pasta(T_dir);
  farolsinal_iniciar();

  // ---- ressalva da r2: estado com "p" truncado rejeita o arquivo inteiro --
  zeraTudo();
  entrada(&e, "tt_missing", DIARIO_FILME, 0, 0, 0, 0);
  dAdd(&e);                    // sem catalogo -> vira pendencia + vai ao disco
  farolsinal_sair();           // grava estado com 1 pendencia valida
  snprintf(cam, sizeof cam, "%s/farol-sinal-p1.txt", T_dir);
  fp = fopen(cam, "a");        // e uma linha "p" cortada no meio depois dela
  OK(fp != NULL, "estado existe");
  fputs("p 42 1 2 3 4 256 1 0 0 0 0\n", fp);   // faltam campos do registro
  fclose(fp);
  farolsinal_teste_zera();
  OK(farolsinal_pendencias() == 0,
     "estado com p truncado: rejeitado inteiro (0 pendencias)");

  // ---------------- 700 entradas no MESMO quando --------------------------
  zeraTudo();
  for (i = 0; i < 700; i++) {
    snprintf(id, sizeof id, "tt_w%03d", i);
    entrada(&e, id, DIARIO_FILME, 0, 0, 0, 0);
    e.quando = T_now - 3600;                 // o cenario da r2: empate total
    dPoe(&e);
  }
  dRecarrega();
  D_visita = 0; V_maior = 0;
  pthread_mutex_lock(&V_mtx); V_nIds = 0; pthread_mutex_unlock(&V_mtx);
  antes = V_fopenUi;

  farolsinal_passo(1);
  v1 = D_visita;
  OK(v1 <= VEZ_MAX + 1,
     "passada 1: <= teto+1 entregas no fio chamador (era 700)");
  OK(v1 == VEZ_MAX + 1, "passada 1: fatia cheia (512 + a recusada)");

  farolsinal_passo(1);
  v2 = (int)D_visita - v1;
  OK(v2 <= VEZ_MAX + 1, "passada 2: <= teto+1 entregas no fio chamador");
  OK(v2 == 700 - VEZ_MAX, "passada 2: as 188 que faltavam");
  OK(D_visita == 701, "varredura TERMINOU nos passos seguintes");
  farolsinal_passo(1);
  OK(D_visita == 701, "sem terceira volta");

  drenaTrab();                 // fio de tras termina conferidas/gravacoes
  OK(!farolsinal_teste_trabalho(), "nada pendente depois da drenagem");

  // sem perder nem repetir — mas so dentro do teto do aquecimento (issue
  // 43): as entregas sao as 700 de sempre (o cursor anda por todas), so que
  // vezNova e o cat_indice ficam nos 400 titulos mais novos. No empate
  // total de `quando` a ordem de entrega e a de insercao: tt_w000..tt_w399.
  {
    int repete = 0, falta = 0;
    pthread_mutex_lock(&V_mtx);
    OK(V_nIds == 400, "400 processamentos: o teto do aquecimento");
    for (i = 0; i < 700; i++) {
      int n = 0, j;
      snprintf(id, sizeof id, "tt_w%03d", i);
      for (j = 0; j < V_nIds; j++)
        if (!strcmp(V_ids[j], id)) n++;
      if (n > 1) repete++;
      if (!n && i < 400) falta++;
    }
    pthread_mutex_unlock(&V_mtx);
    OK(!repete && !falta, "sem perder nem repetir entrada no empate");
  }

  // a gravacao existiu — e NAO rolou no fio chamador
  OK(stat(cam, &st) == 0 && st.st_size > 0, "estado gravado pelo fio de tras");
  OK(V_fopenUi == antes, "nenhuma escrita de arquivo no fio chamador");
  OK(V_maior <= VEZ_MAX + 1, "nenhuma chamada de percorrer passou do teto");

  // limpeza da pasta temporaria
  {
    const char *nomes[] = { "farol-gosto-p1.txt", "farol-gosto-p2.txt",
                            "farol-gosto-p3.txt", "farol-sinal-p1.txt",
                            "farol-sinal-p2.txt", "farol-sinal-p3.txt",
                            "farol-fichas.bin", NULL };
    for (i = 0; nomes[i]; i++) {
      snprintf(cam, sizeof cam, "%s/%s", T_dir, nomes[i]); unlink(cam);
      snprintf(cam, sizeof cam, "%s/%s.tmp", T_dir, nomes[i]); unlink(cam);
    }
    rmdir(T_dir);
  }
  printf("farol_varre: tudo ok\n");
  return 0;
}
