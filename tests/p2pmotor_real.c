// Motor P2P embutido DE VERDADE (nuvio-engine + libtorrent compilados no Mac),
// pelo mesmo caminho do app: p2p_resolver -> p2pmotor_resolver.
//
//   ciclo (padrao): hash SEM peers. Sobe o motor real, cancela no meio da
//     espera dos metadados, para (destroy real + limpeza), sobe de novo, para
//     em fio solto no meio do pedido, e a saida do app. Prova ciclo de vida,
//     cancelamento e ordem da limpeza no motor real. Nao baixa video.
//   rede <hash> [s]: torrent real (so legal: Big Buck Bunny por padrao). Ate a
//     URL local + 64 KB por ela. Depende de peers: opt-in (NV_P2P_REDE=1).
#include "p2p.h"
#include "p2pmotor.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "nvtmp.h"

int ajustes_p2p_ligado(void) { return 1; }
const char *ajustes_p2p_url(void) { return ""; }
void debrid_episodio(int *t, int *e) { *t = 0; *e = 0; }

static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
static int existe(const char *p) { struct stat st; return stat(p, &st) == 0; }

typedef struct { const char *h; int r; char url[600]; double dur; } Job;
static void *jobFio(void *u) {
  Job *j = u;
  double t0 = agora();
  j->r = p2p_resolver(j->h, -1, "", j->url, sizeof j->url);
  j->dur = agora() - t0;
  return NULL;
}

int main(int argc, char **argv) {
  const char *modo = argc > 1 ? argv[1] : "ciclo";
  char raiz[700];
  setvbuf(stdout, NULL, _IOLBF, 0);
  dados_iniciar(getenv("NUVIO_DADOS") ? getenv("NUVIO_DADOS") : nvt_dir());
  snprintf(raiz, sizeof raiz, "%s/p2p", dados_dir());
  if (!p2p_ativo() || !p2p_usa_motor()) { puts("p2pmotor_real: motor ausente neste build"); return 1; }
  printf("motor %s, dados em %s\n", p2pmotor_versao(), dados_dir());
  if (!strcmp(modo, "ciclo")) {
    // Hash valido sem ninguem no swarm: os metadados nunca chegam.
    const char *nada = "0123456789abcdef0123456789abcdef01234567";
    Job j = { nada, -1, "", 0 };
    pthread_t t;
    double t0;
    pthread_create(&t, NULL, jobFio, &j);
    sleep(3);
    assert(p2pmotor_ativo() && existe(raiz));
    t0 = agora();
    p2pmotor_cancelar();
    pthread_join(t, NULL);
    printf("cancelado: erro %d, %.0f ms depois do cancelar\n", j.r, (agora() - t0) * 1000);
    assert(j.r == P2P_ERR_CANCELADO && !j.url[0] && agora() - t0 < 1.0);
    t0 = agora();
    p2pmotor_parar();
    printf("destroy real + limpeza: %.1f s\n", agora() - t0);
    assert(!p2pmotor_ativo() && !existe(raiz));
    // De novo, e agora a parada em fio solto no meio do pedido.
    memset(&j, 0, sizeof j); j.h = nada;
    pthread_create(&t, NULL, jobFio, &j);
    sleep(2);
    t0 = agora();
    p2pmotor_parar_fundo();
    printf("parar_fundo voltou em %.2f ms\n", (agora() - t0) * 1000);
    assert(agora() - t0 < 0.05);
    pthread_join(t, NULL);
    assert(j.r == P2P_ERR_CANCELADO);
    p2pmotor_parar();
    assert(!p2pmotor_ativo() && !existe(raiz));
    // Saida do app com o motor de pe.
    memset(&j, 0, sizeof j); j.h = nada;
    pthread_create(&t, NULL, jobFio, &j);
    sleep(2);
    t0 = agora();
    p2pmotor_saida();
    printf("saida: %.2f s\n", agora() - t0);
    pthread_join(t, NULL);
    p2pmotor_parar();
    assert(!existe(raiz));
    puts("p2pmotor_real ciclo: ok (motor real; sem download)");
    return 0;
  }
  {
    const char *h = argc > 2 ? argv[2] : "dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c";
    int seg = argc > 3 ? atoi(argv[3]) : 0;
    Job j = { h, -1, "", 0 };
    jobFio(&j);
    printf("resolver: erro %d em %.1f s\nURL %s\n", j.r, j.dur, j.url);
    if (j.r != P2P_OK) return 2;
    while (seg-- > 0) {
      int m;
      sleep(1);
      if ((m = p2pmotor_motivo_parada())) { printf("vigia: parou com erro %d\n", m); break; }
    }
    p2pmotor_parar();
    assert(!existe(raiz));
    puts("p2pmotor_real rede: ok");
  }
  return 0;
}
