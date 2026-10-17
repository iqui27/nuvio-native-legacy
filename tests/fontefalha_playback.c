// Exercise the production app worker mailbox and playback guards. Only player,
// network and UI boundaries are mocked; the failover session is the real policy.
#define _GNU_SOURCE
#define SDL_MAIN_HANDLED
#include <pthread.h>
static int fake_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
static int fake_join(pthread_t, void **);
#define pthread_create fake_create
#define pthread_join fake_join
#include "../src/app.c"
#undef pthread_create
#undef pthread_join
#include <assert.h>

static Uint32 agora;
static unsigned lista = 41;
static Stream fontes[2];
static int aberto, mini, sair, carregando, pronto, falhou, playerFalhou;
static int reconectando, tocou, dv, bufferMs, proximaSemPerda;
static double posicao, bufferFim;
static int paradas, retomadas, aberturas, aplicacoes, legadas, mortas, irmas;
static int tentativaN, tentativaT, evento, eventoRetomada, eventoParada;
static double retomada;
static void *(*worker)(void *);
static void *workerArg;
static FonteFalhaConfig configuracao;
static int workerEsperar, workerEscolhida;

static int fake_create(pthread_t *t, const pthread_attr_t *attr,
                       void *(*fn)(void *), void *arg) {
  (void)attr; assert(!worker); *t = (pthread_t)1; worker = fn; workerArg = arg; return 0;
}
static int fake_join(pthread_t t, void **ret) {
  (void)t; (void)ret; assert(!worker && "UI only joins after worker completion"); return 0;
}
static void concluirWorker(void) {
  assert(worker); void *(*fn)(void *) = worker; void *arg = workerArg;
  worker = NULL; workerArg = NULL; fn(arg);
}

