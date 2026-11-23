// Sem login na conta Nuvio o worker de Perfil/Stats nao publicava nada e
// deixava perfilCarga presa em 1: a tela ficava para sempre em carregando
// ("Refreshing history...") e toda nova tentativa saia cedo no CAS de
// pedirPerfil. O worker, o pedido e a flag sao os de app.c; trackers,
// Diario, sessao e a fachada do Perfil sao dubles — sem rede, janela nem
// dados pessoais.
#define trakt_ativo teste_trakt_ativo
#define trakt_perfil teste_trakt_perfil
#define simkl_ativo teste_simkl_ativo
#define simkl_perfil teste_simkl_perfil
#define sessao_logada teste_sessao_logada
#define sessao_usuario teste_sessao_usuario
#define perfis_ativo teste_perfis_ativo
#define perfis_item_ativo teste_perfis_item_ativo
#define diario_perfil_dados teste_diario_perfil_dados
#define perfil_definir_carregando teste_perfil_carregando
#define perfil_definir_dados teste_perfil_dados
#define perfil_definir_erro teste_perfil_erro
#define perfil_definir_estado teste_perfil_estado
#define retomamorna_cancelar teste_noop
#define player_descartar_retido teste_noop
#define cacheboost_backend_descartar teste_noop
#define ilhacart_esquecer_vivo teste_noop
#define home_retomar_esquecer teste_noop
#define cacheboost_backend_quadro teste_noop_quadro
#include "../src/app.c"
#include "stub_m3u.h"
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { \
  fprintf(stderr, "FALHOU: %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

static int tkLigado, skLigado, logado, diarioOk, diarioChamadas;
static int perfilAtual = 1;
static const char *contaAtual = "";
static int carregandoFlag, dadosOk, ultimoEstado = -1;
static char ultimaMsg[200];

int teste_trakt_ativo(void) { return tkLigado; }
int teste_trakt_perfil(PerfilDados *d) { snprintf(d->nome, sizeof d->nome, "Trakt"); return 1; }
int teste_simkl_ativo(void) { return skLigado; }
int teste_simkl_perfil(PerfilDados *d) { snprintf(d->nome, sizeof d->nome, "Simkl"); return 1; }
int teste_sessao_logada(void) { return logado; }
const char *teste_sessao_usuario(void) { return contaAtual; }
int teste_perfis_ativo(void) { return perfilAtual; }
const ContaPerfil *teste_perfis_item_ativo(void) {
  static ContaPerfil cp;
  if (!cp.nome[0]) snprintf(cp.nome, sizeof cp.nome, "Perfil Local");
  return &cp;
}
int teste_diario_perfil_dados(int perfil, PerfilDados *d, long long agora) {
  (void)perfil; (void)agora;
  diarioChamadas++;
  if (!diarioOk) return 0;
  memset(d, 0, sizeof *d);
  snprintf(d->nome, sizeof d->nome, "Diario");
  return 1;
}
unsigned long long simklauth_credencial_copiar(char *dst, size_t tam) {
  (void)dst; (void)tam; return 0;
}
void teste_noop(void) {}
int teste_noop_quadro(int guardar) { (void)guardar; return 0; }
void teste_perfil_carregando(int c) { carregandoFlag = c; }
void teste_perfil_dados(const PerfilDados *d) {
  dadosOk = d != NULL;
  ultimoEstado = d ? PERFIL_ESTADO_PRONTO : -1;
}
void teste_perfil_erro(const char *m) {
  ultimoEstado = PERFIL_ESTADO_ERRO;
  snprintf(ultimaMsg, sizeof ultimaMsg, "%s", m ? m : "");
}
void teste_perfil_estado(PerfilEstado e, const char *m) {
  ultimoEstado = e;
  snprintf(ultimaMsg, sizeof ultimaMsg, "%s", m ? m : "");
}

static void ambiente(int lg, const char *conta, int tk, int sk, int dia, int perfil) {
  logado = lg; contaAtual = conta; tkLigado = tk; skLigado = sk;
  diarioOk = dia; perfilAtual = perfil;
  ultimoEstado = -1; dadosOk = 0; ultimaMsg[0] = 0;
  // O snapshot anterior ja foi consumido/invalidado: começa limpo, como na
  // vida real depois de invalidarPerfil ou do consumo da carga==2.
  pthread_mutex_lock(&perfilTrava);
  memset(&perfilPendente, 0, sizeof perfilPendente); perfilSucesso = 0;
  pthread_mutex_unlock(&perfilTrava);
}

static int esperarCarga(int alvo, int ms) {
  int t;
  for (t = 0; t < ms; t += 5) {
    if (atomic_load_explicit(&perfilCarga, memory_order_acquire) == alvo) return 1;
    usleep(5000);
  }
  return atomic_load_explicit(&perfilCarga, memory_order_acquire) == alvo;
}

// Espelho do consumidor do laco principal (app.c, ~6077): carga==2 aplica o
// snapshot ou um estado claro e devolve a flag a 0, liberando nova carga.
static int consumir(void) {
  PerfilDados snap; int ok;
  if (atomic_load_explicit(&perfilCarga, memory_order_acquire) != 2) return 0;
  pthread_mutex_lock(&perfilTrava); snap = perfilPendente; ok = perfilSucesso;
  pthread_mutex_unlock(&perfilTrava);
  if (ok) perfil_definir_dados(&snap);
  else if (!trakt_ativo() && !simkl_ativo())
    perfil_definir_estado(PERFIL_ESTADO_DESCONECTADO, "Seu Diário ainda está vazio.");
  else perfil_definir_estado(PERFIL_ESTADO_INDISPONIVEL, "Perfil indisponível.");
  atomic_store_explicit(&perfilCarga, 0, memory_order_release);
  return 1;
}

// Pedido montado como pedirPerfil monta (calloc zera o resto), para o worker
// rodar sincrono no cenario de descarte. So serve para pedido feito
// DESLOGADO: conta "" e nada mais preenchido.
static PerfilPedido *pedidoDeslogado(void) {
  PerfilPedido *p = calloc(1, sizeof *p);
  CHECK(p);
  p->geracao = atomic_load_explicit(&perfilGeracao, memory_order_acquire);
  p->perfil = perfis_ativo();
  snprintf(p->conta, sizeof p->conta, "%s", sessao_usuario());
  return p;
}

static void publicado(int *ok, char *nome, size_t n) {
  pthread_mutex_lock(&perfilTrava);
  *ok = perfilSucesso;
  snprintf(nome, n, "%s", perfilPendente.nome);
  pthread_mutex_unlock(&perfilTrava);
}

int main(void) {
  char nome[80]; int ok;
  unsigned gen0;

  // 1. SEM LOGIN, Diario local com dados: a carga TERMINA (carga 0->1->2),
  // publica o snapshot do Diario e a flag volta a permitir nova tentativa.
  ambiente(0, "", 0, 0, 1, 1);
  pedirPerfil();
  CHECK(carregandoFlag == 1);
  CHECK(esperarCarga(2, 3000));
  CHECK(diarioChamadas > 0);
  publicado(&ok, nome, sizeof nome);
  CHECK(ok == 1 && !strcmp(nome, "Perfil Local"));   // Diario + nome do perfil ativo
  CHECK(consumir());
  CHECK(ultimoEstado == PERFIL_ESTADO_PRONTO && dadosOk);
  CHECK(atomic_load(&perfilCarga) == 0);
  // Nova tentativa sai do CAS e completa de novo.
  pedirPerfil();
  CHECK(esperarCarga(2, 3000));
  CHECK(consumir());
  CHECK(atomic_load(&perfilCarga) == 0);

  // 2. SEM LOGIN, Diario vazio: publica sucesso=0 e o consumidor mostra o
  // estado desconectado claro, nao carregando sem fim.
  ambiente(0, "", 0, 0, 0, 1);
  pedirPerfil();
  CHECK(esperarCarga(2, 3000));
  publicado(&ok, nome, sizeof nome);
  CHECK(ok == 0);
  CHECK(consumir());
  CHECK(ultimoEstado == PERFIL_ESTADO_DESCONECTADO);
  CHECK(atomic_load(&perfilCarga) == 0);
  pedirPerfil();
  CHECK(esperarCarga(2, 3000));
  CHECK(consumir());
  CHECK(atomic_load(&perfilCarga) == 0);

  // 3. COM LOGIN: publica como antes (Trakt primeiro).
  ambiente(1, "conta-A", 1, 0, 0, 1);
  pedirPerfil();
  CHECK(esperarCarga(2, 3000));
  publicado(&ok, nome, sizeof nome);
  CHECK(ok == 1 && !strcmp(nome, "Trakt"));
  CHECK(consumir());
  CHECK(atomic_load(&perfilCarga) == 0);

  // 4. Troca de perfil durante a carga: a resposta e descartada e a
  // invalidacao da troca devolve a flag a 0 — nova carga sai normal.
  ambiente(0, "", 0, 0, 1, 1);
  { PerfilPedido *p = pedidoDeslogado();
    atomic_store(&perfilCarga, 1);
    perfilAtual = 2;
    carregarPerfil(p);
    CHECK(atomic_load(&perfilCarga) == 1);
    publicado(&ok, nome, sizeof nome);
    CHECK(ok == 0 && !nome[0]); }
  gen0 = atomic_load(&perfilGeracao);
  invalidarPerfil();
  CHECK(atomic_load(&perfilGeracao) == gen0 + 1);
  CHECK(atomic_load(&perfilCarga) == 0);
  ambiente(0, "", 0, 0, 1, 2);
  pedirPerfil();
  CHECK(esperarCarga(2, 3000));
  CHECK(consumir());
  CHECK(atomic_load(&perfilCarga) == 0);

  // 5. Login no meio da carga: o pedido feito deslogado e descartado.
  ambiente(0, "", 0, 0, 1, 1);
  { PerfilPedido *p = pedidoDeslogado();
    atomic_store(&perfilCarga, 1);
    logado = 1; contaAtual = "conta-A";
    carregarPerfil(p);
    CHECK(atomic_load(&perfilCarga) == 1);
    publicado(&ok, nome, sizeof nome);
    CHECK(ok == 0 && !nome[0]); }
  invalidarPerfil();
  CHECK(atomic_load(&perfilCarga) == 0);

  // 6. Troca de conta: a resposta montada para a conta antiga nao publica.
  ambiente(0, "", 0, 0, 1, 1);
  { PerfilPedido *p = pedidoDeslogado();
    snprintf(p->conta, sizeof p->conta, "%s", "conta-A");
    atomic_store(&perfilCarga, 1);
    logado = 1; contaAtual = "conta-B";
    carregarPerfil(p);
    CHECK(atomic_load(&perfilCarga) == 1);
    publicado(&ok, nome, sizeof nome);
    CHECK(ok == 0 && !nome[0]); }
  invalidarPerfil();
  CHECK(atomic_load(&perfilCarga) == 0);

  // 7. Logout no meio da carga: pedido da conta que saiu nao publica.
  ambiente(0, "", 0, 0, 1, 1);
  { PerfilPedido *p = pedidoDeslogado();
    snprintf(p->conta, sizeof p->conta, "%s", "conta-A");
    atomic_store(&perfilCarga, 1);
    carregarPerfil(p);
    CHECK(atomic_load(&perfilCarga) == 1);
    publicado(&ok, nome, sizeof nome);
    CHECK(ok == 0 && !nome[0]); }
  invalidarPerfil();
  CHECK(atomic_load(&perfilCarga) == 0);

  puts("perfil_semlogin: tudo ok");
  return 0;
}
