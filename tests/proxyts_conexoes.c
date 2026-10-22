// Segundo GET do uMS nao pode encerrar o fluxo que ja esta tocando (#158).
// Socketpairs locais e fonte falsa: nenhuma URL ou credencial de uma conta.
#include "../src/proxyts.c"
#include <assert.h>

#define TAM_SEG (188 * 5)
static pthread_mutex_t fixtureTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fixtureCond = PTHREAD_COND_INITIALIZER;
static int liberar, baixando, maxBaixando, chamadas, listaFim = 1, fonteTs;
static size_t tamSegmento = TAM_SEG;
static unsigned sessaoPedido;

char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *n, int *st, int *erro, char *final, unsigned nf) {
  const char *lista = "#EXTM3U\n#EXT-X-TARGETDURATION:1\n#EXT-X-MEDIA-SEQUENCE:120\n"
    "#EXTINF:1,\ns0.ts\n#EXTINF:1,\ns1.ts\n#EXTINF:1,\ns2.ts\n"
    "#EXTINF:1,\ns3.ts\n#EXTINF:1,\ns4.ts\n#EXTINF:1,\ns5.ts\n#EXT-X-ENDLIST\n";
  (void)segundos; (void)ini; (void)fim;
  char *corpo = strdup(lista);
  assert(corpo);
  if (fonteTs) {
    unsigned char *ts = calloc(1, TAM_SEG); assert(ts);
    for (int i = 0; i < 5; i++) ts[i * 188] = 0x47;
    free(corpo); corpo = (char *)ts;
    *n = TAM_SEG; *st = 200; *erro = 0; final[0] = 0;
    return corpo;
  }
  if (!listaFim) *strstr(corpo, "#EXT-X-ENDLIST") = 0;
  *n = (long)strlen(corpo); *st = 200; *erro = 0;
  snprintf(final, nf, "%s", url);
  return corpo;
}
// So o caminho com cabecalhos do addon (#283) chama; este teste nao usa.
int rede_url_final_cab(const char *url, int segundos, const char *const *cab,
                       char *dst, unsigned tam, int *st) {
  (void)url; (void)segundos; (void)cab; (void)st;
  if (dst && tam) dst[0] = 0;
  return 0;
}
char *rede_baixar_bin_medido_controle(const char *url, int segundos,
    const char *const *cab, const RedeControle *ctl, long *n, RedeMedida *md) {
  unsigned char *b;
  (void)url; (void)segundos; (void)cab;
  pthread_mutex_lock(&fixtureTrava);
  chamadas++; baixando++;
  if (baixando > maxBaixando) maxBaixando = baixando;
  while (chamadas > 1 && !liberar && !__atomic_load_n(ctl->cancelado, __ATOMIC_ACQUIRE)) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 20000000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_nsec -= 1000000000; ts.tv_sec++; }
    pthread_cond_timedwait(&fixtureCond, &fixtureTrava, &ts);
  }
  baixando--;
  pthread_mutex_unlock(&fixtureTrava);
  if (__atomic_load_n(ctl->cancelado, __ATOMIC_ACQUIRE)) { *n = 0; return NULL; }
  b = calloc(1, tamSegmento);
  assert(b);
  for (size_t i = 0; i < tamSegmento / 188; i++) {
    b[i * 188] = 0x47;
    b[i * 188 + 1] = i ? 1 : 0x40;
    b[i * 188 + 3] = 0x10;
  }
  *n = (long)tamSegmento; memset(md, 0, sizeof *md); md->status = 200;
  return (char *)b;
}

