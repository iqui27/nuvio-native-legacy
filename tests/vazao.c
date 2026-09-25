// Ver tests/vazao.sh. Sem argumento: so a conta (vazao.c). Com a porta do
// servidor local: tambem rede_medir_vazao contra ele.
#include "vazao.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int falhas;
static void conferir(int ok, const char *o_que) {
  printf("%s  %s\n", ok ? "ok  " : "FALHOU", o_que);
  if (!ok) falhas++;
}

static void conta(void) {
  VazaoResumo r;
  char s[32];
  { static const int a[5] = { 50, 10, 40, 20, 30 };
    conferir(vazao_resumir(a, 5, &r) && r.n == 5 && r.medianaKbps == 30 && r.p20Kbps == 10 &&
             r.otimoKbps == 8 && r.maximoKbps == 27,
             "impar: mediana 30, p20 10, otimo = p20 x 0,75, maximo = mediana x 0,9"); }
  { static const int a[4] = { 4000, 1000, 3000, 2000 };
    conferir(vazao_resumir(a, 4, &r) && r.medianaKbps == 2500 && r.p20Kbps == 1000,
             "par: mediana e a media dos dois do meio"); }
  { int a[24], i;
    for (i = 0; i < 24; i++) a[i] = (i + 1) * 1000;   // 1000..24000
    vazao_resumir(a, 24, &r);
    conferir(r.p20Kbps == 5000 && r.medianaKbps == 12500,
             "24 amostras: p20 e a 5a (ceil(0,2 n)), nao o minimo"); }
  { static const int a[6] = { -1, 0, 30000, 30000, 30000, -5 };
    conferir(vazao_resumir(a, 6, &r) && r.n == 4 && r.p20Kbps == 0 && r.otimoKbps == 0 &&
             r.medianaKbps == 30000,
             "negativa fica de fora; segundo PARADO (0) conta como trecho ruim"); }
  conferir(!vazao_resumir(NULL, 0, &r) && r.n == 0 && r.otimoKbps == 0, "sem amostra: 0 e nada inventado");

  conferir(vazao_gb(40000, VAZAO_FILME_S) > 35.99 && vazao_gb(40000, VAZAO_FILME_S) < 36.01,
           "40 Mbps x 2 h = 36 GB (Mbps x s / 8 / 1000)");
  conferir(vazao_gb(13334, VAZAO_EPISODIO_S) > 4.49 && vazao_gb(13334, VAZAO_EPISODIO_S) < 4.51,
           "13,3 Mbps x 45 min = 4,5 GB");
  vazao_fmt_gb(s, sizeof s, 36.0, ','); conferir(!strcmp(s, "36"), "36 GB sem casa decimal");
  vazao_fmt_gb(s, sizeof s, 4.5, ','); conferir(!strcmp(s, "4,5"), "4,5 GB com virgula em portugues");
  vazao_fmt_gb(s, sizeof s, 4.5, '.'); conferir(!strcmp(s, "4.5"), "4.5 GB com ponto em ingles");
  vazao_fmt_gb(s, sizeof s, 9.97, ','); conferir(!strcmp(s, "10"), "9,97 arredonda para 10, nao 10,0");
  vazao_fmt_mbps(s, sizeof s, 38400, ','); conferir(!strcmp(s, "38"), "38,4 Mbps vira 38");
  vazao_fmt_mbps(s, sizeof s, 820, ','); conferir(!strcmp(s, "0,8"), "820 kbps vira 0,8 Mbps");

  conferir(vazao_url_aviso("https://slate.elfhosted.com/x.mp4") &&
           vazao_url_aviso("https://static.debridio.com/scraperV2/500.mp4") &&
           vazao_url_aviso("https://a.b/c/downloading.mp4") &&
           vazao_url_aviso("https://aio.x/static/slate.mp4") &&
           !vazao_url_aviso("https://cdn.real-debrid.com/d/ABC/filme.mkv"),
           "links de aviso ficam de fora; o do CDN nao");
  vazao_host("https://cdn.x.com:8443/d/CHAVE-SECRETA/filme.mkv?t=1", s, sizeof s);
  conferir(!strcmp(s, "https://cdn.x.com:8443") && !strstr(s, "CHAVE"),
           "host = esquema + host + porta; a chave do caminho nao sai");
  conferir(!vazao_host("sem-esquema", s, sizeof s) && !s[0], "sem :// nao ha host");
  conferir(strstr(vazao_dica(90000), "Remux 4K") && strstr(vazao_dica(35000), "remux 1080p") &&
           strstr(vazao_dica(5000), "720p"), "dica por degrau de bitrate");
}

