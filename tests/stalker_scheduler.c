// Teste comportamental do contrato de jobs de fonte em app.c.
//
// O app.c e incluido para exercitar a implementacao real do scheduler. O
// linker remove as telas que nao entram neste teste; os stubs abaixo isolam
// somente player, streams e rede Stalker. A barreira do resolver permite
// reproduzir uma resposta velha sem depender de rede ou de temporizacao.
#define SDL_MAIN_HANDLED
#include "../src/app.c"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
  pthread_mutex_t trava;
  pthread_cond_t evento;
  int chamadas;
  int ativas;
  int maxAtivas;
  int iniciadas;
  int liberar;
  char ids[8][80];
} Barreira;

static Barreira resolverBarreira = {
  PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0, 0, 0, 0, {{0}}
};
static int idPlayerAberto;
static int playerSaindo;
static char idCanal[80];
static int montagens;
static char ultimaUrl[4096];
static int errosPlayer;
static int fonteAtual;
static Stream lista[2];
static int listaN;
static int addonChamadas;

static void barreira_reset(void) {
  pthread_mutex_lock(&resolverBarreira.trava);
  resolverBarreira.chamadas = 0;
  resolverBarreira.ativas = 0;
  resolverBarreira.maxAtivas = 0;
  resolverBarreira.iniciadas = 0;
  resolverBarreira.liberar = 0;
  memset(resolverBarreira.ids, 0, sizeof resolverBarreira.ids);
  pthread_mutex_unlock(&resolverBarreira.trava);
}

static void esperar_inicio(int n) {
  struct timespec limite;
  clock_gettime(CLOCK_REALTIME, &limite);
  limite.tv_sec += 2;
  pthread_mutex_lock(&resolverBarreira.trava);
  while (resolverBarreira.iniciadas < n) {
    int r = pthread_cond_timedwait(&resolverBarreira.evento,
                                   &resolverBarreira.trava, &limite);
    assert(r == 0 || r == ETIMEDOUT);
    assert(r != ETIMEDOUT && "worker nao iniciou dentro do limite");
  }
  pthread_mutex_unlock(&resolverBarreira.trava);
}

static void liberar_resolver(void) {
  pthread_mutex_lock(&resolverBarreira.trava);
  resolverBarreira.liberar = 1;
  pthread_cond_broadcast(&resolverBarreira.evento);
  pthread_mutex_unlock(&resolverBarreira.trava);
}

static void esperar_job_pronto(void) {
  for (int i = 0; i < 2000; i++) {
    if (atomic_load_explicit(&fonteJob.estado, memory_order_acquire) == FJOB_DONE)
      return;
    usleep(1000);
  }
  assert(!"worker nao concluiu dentro do limite");
}

static void limpar_cenario(void) {
  liberar_resolver();
  if (fioFonteVivo) {
    esperar_job_pronto();
    assert(pthread_join(fioFonte, NULL) == 0);
    fioFonteVivo = 0;
  }
  atomic_store_explicit(&fonteJob.estado, FJOB_IDLE, memory_order_release);
  fonteJob.tipo = FJOB_NONE;
  fonteJob.renovando = 0;
  fonteJob.resultado = -1;
  fonteJob.geracao = 0;
  fontePedidoGeracao = 0;
  atomic_store_explicit(&stalkerGeracao, 0, memory_order_release);
  aguardandoFonte = 2;
  fonteEscolhida = -2;
  stalkerRenovando = 0;
  canalFonteIdx = -1;
  idPlayerAberto = 1;
  playerSaindo = 0;
  snprintf(idCanal, sizeof idCanal, "%s", "stalker:reset");
  montagens = 0;
  errosPlayer = 0;
  fonteAtual = -1;
  ultimaUrl[0] = 0;
  listaN = 0;
  addonChamadas = 0;
  limparFontePendente();
  barreira_reset();
}

static unsigned novo_pedido(const char *id) {
  unsigned g = novaGeracaoFonte();
  fontePedidoGeracao = g;
  aguardandoFonte = 2;
  snprintf(idCanal, sizeof idCanal, "%s", id ? id : "");
  return g;
}

static void iniciar_stalker(const char *id, int renovando) {
  unsigned g = novo_pedido(id);
  int r = pedirFonteJob(FJOB_STALKER, g, id, renovando);
  assert(r == 0);
  esperar_inicio(1);
}

static void concluir_job(void) {
  esperar_job_pronto();
  processarFonteJob();
}

// ---- Stubs do caminho alcançado por processarFonteJob/escolherFonte* ----

int stalker_resolver(const char *id, char *url, unsigned n) {
  pthread_mutex_lock(&resolverBarreira.trava);
  int indice = resolverBarreira.chamadas++;
  assert(indice < (int)(sizeof resolverBarreira.ids / sizeof resolverBarreira.ids[0]));
  snprintf(resolverBarreira.ids[indice], sizeof resolverBarreira.ids[indice], "%s", id);
  resolverBarreira.ativas++;
  if (resolverBarreira.ativas > resolverBarreira.maxAtivas)
    resolverBarreira.maxAtivas = resolverBarreira.ativas;
  resolverBarreira.iniciadas++;
  pthread_cond_broadcast(&resolverBarreira.evento);
  while (!resolverBarreira.liberar)
    pthread_cond_wait(&resolverBarreira.evento, &resolverBarreira.trava);
  resolverBarreira.ativas--;
  pthread_mutex_unlock(&resolverBarreira.trava);
  snprintf(url, n, "https://fresh/%s", id);
  return 1;
}

