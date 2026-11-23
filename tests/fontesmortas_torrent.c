// Ciclo real do job manual: baixar -> resolver -> primeiro quadro -> limpar.
#define SDL_MAIN_HANDLED
#include "stub_perfisessao.h"
#include "../src/app.c"
#include "stub_m3u.h"
#include "fontesmortas.h"
#include <assert.h>

static Stream fontes[2];
#define fonte fontes[0]
static int atual, resolvidos, selecoes, cenario;
static char *disco;
static Uint32 ticks = 1000;
static int pronto, abriu, erros, falhou, esperando;
static CatItem titulo;
char *dados_ler(const char *nome) { (void)nome; return disco ? strdup(disco) : NULL; }
int dados_gravar(const char *nome, const char *s) {
  (void)nome; free(disco); disco = strdup(s); return 1;
}
Uint32 SDL_GetTicks(void) { return ticks; }
int player_aberto(void) { return 1; }
int player_quer_sair(void) { return 0; }
const char *player_id_canal(void) { return ""; }
int player_indice(void) { return -1; }
int player_fonte_falhou(void) { return falhou; }
int player_carregando(void) { return !pronto; }
void player_definir_fonte(const char *u) { assert(!strcmp(u, "https://video.invalid/resolvido")); abriu++; }
void player_erro_fonte(void) { erros++; }
void player_erro_fonte_motivo(const char *a, const char *b) { (void)a; (void)b; erros++; }
int video_pronto(void) { return pronto; }
static double posicao;
double video_pos(void) { return pronto ? posicao : 0.0; }
int video_falhou(void) { return falhou; }
double video_duracao(void) { return pronto ? 5400 : 0; }
int player_duracao_midia(double *seg) {
  if (!pronto) return 0;
  if (seg) *seg = video_duracao();
  return 1;
}
void player_voltar_a_esperar(void) { esperando++; }
void player_definir_tentativa(int i, int n) { (void)i; (void)n; }
void player_episodio_atual(int *t, int *e) { *t = *e = 0; }
int cat_id_stream(int i, int t, int e, char *s, unsigned n) {
  (void)i; (void)t; (void)e; snprintf(s, n, "%s", titulo.imdb); return 1;
}
const char *sessao_usuario(void) { return "conta"; }
int perfis_ativo(void) { return 1; }
Uint32 stream_idade_ms(void) { return 0; }
int stream_lista_do_alvo(const char *s) { (void)s; return 1; }
void stream_definir_alvo(const char *s) { (void)s; }
void addons_buscar(const char *s, const char *t) { (void)s; (void)t; }
void marco(const char *s) { (void)s; }
int stream_url_serve(const char *u, const char *c) { (void)u; (void)c; return 0; }
int rede_url_final_tipo(const char *u, int s, const char *const *c, char *d,
                        unsigned t, int *st, char *m, unsigned mt, long *b) {
  (void)u; (void)s; (void)c; (void)d; (void)t; (void)st; (void)m; (void)mt; (void)b;
  return 0;
}
char *rede_colocar_st(const char *u, int s, const char *const *c, const char *corpo, int *st) {
  (void)u; (void)s; (void)c; (void)corpo; *st = 0; return NULL;
}
void stream_falha_identidade(const Stream *s, char *dst, unsigned tam) {
  (void)s; if (tam) dst[0] = 0;
}
int stream_falha_aplicar(int indice, unsigned lista, int servico, const char *url) {
  (void)indice; (void)lista; (void)servico; (void)url; return 1;
}
const Stream *stream_item(int i) { return i >= 0 && i < 2 ? &fontes[i] : NULL; }
const char *video_url_atual(void) { return ""; }
const char *video_erro_texto(void) { return ""; }
int stream_atual(void) { return atual; }
void stream_definir_atual(int i) { atual = i; selecoes++; }
unsigned stream_lista_geracao(void) { return 1; }
void stream_vod_falhou(int i) { assert(i == (cenario ? 1 : 0)); fontesmortas_marcar(&fontes[i]); }
int stream_automatico_excluir(int i) { assert(i == 0); return 1; }
void stream_vod_tocou(int i) { assert(i == 0); fontesmortas_limpar(&fonte); }
int stream_resolver_escolhida(int i, unsigned g, char *u, unsigned n,
                              char *s, unsigned ns, int *pct) {
  assert(cenario && i == 1 && g == 1);
  assert(!strcmp(fontes[i].infoHash, "11223344") && fontes[i].fileIdx == 7);
  resolvidos++;
  snprintf(u, n, "https://video.invalid/resolvido");
  snprintf(s, ns, "P2P"); *pct = 100;
  return 1;
}
int p2p_ultimo_erro(void) { return 0; }
void p2pmotor_arquivo_atual(uint64_t *t, uint64_t *m, uint64_t *l) { *t = *m = *l = 0; }
const CatItem *cat_item(int i) { (void)i; return titulo.imdb[0] ? &titulo : NULL; }
void ilhasinais_debrid_baixando(const char *s, const char *t) { (void)s; (void)t; }
const char *i18n(const char *s) { return s; }

