// #330: Range de geracao velha nao vai a rede. Com o pool de 3 fios e FIFO,
// pre-buscas e fontes anexadas da fonte anterior atrasavam a fonte nova.
#include "../src/mkvass.c"
#include <assert.h>

static int baixados;
static pthread_mutex_t tb = PTHREAD_MUTEX_INITIALIZER;
char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim, long *tam,
                            int *st, int *er, char *fin, unsigned tf) {
  char *p;
  (void)url; (void)s; (void)fin; (void)tf;
  pthread_mutex_lock(&tb); baixados++; pthread_mutex_unlock(&tb);
  usleep(20000);
  *tam = (long)(fim - ini + 1); if (st) *st = 206; if (er) *er = 0;
  p = malloc((size_t)*tam); memset(p, 'x', (size_t)*tam);
  return p;
}
int rede_resto_recusado(void) { return 0; }
char *rede_baixar(const char *url, int s) { (void)url; (void)s; return NULL; }
char *rede_baixar_bin(const char *url, int s, long *n) { (void)url; (void)s; (void)n; return NULL; }
const char *rede_url_publica(const char *url, char *dst, unsigned tam) { (void)url; snprintf(dst, tam, "local"); return dst; }
long rede_corte_host(const char *url) { (void)url; return 0; }

static int falhas;
static void conferir(int ok, const char *caso) {
  printf("%s: %s\n", ok ? "ok" : "FALHA", caso);
  if (!ok) falhas++;
}
static void esperarJob(Job *j) {
  pthread_mutex_lock(&PT);
  while (j->estado != 2) pthread_cond_wait(&PFeito, &PT);
  pthread_mutex_unlock(&PT);
}

int main(void) {
  Fio f; Job *v[8], *n[2]; int i;
  memset(&f, 0, sizeof f);
  snprintf(f.url, sizeof f.url, "http://x/a.mkv");
  pthread_mutex_lock(&S.trava); S.geracao = 5; S.parar = 0; pthread_mutex_unlock(&S.trava);

  // Geracao 5 pede 8 Ranges; antes de o pool andar, a geracao avanca (outra fonte).
  f.g = 5;
  pthread_mutex_lock(&PT);   // segura o pool: nada sai da fila ainda
  for (i = 0; i < 8; i++) {
    Job *j = calloc(1, sizeof *j);
    snprintf(j->url, sizeof j->url, "%s", f.url); j->ini = i * 1000; j->n = 1000; j->g = 5;
    if (filaFim) filaFim->prox = j; else filaIni = j;
    filaFim = j; v[i] = j;
  }
  pthread_mutex_unlock(&PT);
  pthread_mutex_lock(&S.trava); S.geracao = 6; pthread_mutex_unlock(&S.trava);
  f.g = 6;
  n[0] = submeter(&f, 0, 1000);   // cria os fios do pool e acorda
  assert(n[0]);
  for (i = 0; i < 8; i++) esperarJob(v[i]);
  esperarJob(n[0]);
  conferir(baixados == 1, "8 Ranges de geracao velha nao foram a rede; o da nova sim");
  conferir(n[0]->r != NULL && n[0]->tam == 1000, "Range da geracao atual chega inteiro");

  // mkvass_parar(): o que sobrou na fila tambem nao baixa.
  baixados = 0;
  pthread_mutex_lock(&S.trava); S.vivos = 1; pthread_mutex_unlock(&S.trava);
  mkvass_parar();
  n[1] = submeter(&f, 0, 1000);
  esperarJob(n[1]);
  conferir(baixados == 0 && n[1]->r == NULL, "apos mkvass_parar() nada novo vai a rede");
  return falhas ? 1 : 0;
}
