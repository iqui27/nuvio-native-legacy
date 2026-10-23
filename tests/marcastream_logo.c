// O WORKER DO WORDMARK (marcastream_logo.c) NAO DESISTE PARA SEMPRE (#205).
//
// Cenario da revisao: abrir a colecao com o TMDB desligado (ou sem rede) grava
// o estado 3 e chamadas posteriores nunca repetem a busca — nem ativando o
// TMDB, nem recuperando a rede. Este teste cobra:
//   1. sem chave, a busca falha e fica no estado 3;
//   2. ligar a chave do TMDB re-tenta na hora e o wordmark chega;
//   3. a chave nao mudou, mas o intervalo passou: re-tenta tambem;
//   4. falha recente sem mudanca de chave NAO martela o TMDB a cada quadro.
//
// Inclui src/marcastream_logo.c (o estado e estatico) e troca a rede e a chave
// por stubs; o leitor de JSON e o de sempre (src/js.c).
#include "../src/marcastream_logo.c"
#undef desc_chave_tmdb
#undef rede_baixar
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char chaveTmdb[64] = "";
const char *desc_chave_tmdb(void) { return chaveTmdb; }

static int redeFalha = 1;   // 1: sem rede; 0: o TMDB responde o corpo abaixo
static const char CORPO[] =
  "{\"id\":213,\"logos\":[{\"file_path\":\"/n.svg\"},{\"file_path\":\"/logo.png\"}]}";
char *rede_baixar(const char *url, int segundos) {
  (void)url; (void)segundos;
  return redeFalha ? NULL : strdup(CORPO);
}

static int slotDaRede(long rede) {
  for (int k = 0; k < MS_MAX; k++)
    if (slot[k].rede == rede) return k;
  return -1;
}
static int esperar(long rede, int estado) {
  int k = slotDaRede(rede);
  assert(k >= 0);
  for (int i = 0; i < 500 && atomic_load(&slot[k].estado) != estado; i++)
    usleep(10000);
  return atomic_load(&slot[k].estado) == estado;
}

int main(void) {
  MarcaStream netflix = { "netflix", 213, 0, 0, 0 };
  MarcaStream prime = { "prime video", 1024, 0, 0, 0 };
  const char *u;

  // 1. SEM CHAVE: falha e grava o estado 3.
  assert(!marca_stream_logo(&netflix)[0]);
  assert(esperar(213, 3));
  assert(!marca_stream_logo(&netflix)[0]);
  assert(atomic_load(&slot[slotDaRede(213)].estado) == 3);   // nao re-tenta sozinho

  // 2. O DONO LIGOU O TMDB: a chave apareceu, a busca volta AGORA.
  snprintf(chaveTmdb, sizeof chaveTmdb, "chave-de-teste");
  redeFalha = 0;
  marca_stream_logo(&netflix);
  assert(esperar(213, 2));
  u = marca_stream_logo(&netflix);
  assert(u[0] && !strcmp(u, marca_stream_logo(&netflix)));
  assert(strstr(marca_stream_logo(&netflix), "/logo.png"));   // o SVG e ignorado

  // 3. INTERVALO: chave igual, rede que voltou — re-tenta quando o prazo passa.
  redeFalha = 1;
  assert(!marca_stream_logo(&prime)[0]);
  assert(esperar(1024, 3));
  assert(atomic_load(&slot[slotDaRede(1024)].estado) == 3);   // falha recente
  slot[slotDaRede(1024)].falhouEm -= MS_RETRY_S + 1;          // o prazo passou
  redeFalha = 0;
  marca_stream_logo(&prime);
  assert(esperar(1024, 2));
  u = marca_stream_logo(&prime);
  assert(strstr(u, "/logo.png"));

  // 4. FALHA RECENTE, NADA MUDOU: nao martela o servico a cada chamada.
  int k = slotDaRede(213);
  slot[k].falhouEm = time(NULL);
  atomic_store(&slot[k].estado, 3);
  for (int i = 0; i < 3; i++) assert(!marca_stream_logo(&netflix)[0]);
  assert(atomic_load(&slot[k].estado) == 3);

  puts("ok  marcastream_logo");
  return 0;
}