static void iniciarCliente(int par[2], pthread_t *fio) {
  PxConexao *c = calloc(1, sizeof *c);
  char req[160];
  struct timeval tv = {3, 0};
  assert(c && socketpair(AF_UNIX, SOCK_STREAM, 0, par) == 0);
  setsockopt(par[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  c->s = par[1];
  snprintf(req, sizeof req, "GET /live.ts?s=%u HTTP/1.1\r\nHost: localhost\r\nRange: bytes=0-\r\n\r\n",
           sessaoPedido ? sessaoPedido : atomic_load(&sessaoAtual));
  assert(send(par[0], req, strlen(req), 0) == (ssize_t)strlen(req));
  assert(pthread_create(fio, NULL, pxAtender, c) == 0);
}

int main(void) {
  int principal[2], probe[2];
  pthread_t f1, f2;
  unsigned char recebido[16384], cab[2048];
  size_t total = 0, nCab = 0;
  ssize_t n;
  signal(SIGPIPE, SIG_IGN);
  atomic_store(&sessaoAtual, 1);
  snprintf(fonteAtual, sizeof fonteAtual, "http://fixture.invalid/canal.m3u8");
  iniciarCliente(principal, &f1);
  do {
    n = recv(principal[0], recebido + total, sizeof recebido - total - 1, 0);
    const char *fimCab;
    assert(n > 0); total += (size_t)n; recebido[total] = 0;
    fimCab = strstr((char *)recebido, "\r\n\r\n");
    if (fimCab) nCab = (size_t)(fimCab - (char *)recebido) + 4;
  } while (!nCab || total - nCab < TAM_SEG);

  // A conexao de sondagem fecha depois do cabecalho; o principal continua.
  iniciarCliente(probe, &f2);
  n = recv(probe[0], cab, sizeof cab, 0); assert(n > 0);
  close(probe[0]);
  pthread_mutex_lock(&fixtureTrava);
  liberar = 1; pthread_cond_broadcast(&fixtureCond);
  pthread_mutex_unlock(&fixtureTrava);
  while ((n = recv(principal[0], recebido + total, sizeof recebido - total, 0)) > 0)
    total += (size_t)n;
  assert(n == 0);
  pthread_join(f1, NULL); pthread_join(f2, NULL);
  close(principal[0]);
  proxyts_parar();
  if (total - nCab != 3 * TAM_SEG || maxBaixando != 1) {
    fprintf(stderr, "FAIL: principal recebeu %zu/%d bytes; downloads simultaneos=%d\n",
            total - nCab, 3 * TAM_SEG, maxBaixando);
    return 1;
  }

  // O buffer nunca cresce alem do teto; uma reabertura pousa num segmento
  // ainda disponivel, e nao em bytes sobrescritos ou no meio do pacote.
  {
    PxFluxo *f = calloc(1, sizeof *f);
    unsigned char *grande = malloc(PX_BUFFER + TAM_SEG), ultimo[188 * 2];
    assert(f && grande);
    f->buffer = malloc(PX_BUFFER); assert(f->buffer);
    assert(!pthread_mutex_init(&f->trava, NULL) && !pthread_cond_init(&f->chegou, NULL));
    f->produtor.fluxo = f;
    memset(grande, 0x47, PX_BUFFER + TAM_SEG); memset(ultimo, 0x11, sizeof ultimo);
    pxInicioSegmento(f); assert(pxEntregar(&f->produtor, grande, PX_BUFFER));
    pxInicioSegmento(f); assert(pxEntregar(&f->produtor, ultimo, sizeof ultimo));
    assert(f->fim - f->inicio == PX_BUFFER && f->inicio == sizeof ultimo);
    assert(pxCursorInicio(f) == PX_BUFFER && !memcmp(f->buffer, ultimo, sizeof ultimo));
    f->inicio = f->fim = 0; f->nSegmentos = 0;
    pxInicioSegmento(f);
    assert(pxEntregar(&f->produtor, grande, PX_BUFFER));
    assert(pxEntregar(&f->produtor, ultimo, sizeof ultimo));
    assert(pxCursorInicio(f) == f->inicio && f->inicio % 188 == 0);
    f->inicio = f->fim = 0; f->nSegmentos = 0;
    pxInicioSegmento(f);
    assert(pxEntregar(&f->produtor, grande, PX_BUFFER + TAM_SEG));
    assert(f->inicio == TAM_SEG && pxCursorInicio(f) == TAM_SEG && f->fim - f->inicio == PX_BUFFER);
    f->encerrado = 1;
    assert(!pxEntregar(&f->produtor, ultimo, sizeof ultimo));
    pxFluxoDestruir(f); free(grande);
  }

  // Trocar/parar sessao cancela o unico download pendente e acorda leitores.
  pthread_mutex_lock(&fixtureTrava);
  liberar = chamadas = baixando = maxBaixando = 0; listaFim = 0;
  pthread_mutex_unlock(&fixtureTrava);
  snprintf(fonteAtual, sizeof fonteAtual, "http://fixture.invalid/canal.m3u8");
  iniciarCliente(principal, &f1);
  total = nCab = 0;
  do {
    const char *fimCab;
    n = recv(principal[0], recebido + total, sizeof recebido - total - 1, 0);
    assert(n > 0); total += (size_t)n; recebido[total] = 0;
    fimCab = strstr((char *)recebido, "\r\n\r\n");
    if (fimCab) nCab = (size_t)(fimCab - (char *)recebido) + 4;
  } while (!nCab || total - nCab < TAM_SEG);
  proxyts_parar();
  while ((n = recv(principal[0], recebido, sizeof recebido, 0)) > 0) {}
  assert(n == 0);
  pthread_join(f1, NULL); close(principal[0]);
  for (int i = 0; i < 100; i++) {
    int pendente;
    pthread_mutex_lock(&trava); pendente = cancelarEmCurso != NULL; pthread_mutex_unlock(&trava);
    if (!pendente) break;
    usleep(10000);
  }
  pthread_mutex_lock(&trava);
  assert(!fluxoAtual && !cancelarEmCurso);
  pthread_mutex_unlock(&trava);

  // Segmentos maiores que o ring chegam INTEIROS ao leitor ativo. A sonda
  // que para de ler nao bloqueia esse progresso nem dispara outra ingestao.
  pthread_mutex_lock(&fixtureTrava);
  liberar = listaFim = 1; chamadas = baixando = maxBaixando = 0;
  tamSegmento = PX_BUFFER + 188 * 2000;
  pthread_mutex_unlock(&fixtureTrava);
  snprintf(fonteAtual, sizeof fonteAtual, "http://fixture.invalid/canal.m3u8");
  iniciarCliente(principal, &f1);
  total = nCab = 0;
  do {
    const char *fimCab;
    n = recv(principal[0], recebido + total, sizeof recebido - total - 1, 0);
    assert(n > 0); total += (size_t)n; recebido[total] = 0;
    fimCab = strstr((char *)recebido, "\r\n\r\n");
    if (fimCab) nCab = (size_t)(fimCab - (char *)recebido) + 4;
  } while (!nCab);
  iniciarCliente(probe, &f2);
  n = recv(probe[0], cab, sizeof cab, 0); assert(n > 0);
  while ((n = recv(principal[0], recebido, sizeof recebido, 0)) > 0) total += (size_t)n;
  assert(n == 0 && total - nCab == 3 * tamSegmento && maxBaixando == 1);
  close(probe[0]); close(principal[0]);
  pthread_join(f1, NULL); pthread_join(f2, NULL);
  proxyts_parar();

  // TS continuo segue direto a fonte; URL da sessao antiga continua404.
  fonteTs = 1;
  snprintf(fonteAtual, sizeof fonteAtual, "http://fixture.invalid/continuo.ts");
  iniciarCliente(principal, &f1);
  n = recv(principal[0], cab, sizeof cab - 1, 0); assert(n > 0); cab[n] = 0;
  assert(strstr((char *)cab, "302 Found") && strstr((char *)cab, "Location: http://fixture.invalid/continuo.ts"));
  close(principal[0]); pthread_join(f1, NULL);
  sessaoPedido = atomic_load(&sessaoAtual);
  proxyts_parar();
  iniciarCliente(principal, &f1);
  n = recv(principal[0], cab, sizeof cab - 1, 0); assert(n > 0); cab[n] = 0;
  assert(strstr((char *)cab, "404 Not Found"));
  close(principal[0]); pthread_join(f1, NULL);
  puts("proxyts conexoes: PASS (probe nao fecha principal; uma ingestao HLS)");
  return 0;
}