Uint32 SDL_GetTicks(void) { return agora; }
int player_aberto(void) { return aberto; }
int player_mini_ativo(void) { return mini; }
int player_quer_sair(void) { return sair; }
const char *player_id_canal(void) { return ""; }
const char *player_tipo_canal(void) { return ""; }
int player_carregando(void) { return carregando; }
int player_fonte_falhou(void) { return playerFalhou; }
void player_voltar_a_esperar(void) {
  paradas++; eventoParada = ++evento; posicao = 0.0; pronto = 0; carregando = 1;
}
void player_definir_retomada_fonte(double p) {
  retomadas++; retomada = p; eventoRetomada = ++evento;
}
void player_definir_fonte(const char *url) { (void)url; aberturas++; }
void player_definir_tentativa(int n, int t) { tentativaN = n; tentativaT = t; }
void player_toast(const char *txt, unsigned ms) { (void)txt; (void)ms; }
void player_erro_fonte(void) {}
void player_erro_fonte_motivo(const char *titulo, const char *subtitulo) { (void)titulo; (void)subtitulo; }
void player_fechar_mini(void) { mini = 0; }
int video_falhou(void) { return falhou; }
int video_reconectando(void) { return reconectando; }
int video_pronto(void) { return pronto; }
int video_fonte_tocou(void) { return tocou; }
int video_dv_ativo(void) { return dv; }
unsigned video_bufferando_ms(void) { return (unsigned)bufferMs; }
double video_pos(void) { return posicao; }
double video_buffer_fim(void) { return bufferFim; }
const char *video_url_atual(void) { return ""; }
const char *video_erro_texto(void) { return "fixture playback error"; }
int stream_atual(void) { return 0; }
unsigned stream_lista_geracao(void) { return lista; }
int stream_n(void) { return 2; }
const Stream *stream_item(int i) { return i >= 0 && i < 2 ? &fontes[i] : NULL; }
int stream_proxima_sem_perda(int i) { (void)i; return proximaSemPerda; }
void stream_vod_falhou(int i) { (void)i; mortas++; }
int stream_automatico_excluir(int i) { (void)i; mortas++; return 1; }
int stream_automatico_excluir_irmas(int i) { (void)i; irmas++; return 1; }
int stream_proxima_vod(int i) { (void)i; legadas++; return 1; }
int ajustes_fonte_repor(void) { return 2; }
int debrid_servico_disponivel(int servico) { return servico == 0 || servico == 1; }
const char *i18n(const char *s) { return s; }
void marco(const char *s) { (void)s; }
uint64_t badges_bit(const char *s) { (void)s; return 0; }
int player_indice(void) { return 0; }
void player_episodio_atual(int *t, int *e) { *t = *e = 0; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
int cat_id_stream(int i, int t, int e, char *url, unsigned n) {
  (void)i; (void)t; (void)e; (void)n; url[0] = 0; return 0;
}
void stream_definir_lista(const Stream *s, int n) { (void)s; (void)n; assert(!"VOD policy must not replace list"); }
void stream_definir_atual(int i) { (void)i; assert(!"VOD consumer opens in the later UI phase"); }
int stream_automatico_disponivel(int i) { (void)i; return 1; }
Uint32 stream_idade_ms(void) { return 0; }
int stalker_resolver(const char *id, char *url, unsigned n) {
  (void)id; (void)url; (void)n; assert(!"VOD policy must not dispatch Stalker"); return 0;
}
int stream_canal_primeira_viva(int n) { (void)n; assert(!"VOD policy must not dispatch channel"); return -1; }
int stream_canal_prazo_longo(int i) { (void)i; return 0; }
int stream_automatico_canal(void) { assert(!"VOD policy must not dispatch channel"); return -1; }
int stream_primeira_boa(int n) { (void)n; legadas++; assert(!"VOD policy must bypass legacy selection"); return -1; }
int stream_primeira_boa_sessao(int n, uint64_t sessao) {
  (void)sessao; return stream_primeira_boa(n);
}
int stream_filtro_permite(const Stream *s) { (void)s; return 1; }
void stream_filtro_dados(const Stream *s, FonteFiltroDados *dados) {
  *dados = s ? s->filtro : (FonteFiltroDados){0};
}
int stream_verificar_proxima_vod(int i, int n, int *conferidas) {
  (void)i; (void)n; legadas++; *conferidas = 1; return 1;
}
int stream_verificar_proxima_vod_sessao(int i, int n, int *conferidas, uint64_t sessao) {
  (void)sessao; return stream_verificar_proxima_vod(i, n, conferidas);
}
int stream_canal_primeira_viva_sessao(int n, uint64_t sessao) {
  (void)sessao; return stream_canal_primeira_viva(n);
}
int stalker_resolver_sessao(const char *id, char *url, unsigned n, uint64_t sessao) {
  (void)sessao; return stalker_resolver(id, url, n);
}
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t geracao) { return geracao == 1; }
int perfis_sessao_travar(uint64_t geracao) { return geracao == 1; }
void perfis_sessao_destravar(void) {}
int perfis_ativo(void) { return 1; }
const char *sessao_usuario(void) { return ""; }
void debrid_contexto_capturar(DebridContexto *c) { if (c) memset(c, 0, sizeof *c); }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
unsigned fonteregra_versao(void) { return 0; }
int detail_aberto(void) { return 0; }
int detail_indice(void) { return 0; }
int detail_ep_foco(int *t, int *e) { if (t) *t = 0; if (e) *e = 0; return 0; }
int stream_lista_do_alvo(const char *id) { (void)id; return 1; }
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
int rede_url_final_tipo(const char *url, int seg, const char *const *cab,
                        char *fim, unsigned tam, int *status,
                        char *mime, unsigned mimeTam, long *corpo) {
  (void)url; (void)seg; (void)cab; (void)fim; (void)tam; (void)status;
  (void)mime; (void)mimeTam; (void)corpo; assert(!"fixture error is not a connection-probe error"); return 0;
}

// Simulated network worker asks the real policy for its next route, consumes a
// distinct link, and publishes only its private cloned session.
int stream_falha_verificar(FonteFalhaSessao *s, unsigned ger, int escolhida,
                          int (*cancelado)(void *), void *ctx,
                          int *idx, int *serv, char *url, unsigned n) {
  workerEscolhida = escolhida; *idx = *serv = -1; url[0] = 0;
  if (cancelado(ctx) || ger != lista) return FONTEFALHA_FIM;
  if (workerEsperar) return FONTEFALHA_ESPERAR;
  FonteFalhaCandidata c[2] = {{0}};
  for (int i = 0; i < 2; i++) {
    c[i].indice = i; c[i].nativa = 1; c[i].ordemAddon = 0;
    strcpy(c[i].addon, "addon-a"); c[i].identidade = i ? "hash-b:0" : "hash-a:0";
  }
  FonteFalhaRota r;
  int estado = fontefalha_proxima(s, c, 2, 1, &r);
  if (estado != FONTEFALHA_PROXIMA) return estado;
  assert(fontefalha_tentar(s, &c[r.candidata], r.servico));
  snprintf(url, n, "https://resolved.invalid/%d/%d.mp4", r.servico, r.indice);
  assert(fontefalha_url_registrar(s, url)); *idx = r.indice; *serv = r.servico;
  return estado;
}
int stream_falha_aplicar_sessao(int idx, unsigned ger, int serv, const char *url, uint64_t sessao) {
  (void)sessao; return stream_falha_aplicar(idx, ger, serv, url);
}
int stream_falha_renovar(int indice, unsigned ger, int servico,
                         int (*cancelado)(void *), void *ctx, char *url, unsigned tam) {
  (void)indice; (void)ger; (void)servico; (void)cancelado; (void)ctx; (void)tam;
  if (url) url[0] = 0;
  return 0;
}
int stream_falha_aplicar(int idx, unsigned ger, int serv, const char *url) {
  assert(ger == lista); assert(idx >= 0 && idx < 2); assert(url && *url);
  aplicacoes++; memmove(fontes[idx].url, url, strlen(url) + 1);
  fontes[idx].debridServico = serv + 1; return 1;
}
void stream_falha_identidade(const Stream *s, char *dst, unsigned tam) {
  assert(s == &fontes[0] || s == &fontes[1]);
  snprintf(dst, tam, "%s", s == &fontes[0] ? "hash-a:0" : "hash-b:0");
}

