// Run the production drawing-thread recovery block against real discovery state.
#define NV_DESC_RECOVERY_TEST
#include "desc_sessao_pin.c"

enum { TELA_HOME, TELA_ESCOLHA_PERFIL };
static int tela = TELA_HOME, perfilAntes = 1;
static uint64_t perfilSessaoAplicada = 1;
static unsigned trocaPerfilDesde;
static pthread_t desenho;
static int aberturas;
int sessao_logada(void) { return 1; }
int perfilsel_concluido(void) { return 1; }
void perfilsel_preparar(int v, unsigned t) { (void)v; (void)t; }
static void perfilAbrirSessao(int antes) {
  (void)antes;
  assert(pthread_equal(pthread_self(), desenho));
  aberturas++;
  desc_abrir_sessao();
  perfilSessaoAplicada = perfis_sessao_geracao();
}
void sync_iniciar(void) { desc_sessao_preparada(); }
static void quadro(unsigned agora) {
#include "recovery.inc"
}
static void *reiniciar(void *u) { (void)u; desc_iniciar(); return NULL; }

int main(void) {
  desenho = pthread_self();
  desc_abrir_sessao();
  desc_sessao_preparada();
  desc_iniciar();
  assert(atomic_load(&montares) == 1);
  // A startup HTTP 401 invalidates the same profile; renewal keeps login alive.
  atomic_fetch_add(&sessaoG, 1);
  buscandoSoltar();
  pthread_t worker;
  assert(pthread_create(&worker, NULL, reiniciar, NULL) == 0);
  pthread_join(worker, NULL);
  assert(atomic_load(&montares) == 1);
  quadro(100);
  CONFERE(aberturas == 1, "Home reopens a stale session on the drawing thread");
  CONFERE(atomic_load(&montares) == 2, "refused discovery resumes after opening");
  CONFERE(!aberturaPendente, "a started round consumes the pending request");
  quadro(101);
  CONFERE(aberturas == 1, "an applied session is not reopened every frame");
  // A drawing-thread opening may also be missing while app generation matches.
  buscandoSoltar();
  aberturaSessao = 0;
  desc_iniciar();
  CONFERE(aberturaPendente, "unopened discovery retains its request");
  quadro(102);
  CONFERE(aberturas == 2 && atomic_load(&montares) == 3,
          "Home repairs an unopened discovery session too");
  return falhas ? 1 : 0;
}
