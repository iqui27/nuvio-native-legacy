// rede_zerar_cache (issue #34): o cache de conexoes/DNS do cliente e zerado sem
// tocar em transferencia de ninguem. Liga o src/rede.c REAL com uma libcurl
// simulada: cada fio descarta o PROPRIO par easy+multi na proxima pegarHandle e
// o handle novo pede a poda do cache de DNS; os pares estacionados morrem na hora.
#include <assert.h>
#include <stdarg.h>
#include "../src/rede.c"

static int vivos, criados, limpos, resets, multisLimpos, dnsPoda = -1;
static void *init(void) { criados++; vivos++; return calloc(1, 16); }
static void cleanup(void *c) { limpos++; vivos--; free(c); }
static void reset(void *c) { (void)c; resets++; dnsPoda = -1; }
static int multiCleanup(void *m) { multisLimpos++; free(m); return 0; }
static int option(void *c, int opt, ...) {
  va_list ap;
  (void)c;
  va_start(ap, opt);
  if (opt == 92) dnsPoda = (int)va_arg(ap, long);
  va_end(ap);
  return 0;
}

typedef struct { void *tomado; unsigned ger; } Fio;
static void *fioNovo(void *arg) {
  Fio *f = arg;
  void *c = pegarHandle("https://exemplo.invalid/a");
  assert(c);
  f->tomado = c;
  f->ger = rede_cache_geracao();
  // Fio nascido depois do zerar: o reuso do segundo pedido continua valendo.
  assert(pegarHandle("https://exemplo.invalid/b") == c);
  // Devolve o handle ao pool do fio e termina: o destrutor limpa o par.
  return NULL;
}

int main(void) {
  void *a, *b, *c;
  curl_init = init; curl_cleanup = cleanup; curl_reset = reset;
  curl_setopt_f = option; curl_multi_cleanup = multiCleanup;

  // Geracao 0: pool novo, nada zerado ainda; o handle e criado e reusado.
  assert(rede_cache_geracao() == 0);
  a = pegarHandle("https://exemplo.invalid/a");
  assert(a && criados == 1 && vivos == 1);
  b = pegarHandle("https://exemplo.invalid/b");
  assert(b == a && criados == 1 && limpos == 0 && resets == 1);   // reuso: conexoes preservadas
  assert(dnsPoda == -1);                                          // sem poda de DNS sem pedido

  // Zerar NAO mexe no handle do fio ate o proximo pedido (transferencia segura).
  rede_zerar_cache();
  assert(rede_cache_geracao() == 1);
  assert(limpos == 0 && vivos == 1);

  // Proximo pedido deste fio: o par antigo morre, o novo nasce, e o cache de DNS
  // (inclusive o compartilhado) e podado nesse pedido.
  c = pegarHandle("https://exemplo.invalid/c");
  assert(c && limpos == 1 && criados == 2 && vivos == 1);
  assert(dnsPoda == 0);
  // Depois disso o fio volta ao reuso normal: nenhuma troca a cada pedido.
  dnsPoda = -1;
  b = pegarHandle("https://exemplo.invalid/d");
  assert(b == c && limpos == 1 && criados == 2 && dnsPoda == -1);

  // Outro fio so criado agora ja nasce na geracao nova (nada a descartar).
  { Fio f = {0, 0}; pthread_t t;
    int cr = criados;
    assert(pthread_create(&t, NULL, fioNovo, &f) == 0);
    pthread_join(t, NULL);
    assert(f.ger == 1 && criados == cr + 1);
  }
  vivos = 1;   // o destrutor do fio de teste limpou o dele

  // Pares estacionados (aquecimento): nao pertencem a ninguem, morrem na hora.
  { char hosts[1][96] = {"exemplo.invalid"};
    void *e1 = init(), *m1 = calloc(1, 8);
    int antes = limpos, antesM = multisLimpos;
    estacionar(e1, m1, hosts, 1);
    rede_zerar_cache();
    assert(limpos == antes + 1 && multisLimpos == antesM + 1);
    assert(estacTomar("https://exemplo.invalid/x", NULL) == NULL);
  }

  // FAROL34B: a corrida que faltava — par nascido na geracao velha estacionado
  // DEPOIS de rede_zerar_cache nao pode ser adotado: a carimbo do pool do fio
  // (a geracao em que o handle nasceu) fica menor que a atual e o tomador o
  // descarta na hora. Antes o carimbo era so lido por quem o pool pegava.
  { char hosts[1][96] = {"https://exemplo.invalid"};   // hostDaUrl guarda esquema+host
    HandlePool *pp = (HandlePool *)pthread_getspecific(handleChave);
    void *hc = pegarHandle("https://exemplo.invalid/e");
    void *hm;
    int antes, antesM;
    assert(pp && hc);
    hm = pp->multi;
    pp->c = NULL; pp->multi = NULL;      // como rede_aquecer_job faz
    rede_zerar_cache();
    estacionar(hc, hm, hosts, 1);        // carimbo = pp->ger (velha)
    antes = limpos; antesM = multisLimpos;
    assert(estacTomar("https://exemplo.invalid/x", NULL) == NULL);
    assert(limpos == antes + 1);
    assert(multisLimpos == antesM + (hm ? 1 : 0));
  }
  puts("PASS: rede_zerar_cache");
  return 0;
}
