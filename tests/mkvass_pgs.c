// Colheita sintetica de PGS (S_HDMV/PGS) do MKV para o armazem imgsub.
// Transporte local de Range: nenhum socket, parser/extrator reais.
#include "mkvass.h"
#include "legenda.h"
#include "imgsub.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char esperado[4096] = "https://example.test/pgs.mkv";
static const char *arquivo;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int pedidos, semRange;
char *rede_baixar(const char *url, int s) { (void)url; (void)s; return NULL; }
char *rede_baixar_bin(const char *url, int s, long *n) { (void)url; (void)s; (void)n; return NULL; }
// O mkvass pede por rede_baixar_trecho_st (status, erro e url final); o
// transporte local responde como um servidor sem redirecionamento: a url
// final e a propria. semRange simula HTTP 200 com o arquivo desde o byte 0.
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  (void)url; snprintf(dst, tam, "local"); return dst;
}
long rede_corte_host(const char *url) { (void)url; return 0; }
int rede_resto_recusado(void) { return 0; }
char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim, long *tam,
                            int *status, int *erro, char *final, unsigned tamFinal) {
  FILE *f; char *p; long n;
  (void)s; *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 0;
  if (final && tamFinal) snprintf(final, tamFinal, "%s", url);
  pthread_mutex_lock(&trava);
  pedidos++;
  assert(!strcmp(url, esperado));
  pthread_mutex_unlock(&trava);
  f = fopen(arquivo, "rb"); assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f);
  if (semRange) { ini = 0; fim = n - 1; }
  if (ini >= n) { fclose(f); return NULL; }
  if (fim >= n) fim = n - 1;
  n = fim - ini + 1;
  p = malloc((size_t)n); assert(p);
  fseek(f, ini, SEEK_SET);
  assert(fread(p, 1, (size_t)n, f) == (size_t)n);
  fclose(f); *tam = n; if (status) *status = semRange ? 200 : 206; return p;
}
static void parar(void) {
  mkvass_parar();
  for (int i = 0; mkvass_ocupado() && i < 2000; i++) usleep(10000);
  assert(!mkvass_ocupado());
}
int main(int argc, char **argv) {
  assert(argc == 2); dados_iniciar(".");
  arquivo = argv[1];
  legenda_desligar(); imgsub_limpar();
  mkvass_aceitar_imagem(1);
  mkvass_iniciar_ordinal(esperado, 0);
  ImgsubQuadro *q = NULL;
  for (int i = 0; !q && i < 2000; i++) { q = imgsub_pegar(1.5); if (!q) usleep(10000); }
  assert(q && q->n == 1);
  assert(q->tela_w == 1920 && q->tela_h == 1080);
  assert(q->obj[0].x == 100 && q->obj[0].y == 900 && q->obj[0].w == 6 && q->obj[0].h == 2);
  assert(q->obj[0].rgba[0] == 255 && q->obj[0].rgba[3] == 255 && q->obj[0].rgba[7] == 128);
  assert(q->ini == 1.25 && q->fim == 3.0);
  imgsub_soltar(q);
  // Depois do fim do cue: nada. O bloco que so limpa devolve o quadro vazio
  // (a tela apaga mesmo se o cue anterior ainda estivesse na janela).
  assert(!imgsub_pegar(3.5));
  q = imgsub_pegar(5.5);
  assert(q && q->n == 0 && q->ini == 5.0);
  imgsub_soltar(q);
  assert(imgsub_dono_atual() == legenda_geracao());
  // Sem aceitar imagem a faixa nao e colhida (o app cai na rota anterior).
  parar(); imgsub_limpar(); legenda_desligar();
  mkvass_aceitar_imagem(0);
  mkvass_iniciar_ordinal(esperado, 0);
  usleep(500000);
  assert(!imgsub_pegar(1.5));
  parar();
  puts("mkvass_pgs: bloco PGS vira quadro no imgsub, limpeza e recusa ok");
}