int stream_primeira_boa(int tentativas) {
  (void)tentativas;
  __atomic_add_fetch(&addonChamadas, 1, __ATOMIC_RELAXED);
  return 7;
}
int stream_primeira_boa_sessao(int tentativas, uint64_t sessao) {
  (void)sessao; return stream_primeira_boa(tentativas);
}
int stream_verificar_proxima_vod(int atual, int tentativas, int *conferidas) {
  (void)atual; (void)tentativas; if (conferidas) *conferidas = 0; return -1;
}
int stream_verificar_proxima_vod_sessao(int atual, int tentativas, int *conferidas, uint64_t sessao) {
  (void)sessao; return stream_verificar_proxima_vod(atual, tentativas, conferidas);
}
int stream_canal_primeira_viva(int tentativas) { (void)tentativas; return -1; }
int stream_canal_primeira_viva_sessao(int tentativas, uint64_t sessao) {
  (void)sessao; return stream_canal_primeira_viva(tentativas);
}
int stalker_resolver_sessao(const char *id, char *url, unsigned n, uint64_t sessao) {
  (void)sessao; return stalker_resolver(id, url, n);
}
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t g) { return g == 1; }
int perfis_sessao_travar(uint64_t g) { return g == 1; }
void perfis_sessao_destravar(void) {}
int perfis_ativo(void) { return 1; }
const char *sessao_usuario(void) { return ""; }
void debrid_contexto_capturar(DebridContexto *c) { if (c) memset(c, 0, sizeof *c); }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
int debrid_servico_disponivel(int servico) { (void)servico; return 0; }
unsigned fonteregra_versao(void) { return 0; }
int detail_aberto(void) { return 0; }
int detail_indice(void) { return 0; }
int detail_ep_foco(int *t, int *e) { if (t) *t = 0; if (e) *e = 0; return 0; }
int stream_lista_do_alvo(const char *id) { (void)id; return 1; }
int stream_falha_verificar(FonteFalhaSessao *s, unsigned ger, int escolhida,
                          int (*cancelado)(void *), void *ctx,
                          int *idx, int *serv, char *url, unsigned n) {
  (void)s; (void)ger; (void)escolhida; (void)cancelado; (void)ctx; (void)n;
  if (idx) *idx = -1; if (serv) *serv = -1; if (url) url[0] = 0; return 0;
}
int stream_falha_aplicar_sessao(int idx, unsigned ger, int serv, const char *url, uint64_t sessao) {
  (void)idx; (void)ger; (void)serv; (void)url; (void)sessao; return 0;
}
int stream_falha_renovar(int indice, unsigned ger, int servico,
                         int (*cancelado)(void *), void *ctx, char *url, unsigned tam) {
  (void)indice; (void)ger; (void)servico; (void)cancelado; (void)ctx; (void)tam;
  if (url) url[0] = 0; return 0;
}
void fontefalha_config(FonteFalhaConfig *saida) { if (saida) memset(saida, 0, sizeof *saida); }
int fontefalha_ativa(void) { return 0; }
FonteFalhaSessao *fontefalha_sessao_nova(const FonteFalhaConfig *c, unsigned d, int p) {
  (void)c; (void)d; (void)p; return NULL;
}
FonteFalhaSessao *fontefalha_sessao_clonar(const FonteFalhaSessao *s) { (void)s; return NULL; }
void fontefalha_sessao_liberar(FonteFalhaSessao *s) { (void)s; }
int fontefalha_url_registrar(FonteFalhaSessao *s, const char *url) { (void)s; (void)url; return 0; }
void player_definir_tentativa(int n, int t) { (void)n; (void)t; }
int ajustes_fonte_preparar(void) { return 0; }
int ajustes_fonte_precarregar(void) { return 0; }
int ajustes_fonte_manual(void) { return 0; }
int ajustes_fonte_primeira(void) { return 0; }
int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_regex_modo(void) { return 0; }
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_fonte_ordem_uso(void) { return 0; }
int ajustes_fonte_prioridade(void) { return 0; }
int ajustes_fonte_hdr(void) { return 0; }
int ajustes_fonte_conferir_varias(void) { return 0; }
int ajustes_fonte_tocar_conferindo(void) { return 0; }
int ajustes_dolby_vision(void) { return 0; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_dolby_atmos(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_buffer_preset(void) { return 0; }
int ajustes_cache_seek_mb(void) { return 0; }
const char *ajustes_qualidade(void) { return ""; }
int stream_canal_classe_escolhida(void) { return 0; }
int stream_canal_proxima(int a) { return a + 1; }
int stream_canal_prazo_longo(int i) { (void)i; return 1; }
int stream_automatico(void) { return -1; }

void stream_definir_lista(const Stream *nova, int n) {
  montagens++;
  listaN = n > 2 ? 2 : n;
  if (listaN > 0 && nova) lista[0] = nova[0];
}

const Stream *stream_item(int indice) {
  return indice >= 0 && indice < listaN ? &lista[indice] : NULL;
}

void stream_definir_atual(int indice) { fonteAtual = indice; }

const char *player_id_canal(void) { return idCanal; }
const char *player_tipo_canal(void) { return idCanal[0] ? "tv" : ""; }
int player_aberto(void) { return idPlayerAberto; }
int player_mini_ativo(void) { return 0; }
int player_quer_sair(void) { return playerSaindo; }
void player_definir_fonte(const char *url) {
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url ? url : "");
}
void player_erro_fonte(void) { errosPlayer++; }
void player_fechar_mini(void) { idPlayerAberto = 0; }
void marco(const char *texto) { (void)texto; }
// Dobles inertes: o scheduler (app.c) os referencia, mas estes testes nao os exercitam.
int cat_id_stream(int i, int t, int e, char *dst, unsigned tam) { (void)i; (void)t; (void)e; (void)dst; (void)tam; return 0; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
void player_episodio_atual(int *t, int *e) { if (t) *t = 0; if (e) *e = 0; }
int player_indice(void) { return -1; }
int stream_n(void) { return listaN; }
int stream_automatico_canal(void) { return -1; }
int stream_automatico_disponivel(int i) { (void)i; return 0; }
Uint32 stream_idade_ms(void) { return 0; }
unsigned stream_lista_geracao(void) { return 0; }

static void test_gera_invalida_e_single_flight(void) {
  limpar_cenario();
  iniciar_stalker("stalker:A", 0);

  // A primeira resposta fica presa. B entra como pendente, depois A substitui
  // B: o resultado velho nao pode montar nada e so um novo worker deve nascer.
  unsigned gB = novo_pedido("xtream:B");
  assert(pedirFonteJob(FJOB_ADDON, gB, NULL, 0) == 1);
  unsigned gA2 = novo_pedido("stalker:A");
  assert(pedirFonteJob(FJOB_STALKER, gA2, "stalker:A", 0) == 1);
  assert(resolverBarreira.maxAtivas == 1);

  liberar_resolver();
  concluir_job();
  esperar_inicio(2);
  assert(montagens == 0);
  assert(resolverBarreira.iniciadas == 2);
  assert(resolverBarreira.maxAtivas == 1);
  // O segundo pedido e o A mais novo; B nunca deve aparecer no resolver.
  assert(!strcmp(resolverBarreira.ids[0], "stalker:A"));
  assert(!strcmp(resolverBarreira.ids[1], "stalker:A"));
  concluir_job();
  assert(montagens == 1);
  assert(!strcmp(ultimaUrl, "")); // o caminho inicial publica a lista, nao o player
}

static void test_troca_xtream_nao_espera_e_addon_serializa(void) {
  limpar_cenario();
  iniciar_stalker("stalker:old", 0);

  struct timespec antes, depois;
  clock_gettime(CLOCK_MONOTONIC, &antes);
  processarFonteJob();
  clock_gettime(CLOCK_MONOTONIC, &depois);
  long ms = (depois.tv_sec - antes.tv_sec) * 1000L +
            (depois.tv_nsec - antes.tv_nsec) / 1000000L;
  assert(ms < 100 && "polling do job bloqueou esperando rede");

  unsigned g = novo_pedido("xtream:new");
  fonteEscolhida = 0; // resultado imediato do ramo Xtream
  (void)g;
  assert(montagens == 0);
  liberar_resolver();
  concluir_job();
  assert(montagens == 0);

  // O slot liberado pode entao aceitar a busca de addon da nova tela.
  limpar_cenario();
  iniciar_stalker("stalker:old", 0);
  unsigned gAddon = novo_pedido("");
  assert(pedirFonteJob(FJOB_ADDON, gAddon, NULL, 0) == 1);
  liberar_resolver();
  concluir_job();
  concluir_job();
  assert(addonChamadas == 1);
  assert(fonteEscolhida == 7);
  assert(resolverBarreira.maxAtivas == 1);
}

static void test_renovacao_apos_sair_e_descartada(void) {
  limpar_cenario();
  iniciar_stalker("stalker:renew", 1);
  stalkerRenovando = 1;
  idPlayerAberto = 0;
  playerSaindo = 1;
  cancelarFonteSeSaiu();
  assert(stalkerRenovando == 0);
  liberar_resolver();
  concluir_job();
  assert(montagens == 0);
  assert(errosPlayer == 0);
  assert(!strcmp(ultimaUrl, ""));
}

int main(void) {
  test_gera_invalida_e_single_flight();
  test_troca_xtream_nao_espera_e_addon_serializa();
  test_renovacao_apos_sair_e_descartada();
  puts("stalker-scheduler: tudo ok");
  return 0;
}
