// #551: late PIN completion must open the new session before leaving the picker.
// Real profile identity, PIN RPC parsing, picker selection/completion, app routing,
// discovery and catalog publication. Only account/transport and unrelated UI hooks
// are fixtures. pinrows.sh extracts the production blocks, including old revisions.
#define MONTAGEM_PERFIS_REAIS
#define NV_PERFIL_SESSAO_FORTE
#define main montagem_cedo_original_main
#include "montagem_cedo.c"
#undef main
#include "perfis.h"
#include <stdatomic.h>

void fil_definir_perfil(int p) { (void)p; }
void fontepref_definir_perfil(int p) { (void)p; }
void legmem_definir_perfil(int p) { (void)p; }
void audiomem_definir_perfil(int p) { (void)p; }
void arteesc_definir_perfil(int p) { (void)p; }
void plugins_perfil_mudou(void) {}
void servidores_perfil_trocou(void) {}
int ajustes_addons_do_principal(void) { return 0; }
static int pinRequests;
char *sessao_rpc(const char *name, const char *body, int *status) {
  assert(!strcmp(name, "verify_profile_pin"));
  assert(strstr(body, "\"p_profile_id\":2"));
  pinRequests++;
  *status = 200;
  return strdup(strstr(body, "\"p_pin\":\"4271\"") ? "true" : "false");
}

#define PS_PIN_MAX 8
#define PS_PIN_LEN 4
#define PS_PIN_INICIO 4
static int pinDe = -1, concluido, preparando;
#include "picker.inc"
int perfilsel_concluido(void) { return concluido; }
int perfilsel_preparando(void) { return preparando; }
void perfilsel_preparar(int v, Uint32 now) { (void)now; preparando = v; }
static void perfilsel_atualizar(float dt, Uint32 now) {
  (void)dt; (void)now;
  int reduzida = 1;
#include "pin-result.inc"
}

// Keep the account gate closed until the selected profile's sources arrive.
// The fixture transport serves the manifest/catalogs from montagem_cedo.c.
static atomic_int sourcesReady;
static int waitAccount(void) { return atomic_load(&sourcesReady) ? 0 : 2; }
static uint64_t perfilSessaoAplicada;
static int perfilAntes;
static pthread_t drawingThread;
static void perfilAbrirSessao(int before) {
  assert(pthread_equal(pthread_self(), drawingThread));
  (void)before;
  atomic_store(&sourcesReady, 0);
  desc_esperar_conta();
  desc_abrir_sessao();
  perfilSessaoAplicada = perfis_sessao_geracao();
}
int sessao_logada(void) { return 1; }
void sync_iniciar(void) { desc_sessao_preparada(); }
enum { TELA_HOME, TELA_ESCOLHA_PERFIL };
static int tela = TELA_ESCOLHA_PERFIL;
static Uint32 trocaPerfilDesde;
static void frame(Uint32 agora) {
  float dt = 0.016f;
#include "recovery.inc"
  if (tela == TELA_ESCOLHA_PERFIL) {
    perfilsel_atualizar(dt, agora);
#include "completion.inc"
  }
}

static void enterPin(const char *digits) {
  for (const char *p = digits; *p; p++) pinDigito(*p - '0');
  unsigned long long end = agoraMs() + 3000;
  while (!atomic_load(&resultadoPin) && agoraMs() < end) usleep(1000);
  assert(atomic_load(&resultadoPin));
}
static int catalogRequests(void) {
  pthread_mutex_lock(&pedTrava);
  int n = pedidosCatalogo;
  pthread_mutex_unlock(&pedTrava);
  return n;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  int startup = !strcmp(argv[1], "boot");
  int withPin = strcmp(argv[1], "plain") != 0;
  const char *root = getenv("NV_T_DIR"); assert(root && *root);
  snprintf(dirDados, sizeof dirDados, "%s", root);
  assert(dados_gravar("perfil.txt", startup ? "2\n" : "1\n"));
  assert(dados_gravar("perfis.txt", withPin
    ? "1\t0\t1\t0\t#FFFFFF\tPlain\t\t\n2\t1\t0\t0\t#FFFFFF\tLocked\t\t\n"
    : "1\t0\t1\t0\t#FFFFFF\tPlain\t\t\n2\t0\t0\t0\t#FFFFFF\tPlain 2\t\t\n"));
  perfis_carregar_ativo();
  desc_tmdb(dirDados); homeestado_iniciar(); fixtureCW = 1;
  drawingThread = pthread_self();
  perfilAntes = perfis_ativo();
  perfilAbrirSessao(perfilAntes);
  desc_espera_addons_definir(waitAccount);
  sync_iniciar();
  desc_iniciar();   // main.c starts the initial discovery round after app startup.
  escolher(1);
  if (withPin) {
    assert(!concluido && perfis_ativo() == perfilAntes);
    enterPin("0000"); frame(1);
    assert(!concluido && perfis_ativo() == perfilAntes && pinErrado);
    enterPin("4271");
  }
  // Plain selection changes identity before frame(); PIN changes it inside.
  frame(2); frame(3);
  assert(perfis_ativo() == 2 && concluido);
  // No catalog request may escape the account gate for the target session.
  usleep(100000);
  assert(catalogRequests() == 0);
  atomic_store(&sourcesReady, 1);
  unsigned long long end = agoraMs() + 5000;
  while (desc_montando() && agoraMs() < end) usleep(1000);
  assert(!desc_montando());
  if (catalogRequests() != 3 || !temFileira("cat_a") ||
      !temFileira("cat_b") || !temFileira("cat_c")) {
    fprintf(stderr, "FAIL %s: %d catalog requests, %d rows; session opened=%d (expected 3 catalog rows + Continue Watching)\n",
            argv[1], catalogRequests(), cat_n_fileiras(), desc_sessao_aberta());
    return 1;
  }
  assert(perfilSessaoAplicada == perfis_sessao_geracao() && desc_sessao_aberta());
  CatItem cw[2];
  assert(cat_copiar_fileira("continue_watching", cw, 2, NULL) == 1);
  assert(!strcmp(cw[0].imdb, "ttCW"));
  assert(pinRequests == (withPin ? 2 : 0));
  printf("PASS %s: 3 catalog rows requested/present + Continue Watching\n", argv[1]);
  return 0;
}
