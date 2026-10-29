// CORRIDA DA ADOCAO: faixas.c escolhe por ordinal antes de a pre-busca
// publicar S.prebuscaOrdinal (cabecalho lateral lento). A adocao falha e o
// fio novo substitui o leitor lateral da mesma url — deve conservar a marca
// lateral (janela de 12 s) em vez de voltar a colher 90 s a frente do video.
#include "../src/mkvass.c"
#include <assert.h>
const char *i18n(const char *text) { return text; }

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

// Pre-busca viva: so o estado da sessao, sem fio de verdade.
static void fingirPrebusca(const char *url, int ordinal) {
  pthread_mutex_lock(&S.trava);
  snprintf(S.url, sizeof S.url, "%s", url);
  S.prebusca = 1; S.prebuscaFase = 1; S.prebuscaOrdinal = ordinal;
  S.lateral = 1; S.vivos = 1; S.parar = 0;
  S.estado = MKVASS_COLHENDO; S.adotar = 0;
  pthread_cond_broadcast(&S.sinal);
  pthread_mutex_unlock(&S.trava);
}

// Mata o fio de mentira e espera o fio novo morrer na rede stubada (no-go).
static void drenar(void) {
  int i;
  pthread_mutex_lock(&S.trava);
  if (S.vivos > 1) { S.vivos--; pthread_cond_broadcast(&S.sinal); }
  pthread_mutex_unlock(&S.trava);
  for (i = 0; i < 400; i++) {
    pthread_mutex_lock(&S.trava);
    int v = S.vivos; pthread_mutex_unlock(&S.trava);
    if (!v) return;
    usleep(50000);
  }
  conferir(0, "o fio novo morreu na rede stubada");
  exit(1);
}

int main(void) {
  int r, ger, viv, lat;
  // 1. A corrida: ordinal pedido antes de a pre-busca o publicar.
  fingirPrebusca("http://x/a.mkv", -1);
  r = iniciarFio("http://x/a.mkv", -1, 0, 0, NULL, 0.0); // o que mkvass_iniciar_ordinal pede
  pthread_mutex_lock(&S.trava);
  ger = S.geracao; viv = S.vivos; lat = S.lateral;
  pthread_mutex_unlock(&S.trava);
  conferir(r == 1 && viv == 2 && ger >= 1, "adocao falhou: o fio novo nasceu");
  conferir(lat == 1, "substituicao da pre-busca em curso conserva a marca lateral (12 s)");
  drenar();

  // 2. Sem corrida: ordinal publicado -> adocao pura, sem fio novo.
  fingirPrebusca("http://x/a.mkv", 0);
  pthread_mutex_lock(&S.trava); ger = S.geracao; pthread_mutex_unlock(&S.trava);
  r = iniciarFio("http://x/a.mkv", -1, 0, 0, NULL, 0.0); // o que mkvass_iniciar_ordinal pede
  pthread_mutex_lock(&S.trava);
  viv = S.vivos; lat = S.lateral;
  int adotou = S.adotar, faixa = S.faixa;
  pthread_mutex_unlock(&S.trava);
  conferir(r == 1 && viv == 1 && S.geracao == ger, "ordinal publicado: adota, nao cria fio");
  conferir(adotou == 1 && faixa == -1, "adocao marca a faixa pedida por ordinal");
  conferir(lat == 1, "adocao nao mexe no pacing lateral da sessao");

  // 3. Pre-busca de OUTRA url: sessao nova comeca sem marca lateral.
  fingirPrebusca("http://x/b.mkv", -1);
  pthread_mutex_lock(&S.trava); ger = S.geracao; pthread_mutex_unlock(&S.trava);
  r = iniciarFio("http://x/a.mkv", -1, 0, 0, NULL, 0.0); // o que mkvass_iniciar_ordinal pede
  pthread_mutex_lock(&S.trava);
  lat = S.lateral; int pre = S.prebusca, ord = S.prebuscaOrdinal;
  pthread_mutex_unlock(&S.trava);
  conferir(r == 1 && S.geracao > ger, "outra url: adocao falha, fio novo");
  conferir(lat == 0 && pre == 0 && ord == -1, "sessao nova fora da pre-busca volta ao pacing inteiro");
  drenar();
  return falhas ? 1 : 0;
}