// ---------------------------------------------------------------- rede
static int porta;
static volatile int cancelar;
static unsigned long agoraMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}
static int medir(const char *caminho, int seg, long ini, int *kbps, RedeVazao *r,
                 char *final, unsigned nf, unsigned long *ms) {
  char u[160];
  unsigned long t0 = agoraMs();
  int n;
  snprintf(u, sizeof u, "http://127.0.0.1:%d%s", porta, caminho);
  n = rede_medir_vazao(u, NULL, seg, ini, 0, &cancelar, kbps, VAZAO_SEG_MAX, r, final, nf);
  *ms = agoraMs() - t0;
  return n;
}
static void *cancelarDepois(void *u) {
  (void)u;
  usleep(1200 * 1000);
  cancelar = 1;
  return NULL;
}

static void rede(void) {
  int kbps[VAZAO_SEG_MAX], n, i;
  RedeVazao r;
  VazaoResumo s;
  char fim[256];
  unsigned long ms;

  // /lento: 1 MB/s = 8000 kbps, com Range. Janela de 3 s.
  n = medir("/lento", 3, 5L * 1024 * 1024, kbps, &r, fim, sizeof fim, &ms);
  vazao_resumir(kbps, n, &s);
  printf("      /lento: %d amostras, mediana %d kbps, HTTP %d, %lld bytes, %lu ms\n",
         n, s.medianaKbps, r.status, r.bytes, ms);
  conferir(n == 3 && r.status == 206 && s.medianaKbps > 6000 && s.medianaKbps < 10000,
           "servidor a 1 MiB/s: 3 amostras de ~8400 kbps, Range 206");
  conferir(ms < 5000, "a janela conta do primeiro byte e fecha em ~3 s");

  // Redirecionamento: o final sai para quem chama (e o host da dedup).
  n = medir("/redir", 2, 0, kbps, &r, fim, sizeof fim, &ms);
  conferir(n == 2 && strstr(fim, "/lento") != NULL, "segue o 302 e devolve o endereco final");

  // Corpo parado: 64 KB e silencio. A janela fecha pelo vigia, com zeros.
  n = medir("/parado", 3, 0, kbps, &r, fim, sizeof fim, &ms);
  { int zeros = 0;
    for (i = 0; i < n; i++) if (kbps[i] == 0) zeros++;
    conferir(n == 3 && zeros >= 2 && ms < 6000,
             "corpo parado: a janela fecha no prazo e os segundos vazios valem 0"); }

  n = medir("/proibido", 3, 0, kbps, &r, fim, sizeof fim, &ms);
  conferir(n == 0 && r.status == 403, "403: nenhuma amostra, o status vai para quem chama");

  n = medir("/lento", 3, 1100L * 1024 * 1024, kbps, &r, fim, sizeof fim, &ms);
  conferir(n == 0 && r.status == 416, "Range alem do fim: 416, sem amostra (quem chama pede do 0)");

  n = medir("/curto", 3, 0, kbps, &r, fim, sizeof fim, &ms);
  conferir(n == 1 && r.bytes == 200000 && kbps[0] > 0,
           "arquivo menor que 1 s de corpo: uma amostra pela taxa media");

  { pthread_t t;
    cancelar = 0;
    pthread_create(&t, NULL, cancelarDepois, NULL);
    n = medir("/lento", 8, 0, kbps, &r, fim, sizeof fim, &ms);
    pthread_join(t, NULL);
    conferir(r.cancelado && n == 0 && ms < 3500, "cancelamento no meio corta em ~1 s, sem numero");
    cancelar = 0; }

  { char u[160];
    snprintf(u, sizeof u, "http://127.0.0.1:%d/nada", porta == 1 ? 2 : 1);
    n = rede_medir_vazao(u, NULL, 2, 0, 0, NULL, kbps, VAZAO_SEG_MAX, &r, NULL, 0);
    conferir(n == 0 && r.status == 0 && r.erro != 0, "sem servidor: status 0 e o codigo da libcurl"); }
}

int main(int argc, char **argv) {
  conta();
  if (argc > 1) {
    porta = atoi(argv[1]);
    rede();
  }
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  return 0;
}
