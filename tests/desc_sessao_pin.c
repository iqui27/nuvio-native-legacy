// #551: A VOLTA CONDENADA DEPOIS DE UM PIN NUNCA RECOMECAVA.
//
// Log Q9TFVA (LG, 2.0.6): perfil com PIN escolhido na tela de perfis, a volta
// do arranque (sessao velha) e condenada "lendo os manifestos" e desc_iniciar
// recusa calado: a sessao nova nunca foi aberta (app.c so chamava
// perfilAbrirSessao quando a escolha vinha da tecla, nao do PIN conferido em
// perfilsel_atualizar). Nenhuma volta depois disso, a Home sem catalogos.
//
//   1. sessao trocada sem abertura: a volta condenada e recusada COM linha no log;
//   2. aberta a sessao (o que app.c passa a fazer), uma volta nova comeca.
// montar() nao roda: o gancho de pthread_create conta e troca por um fio vazio.
//
//   bash tests/desc_sessao_pin.sh
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#define NV_PERFIL_SESSAO_FORTE
#define NV_TEST_SESSAO_PROPRIA
static void *montar(void *u);
static int testeCriar(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg);
#define pthread_create(t, a, f, arg) testeCriar(t, a, f, arg)
#define main montagem_cedo_main
#include "montagem_cedo.c"
#undef main
#undef pthread_create

static _Atomic uint64_t sessaoG = 1;
uint64_t perfis_sessao_geracao(void) { return atomic_load(&sessaoG); }
int perfis_sessao_atual(uint64_t g) { return g == perfis_sessao_geracao(); }
int perfis_sessao_travar(uint64_t g) { return perfis_sessao_atual(g); }
void perfis_sessao_destravar(void) {}

static atomic_int montares;
static void *fioVazio(void *u) { (void)u; return NULL; }
static int testeCriar(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg) {
  if (f == montar) { atomic_fetch_add(&montares, 1); return pthread_create(t, a, fioVazio, NULL); }
  return pthread_create(t, a, f, arg);
}

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { printf("FALHOU: "); falhas++; } else printf("ok: "); \
                             printf(__VA_ARGS__); printf("\n"); } while (0)

#ifndef NV_DESC_RECOVERY_TEST
int main(void) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  // arranque: perfilAbrirSessao + sync_iniciar, depois main.c larga a volta
  desc_abrir_sessao();
  desc_sessao_preparada();
  desc_iniciar();
  CONFERE(atomic_load(&montares) == 1, "arranque larga uma volta");

  // PIN conferido: perfis_definir_ativo avanca a sessao; a volta velha e
  // condenada e faz o que o rotulo `condenada` de montar() faz.
  atomic_fetch_add(&sessaoG, 1);
  buscandoSoltar();
  desc_iniciar();
  CONFERE(atomic_load(&montares) == 1, "sessao nao aberta: a volta condenada e recusada");

  // app.c abre a sessao no quadro seguinte (perfilAbrirSessao + sync_iniciar)
  desc_abrir_sessao();
  desc_sessao_preparada();
  CONFERE(atomic_load(&montares) == 2, "sessao aberta: a volta recomeca (foram %d)", atomic_load(&montares));
  printf(falhas ? "desc_sessao_pin: %d falha(s)\n" : "desc_sessao_pin: ok\n", falhas);
  return falhas ? 1 : 0;
}
#endif