static void resetar(void) {
  assert(!worker && !fioFonteVivo);
  limparFonteVOD(); memset(&fonteJob, 0, sizeof fonteJob); limparFontePendente();
  fonteEscolhida = -2; fontePedidoGeracao = novaGeracaoFonte(); aguardandoFonte = 0;
  aberto = pronto = 1; mini = sair = carregando = falhou = playerFalhou = 0;
  reconectando = tocou = dv = bufferMs = proximaSemPerda = 0;
  posicao = bufferFim = 0.0; agora = 1001;
  paradas = retomadas = aberturas = aplicacoes = legadas = mortas = irmas = 0;
  evento = eventoRetomada = eventoParada = tentativaN = tentativaT = workerEsperar = 0;
  workerEscolhida = -1;
  memset(fontes, 0, sizeof fontes);
  for (int i = 0; i < 2; i++) { fontes[i].fileIdx = 0; strcpy(fontes[i].addonChave, "addon-a"); }
  strcpy(fontes[0].infoHash, "hash-a"); strcpy(fontes[1].infoHash, "hash-b");
  strcpy(fontes[0].url, "https://resolved.invalid/first.mp4");
  memset(&configuracao, 0, sizeof configuracao);
  configuracao.provedores = 1; configuracao.nAddons = 1;
  strcpy(configuracao.addon[0].chave, "addon-a"); configuracao.addon[0].limite = 3;
  for (int i = 0; i < 4; i++) { configuracao.ordem[i] = i; configuracao.limites[i] = 2; }
  fonteFalha = fontefalha_sessao_nova(&configuracao, 3, -1); assert(fonteFalha);
  FonteFalhaCandidata c = { .indice = 0, .nativa = 1, .identidade = "hash-a:0" };
  strcpy(c.addon, "addon-a");
  assert(fontefalha_semear(fonteFalha, &c, 0, fontes[0].url));
  fontefalha_marcar_aberta(fonteFalha);
  fonteFalhaLista = lista; fonteVODAutomatica = 1; fonteVODDesde = 1000;
  // Exhaust the legacy fallback count deliberately: the independent policy
  // still has routes, and must never call the legacy selector or blacklist.
  fonteVODTentativas = 99;
}