int addonguia_aberto(void) { return 0; }
void addonguia_abrir(int a, int b, const char *c) { (void)a; (void)b; (void)c; }
void addonguia_fechar(void) {}
void addonguia_progresso(const char *a, int b) { (void)a; (void)b; }
AddonModo addonmodo_get(uint64_t id) { (void)id; return AM_P2P; }
int cat_indice_reabrir(int i, const CatItem *c) { (void)i; (void)c; return 0; }
int detail_indice(void) { return 0; }
int detail_ep_foco(int *t, int *e) { *t = *e = 0; return 0; }
void diagnostico_cancelar_vazao(void) {}
const char *extras_ficha_classificacao(void) { return ""; }
int fontepref_guardar(const char *id, const Stream *s) { (void)id; (void)s; return 1; }
void fontepref_id_base(const char *id, char *dst, unsigned n) { snprintf(dst, n, "%s", id); }
void glem_aviso_curto(const char *s) { (void)s; }
void juntos_escolher_titulo(const char *a, int b, int c, const char *d, const char *e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
void juntos_fonte_escolhida(void) {}
const JsMotor *juntos_motor(void) { return NULL; }
int juntos_no_player(void) { return 0; }
int juntos_precarregando(void) { return 0; }
void juntos_retomar_player(int i) { (void)i; }
int juntosui_escolhendo_filme(void) { return 0; }
void juntosui_sala_abriu(void) {}
int addons_base_por_id_copiar(const char *id, char *base, unsigned tam) {
  (void)id; if (tam) base[0] = 0; return 0;
}
void addons_definir_origem(const char *base) { (void)base; }
void servidores_dica(const char *a, const char *b, int c) { (void)a;(void)b;(void)c; }
int srvidx_ano_de(const char *meta) { (void)meta; return 0; }
int stream_falha_semear_manual(FonteFalhaSessao *s, int i, unsigned g, const char *u) {
  (void)s;(void)i;(void)g;(void)u; return 0;
}
int xtream_url_limitada(const char *u) { (void)u; return 0; }
int p2p_ativo(void) { return 1; }
void p2p_cancelar(void) {}
int p2p_usa_motor(void) { return 1; }
void p2pmotor_cancelar(void) {}
void p2pmotor_parar_fundo(void) {}
void partes_arquivo_limpar(void) {}
void partes_link_cancelar(void) {}
void partes_ponto_esquecer(const char *a, const char *b, int c) { (void)a;(void)b;(void)c; }
const PartesLista *partes_sessao_lista(void) { return NULL; }
void partes_sessao_parar(const char *s) { (void)s; }
int perfil_infantil_ativo(void) { return 0; }
int perfil_regra_classif(const char *s) { (void)s; return 1; }
int perfil_regra_tempo(float s) { (void)s; return 0; }
void player_abrir(int i, const char *u) { (void)i; (void)u; }
void player_definir_episodio(int t, int e) { (void)t;(void)e; }
void player_definir_retomada_fonte(double s) { (void)s; }
void player_do_inicio(void) {}
void player_encerrar(void) {}
void player_encerrar_fonte(void) {}
void player_toast_ex(const char *s, unsigned m, const char *i, int a) { (void)s;(void)m;(void)i;(void)a; }
int stream_e_mp4(const Stream *s) { (void)s; return 0; }
int stream_ordem_addon(int i) { return i; }
void stream_preferir(int i) { if (cenario) assert(i == 1); }
int travaui_abrir_espera(int i, int t, int e) { (void)i;(void)t;(void)e; return 0; }
void video_definir_cabecalhos(const char *s) { (void)s; }
void video_definir_dv(int i) { (void)i; }
void video_definir_mp4(int i) { (void)i; }

static void *concluido(void *u) { return u; }
static void concluir(int resultado) {
  torrentJob.indice = 0; torrentJob.geracaoLista = 1;
  torrentJob.sessao = torrentSessao; torrentJob.sessaoPerfil = perfis_sessao_geracao();
  torrentJob.resultado = resultado;
  strcpy(torrentJob.url, "https://video.invalid/resolvido");
  strcpy(torrentJob.servico, "TorBox"); torrentJob.pct = 50;
  atomic_store(&torrentJob.estado, 2);
  assert(!pthread_create(&fioTorrent, NULL, concluido, NULL));
  fioTorrentVivo = 1;
  processarTorrentJob();
}
static void retomar(void) {
  Stream renovada;
  strcpy(titulo.imdb, "tt205"); strcpy(titulo.tipo, "movie");
  strcpy(fonte.provedor, "Addon");
  strcpy(fonte.url, "https://video.invalid/filme?token=velho");
  renovada = fonte;
  strcpy(renovada.url, "https://video.invalid/filme?token=novo");
  fontevolta_guardar(titulo.imdb, "conta", 1, &fonte, 0, ticks);
  fontevolta_conferir(fonte.url, "");
  for (int i = 0; i < 2000 && fontevolta_conferencia() == FV_CONFERINDO; i++) usleep(1000);
  assert(fontevolta_conferencia() == FV_FALHOU);
  voltaAtiva = 1; voltaDesde = ticks;
  vigiarFonteGuardada();
  assert(!voltaAtiva && aguardandoFonte == 1 && esperando == 1);
  assert(fontesmortas_idade(&renovada) < 0 && "sonda isolada bloqueou o token renovado");
  // Erro de abertura e prazo vencido continuam provando a falha do player.
  for (int erro = 0; erro <= 1; erro++) {
    aguardandoFonte = 0; falhou = erro;
    fontevolta_guardar(titulo.imdb, "conta", 1, &fonte, 0, ticks);
    voltaAtiva = 1; voltaDesde = erro ? ticks : ticks - FONTEVOLTA_PRAZO_MS - 1;
    vigiarFonteGuardada();
    assert(fontesmortas_idade(&renovada) >= 0);
    fontesmortas_limpar(&renovada);
  }
  puts("fontesmortas: PASS (Retomar, sonda isolada, token renovado, erro e prazo)");
}
static void escolhida_real(int prazo) {
  cenario = 1;
  alarm(10); /* Bound a broken worker without timing the expected completion. */
  strcpy(fontes[0].provedor, "Torrentio"); strcpy(fontes[1].provedor, "Torrentio");
  strcpy(fontes[0].infoHash, "aabbccdd"); fontes[0].fileIdx = 2;
  strcpy(fontes[1].infoHash, "11223344"); fontes[1].fileIdx = 7;
  tocarFonteEscolhida(1, 1);
  assert(fioTorrentVivo);
  while (atomic_load_explicit(&torrentJob.estado, memory_order_acquire) != 2) sched_yield();
  processarTorrentJob();
  assert(resolvidos == 1 && selecoes == 1 && atual == 1 && abriu == 1);
  assert(fonteManualDesde && !fonteVODAutomatica);
  if (prazo) ticks += FONTE_MANUAL_PRAZO_MS + 1;
  else falhou = 1;
  vigiarAberturaManual();
  assert(!fonteManualDesde && fontesmortas_idade(&fontes[1]) >= 0);
  assert(fontesmortas_idade(&fontes[0]) < 0);
  assert(resolvidos == 1 && selecoes == 1 && atual == 1 && abriu == 1);
  assert(erros == prazo);
  alarm(0);
  puts(prazo ? "manual P2P: PASS (watchdog)" : "manual P2P: PASS (failure)");
}
int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "escolhida")) { escolhida_real(argc > 2); free(disco); return 0; }
  if (argc > 1 && !strcmp(argv[1], "retomar")) { retomar(); free(disco); return 0; }
  strcpy(fonte.provedor, "Torrentio"); strcpy(fonte.infoHash, "aabbccdd"); fonte.fileIdx = 2;
  concluir(DEBRID_BAIXANDO);
  assert(erros == 1 && fontesmortas_idade(&fonte) >= 0);
  ticks += 120000; // Alguns minutos depois, a pessoa escolhe novamente.
  concluir(1);
  assert(abriu == 1 && fontesmortas_idade(&fonte) >= 0); // Resolver nao prova reproducao.
  assert(fonteManualDesde == ticks && "torrent manual sem vigia do primeiro quadro");
  vigiarAberturaManual();
  assert(fontesmortas_idade(&fonte) >= 0);
  pronto = 1; posicao = 1.0;
  vigiarAberturaManual();
  assert(!fonteManualDesde && fontesmortas_idade(&fonte) >= 0 && "1 s de quadro nao prova a fonte");
  posicao = 3.5; // Dois segundos reais de progresso limpam a marca.
  vigiarAberturaManual();
  assert(fontesmortas_idade(&fonte) < 0);
  fontesmortas_iniciar(); assert(fontesmortas_idade(&fonte) < 0);
  free(disco);
  puts("fontesmortas: PASS (torrent baixando, tentativa manual, primeiro quadro, disco)");
}
