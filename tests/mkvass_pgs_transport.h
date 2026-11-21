// Local Range transport shared by the PGS acquisition tests. No network.
static char esperado[4096] = "https://example.test/pgs.mkv";
static const char *arquivo;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int pedidos, semRange, falhar;
static long long request_offsets[4096], request_lengths[4096];
static double request_times[4096];
static double now(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}
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
  assert(pedidos < 4096);
  request_offsets[pedidos] = ini; request_lengths[pedidos] = fim-ini+1;
  request_times[pedidos++] = now();
  assert(!strcmp(url, esperado));
  int fail = falhar;
  pthread_mutex_unlock(&trava);
  if (fail) { if (status) *status = 404; return NULL; }
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