static void erroRetomaAntesDeParar(void) {
  resetar(); posicao = 123.5; guardarPosicaoFonteFalha(); falhou = 1;
  tentarProximaFonteVOD();
  assert(paradas == 1 && retomadas == 1 && retomada == 123.5);
  assert(eventoRetomada < eventoParada && posicao == 0.0);
  assert(aguardandoFonte == 2 && fonteJob.tipo == FJOB_FALHA && worker);
  assert(!aberturas && !mortas && !irmas && !legadas);
  assert(fontefalha_tentativas(fonteFalha) == 1);
  concluirWorker(); processarFonteJob();
  assert(!fioFonteVivo && fonteJob.falha == NULL && fonteEscolhida == 1);
  assert(aplicacoes == 1 && fontefalha_tentativas(fonteFalha) == 2);
  assert(fonteVODTentativas == 99 && !aberturas);
}
static void ultimoQuadroSobreviveAoBackendZerado(void) {
  resetar(); posicao = 234.25; guardarPosicaoFonteFalha();
  playerFalhou = 1; posicao = 0.0; guardarPosicaoFonteFalha();
  assert(fonteFalhaPos == 234.25);
  tentarProximaFonteVOD(); assert(retomadas == 1 && retomada == 234.25);
  concluirWorker(); processarFonteJob();
}
static void seekParaInicioRetomaZero(void) {
  resetar(); posicao = 100.0; guardarPosicaoFonteFalha(); assert(fonteFalhaPos == 100.0);
  posicao = 0.0; guardarPosicaoFonteFalha(); assert(fonteFalhaPos == 0.0);
  falhou = 1; tentarProximaFonteVOD();
  assert(retomadas == 1 && retomada == 0.0 && eventoRetomada < eventoParada);
  concluirWorker(); processarFonteJob();
}
static void amostrasInvalidasNaoSubstituemQuadro(void) {
  resetar(); posicao = 100.0; guardarPosicaoFonteFalha();
  const double invalidas[] = {-1.0, NAN, INFINITY};
  for (unsigned i = 0; i < sizeof invalidas / sizeof invalidas[0]; i++) {
    posicao = invalidas[i]; guardarPosicaoFonteFalha(); assert(fonteFalhaPos == 100.0);
  }
  pronto = 0; posicao = 0.0; guardarPosicaoFonteFalha(); assert(fonteFalhaPos == 100.0);
}
static void prazoDeAbertura(void) {
  resetar(); carregando = 1; pronto = 0; agora = 32001;
  tentarProximaFonteVOD(); assert(paradas == 1 && !retomadas && worker);
  concluirWorker(); processarFonteJob();
}
static void sinaisQueNaoTrocamFonte(void) {
  for (int caso = 0; caso < 6; caso++) {
    resetar();
    if (caso == 0) bufferMs = 120000;                 // mid-play stall/buffer
    if (caso == 1) { falhou = 1; reconectando = 1; } // same-URL recovery owns error
    if (caso == 2) { falhou = 1; sair = 1; }         // explicit Back/Stop
    if (caso == 3) { falhou = 1; aberto = 0; }       // player already closed
    if (caso == 4) { carregando = 1; tocou = 1; agora = 120000; } // track reload
    if (caso == 5) { carregando = 1; dv = 1; agora = 120000; }    // DV backend owns open
    tentarProximaFonteVOD();
    assert(!worker && !paradas && !retomadas && !aplicacoes);
    assert(fontefalha_tentativas(fonteFalha) == 1 && !mortas && !irmas && !legadas);
  }
}
static void respostaObsoletaNaoGastaSessaoAtual(void) {
  for (int caso = 0; caso < 4; caso++) {
    resetar(); falhou = 1; tentarProximaFonteVOD(); assert(worker);
    concluirWorker();
    assert(fontefalha_tentativas(fonteJob.falha) == 2);
    FonteFalhaSessao *viva = fonteFalha;
    if (caso == 0) novaGeracaoFonte(); // cancelled request
    if (caso == 1) lista++;           // replaced stream list
    if (caso == 2) sair = 1;          // explicit Stop/Back
    if (caso == 3) aberto = 0;        // playback closed before completion
    processarFonteJob();
    assert(!fioFonteVivo && !fonteJob.falha && !aplicacoes && !aberturas);
    assert(fonteFalha == viva);
    assert(fontefalha_tentativas(fonteFalha) == 1 && fonteVODTentativas == 99);
    if (caso == 1) assert(fonteEscolhida == -1);
  }
}
static void cancelarNaoPublicaWorker(void) {
  for (int miniAtivo = 0; miniAtivo < 2; miniAtivo++) {
    resetar(); falhou = 1; tentarProximaFonteVOD(); assert(worker);
    unsigned antes = atomic_load(&stalkerGeracao);
    aberto = 0; mini = miniAtivo; sair = 1; cancelarFonteSeSaiu();
    assert(atomic_load(&stalkerGeracao) == antes + 1);
    assert(!fonteFalha && aguardandoFonte == 0 && !fonteVODAutomatica);
    concluirWorker(); processarFonteJob();
    assert(!fonteJob.falha && !aplicacoes && !aberturas && fonteEscolhida == -1);
  }
}
static void pedidoNovoEsperaSlotSemHerdarOrcamento(void) {
  resetar(); falhou = 1; tentarProximaFonteVOD(); concluirWorker();
  assert(fontefalha_tentativas(fonteJob.falha) == 2);
  fontefalha_sessao_liberar(fonteFalha);
  fonteFalha = fontefalha_sessao_nova(&configuracao, 3, 1); assert(fonteFalha);
  fonteFalhaManual = 1;
  assert(pedirProximaFonteVOD(0));
  assert(fontePendenteTipo == FJOB_FALHA && !worker);
  processarFonteJob(); // Reject old DONE clone, then give its slot to latest request.
  assert(worker && fioFonteVivo && fontePendenteTipo == FJOB_NONE);
  assert(!aplicacoes && fonteEscolhida == -2 && fontefalha_tentativas(fonteFalha) == 0);
  concluirWorker(); processarFonteJob();
  assert(aplicacoes == 1 && fonteEscolhida == 1 && fontefalha_tentativas(fonteFalha) == 1);
  assert(!aberturas && !legadas);
}
static void listaParcialPreservaEspera(void) {
  resetar(); falhou = 1; workerEsperar = 1; tentarProximaFonteVOD();
  concluirWorker(); processarFonteJob();
  assert(fonteFalhaEsperando && fonteEscolhida == -2 && aguardandoFonte == 2);
  assert(!aplicacoes && !aberturas && fontefalha_tentativas(fonteFalha) == 1);
}
static void escolhaManualContaPrimeiroLink(void) {
  resetar();
  assert(!fontefalha_ativa());
  assert(!armarPoliticaFonteAberta(0, 0, fontes[0].url));
  assert(fontefalha_definir_provedores(1));
  assert(armarPoliticaFonteAberta(0, 0, fontes[0].url));
  assert(fonteVODAutomatica && fonteVODTentativas == 1 && fonteFalhaManual == 0);
  assert(fontefalha_tentativas(fonteFalha) == 1 && fontes[0].debridServico == 1);
  assert(aplicacoes == 1 && !paradas && !aberturas && !worker);
  // The link already opened by manual resolution is consumed once; failure
  // advances to another distinct route instead of reopening that first URL.
  falhou = 1; tentarProximaFonteVOD(); concluirWorker(); processarFonteJob();
  assert(fonteEscolhida == 1 && fontefalha_tentativas(fonteFalha) == 2);
  assert(!strcmp(fontes[0].url, "https://resolved.invalid/first.mp4"));
  // Ordinary manual selection uses the enabled policy worker too, preserving
  // the explicitly selected source as first and enabling later failover.
  resetar();
  assert(iniciarPoliticaFonte(1, 1)); assert(fontefalha_tentativas(fonteFalha) == 0);
  aguardandoFonte = 2;
  assert(pedirFonteJob(FJOB_FALHA, fontePedidoGeracao, NULL, 0) == 0);
  concluirWorker(); processarFonteJob();
  assert(workerEscolhida == 1 && fonteEscolhida == 1 && aplicacoes == 1);
  assert(fontefalha_tentativas(fonteFalha) == 1 && !aberturas && !legadas);
  assert(fontefalha_definir_provedores(0));
}
static void autoplayPostOpenRetry(void) {
  resetar(); fontefalha_sessao_liberar(fonteFalha); fonteFalha = NULL;
  fonteVODTentativas = 1; tocou = 1; posicao = 42.0; falhou = 1;
  tentarProximaFonteVOD();
  assert(aberturas == 1 && paradas == 1 && retomada == 42.0);
  assert(!mortas && !irmas && !worker && fonteVODTentativas == 1);
  // Retry fails before another frame: the original post-open status survives.
  tocou = 0; falhou = 1;
  tentarProximaFonteVOD();
  assert(irmas == 0 && mortas == 1 && worker);
  concluirWorker(); processarFonteJob();
  assert(fonteEscolhida == 1 && fonteVODAutomatica);
}
int main(int argc, char **argv) {
  assert(argc == 2); fontefalha_dir(argv[1]); assert(!fontefalha_ativa());
  autoplayPostOpenRetry();
  erroRetomaAntesDeParar(); ultimoQuadroSobreviveAoBackendZerado(); seekParaInicioRetomaZero();
  amostrasInvalidasNaoSubstituemQuadro(); prazoDeAbertura();
  sinaisQueNaoTrocamFonte(); respostaObsoletaNaoGastaSessaoAtual();
  cancelarNaoPublicaWorker(); pedidoNovoEsperaSlotSemHerdarOrcamento();
  listaParcialPreservaEspera(); escolhaManualContaPrimeiroLink();
  limparFonteVOD(); puts("fontefalha playback: trigger, resume and stale-worker regressions passed");
  return 0;
}
