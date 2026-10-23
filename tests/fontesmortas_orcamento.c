// Worker, consumidor de app.c e sondas de streams.c reais; sem rede nem telas.
#define SDL_MAIN_HANDLED
#define stream_primeira_boa teste_stream_primeira_boa
#define stream_canal_primeira_viva teste_stream_canal_primeira_viva
#define stream_folha_abrir teste_stream_folha_abrir
#define stream_folha_nome teste_stream_folha_nome
#define stream_folha_canal teste_stream_folha_canal
#define stream_folha_item teste_stream_folha_item
#include "../src/app.c"
#include "stub_m3u.h"
#include <assert.h>
#include <unistd.h>
#include "consumir-fonte.h"

void prepararOrcamento(int falhas);
int totalSondas(void);
int ajustes_fonte_repor(void) { return 2; }
int ajustes_dolby_atmos(void) { return 0; }
int ajustes_dolby_vision(void) { return 0; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_fonte_hdr(void) { return 0; }
int ajustes_fonte_manual(void) { return 0; }
int ajustes_fonte_prioridade(void) { return 0; }
const char *ajustes_qualidade(void) { return "Automatica"; }
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
int badges_fonte_hdr_marca(uint64_t m) { (void)m; return 0; }
uint64_t streamfit_agora_ms(void) { return 0; }
void streamfit_foto(StreamfitFoto *s, uint64_t ms) { (void)ms; memset(s, 0, sizeof *s); }
StreamfitClasse streamfit_classificar(const StreamfitFoto *f, const char *u,
    uint64_t b, double d, StreamfitResultado *s) {
  (void)f; (void)u; (void)b; (void)d; (void)s; return 0;
}
int ajustes_fonte_primeira(void) { return 1; }
int ajustes_fonte_idioma_audio(void) { return 0; }
int stream_primeira_boa(int n) { (void)n; assert(!"job inicial fora deste teste"); return -1; }
int stream_canal_primeira_viva(int n) { (void)n; return -1; }
void player_episodio_atual(int *t, int *e) { *t = *e = 0; }
int cat_id_stream(int i, int t, int e, char *s, unsigned n) { (void)i; (void)t; (void)e; (void)s; (void)n; return 0; }
void player_fechar_mini(void) { }
int player_aberto(void) { return 1; }
int player_mini_ativo(void) { return 0; }
int player_quer_sair(void) { return 0; }
const char *player_id_canal(void) { return ""; }
void player_definir_fonte(const char *u) { (void)u; }
void player_definir_retomada_fonte(double seconds) { (void)seconds; }
int addons_consulta_atual(const AddonsConsulta *query) { (void)query; return 1; }
void addons_consulta_liberar(AddonsConsulta *query) { (void)query; }
void player_definir_tentativa(int i, int n) { (void)i; (void)n; }
void video_definir_dv(int d) { (void)d; }
void video_definir_cabecalhos(const char *c) { (void)c; }
void video_definir_mp4(int m) { (void)m; }
void marco(const char *s) { (void)s; }
const char *i18n(const char *s) { return s; }
int addons_busca_parcial(void) { return 0; }
int debrid_recusa(char *s, unsigned n) { (void)s; (void)n; return 0; }
int debrid_sem_plano_novo(void) { return 0; }
int debrid_sem_plano(void) { return 0; }
int debrid_fora_de_cache(void) { return 0; }
const char *debrid_sem_plano_frase(int m) { (void)m; return ""; }
void player_toast_ex(const char *s, Uint32 ms, const char *i, int t) { (void)s; (void)ms; (void)i; (void)t; }
void player_erro_fonte(void) { }
void player_erro_fonte_motivo(const char *s, const char *d) { (void)s; (void)d; }
int addons_motivo_vazio(char *s, unsigned n) { snprintf(s, n, "vazio"); return 1; }
int ajustes_livetv_proxy(void) { return 0; }
int proxyts_disponivel(void) { return 0; }
int proxyts_candidata(const char *u, int m) { (void)u; (void)m; return 0; }
void proxyts_definir_cabecalhos(const char *s) { (void)s; }
int xtream_e_id(const char *s) { (void)s; return 0; }
int stalker_e_id(const char *s) { (void)s; return 0; }
int stalker_resolver(const char *s, char *u, unsigned n) { (void)s; (void)u; (void)n; return 0; }
int player_indice(void) { return -1; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
int detail_indice(void) { return -1; }
void player_voltar_a_esperar(void) { }
void stream_folha_abrir(void) { assert(!"nao abrir a folha"); }
void stream_folha_nome(const char *s) { (void)s; }
void stream_folha_canal(int c) { (void)c; }
void stream_folha_item(int i) { (void)i; }
int stream_falha_verificar(FonteFalhaSessao *sessao, unsigned lista, int escolhida,
                          int (*cancelado)(void *), void *ctx,
                          int *indice, int *servico, char *url, unsigned tam) {
  (void)sessao; (void)lista; (void)escolhida; (void)cancelado; (void)ctx;
  (void)indice; (void)servico; (void)url; (void)tam;
  assert(!"politica desligada nao pode substituir o worker legado");
  return FONTEFALHA_FIM;
}
int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_regex_modo(void) { return 0; }
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_buffer_preset(void) { return 0; }
int ajustes_cache_seek_mb(void) { return 0; }
int ajustes_fonte_conferir_varias(void) { return 0; }
int ajustes_fonte_ordem_uso(void) { return 0; }
int ajustes_fonte_precarregar(void) { return 0; }
int ajustes_fonte_preparar(void) { return 0; }
int ajustes_fonte_tocar_conferindo(void) { return 0; }
int ajustes_livetv_resolucao(void) { return 0; }
const char *ajustes_p2p_url(void) { return ""; }
int detail_aberto(void) { return 0; }
int detail_ep_foco(int *t, int *e) { (void)t; (void)e; return 0; }
int stalker_resolver_sessao(const char *s, char *u, unsigned n, uint64_t sessao) {
  (void)s; (void)u; (void)n; (void)sessao; return 0;
}
const char *sessao_usuario(void) { return "conta"; }
int perfis_ativo(void) { return 1; }
void debrid_contexto_capturar(DebridContexto *c) { if (c) memset(c, 0, sizeof *c); }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
int debrid_servico_disponivel(int s) { (void)s; return 0; }
int debrid_resolver_servico_cache(int s, const char *h, int f, char *u,
                                  unsigned n, int *fora) {
  (void)s; (void)h; (void)f; (void)u; (void)n; (void)fora;
  return 0;
}
char *rede_baixar_medido_controle(const char *u, int s, const char *const *c,
                                  const RedeControle *ct, RedeMedida *m) {
  (void)u; (void)s; (void)c; (void)ct; if (m) memset(m, 0, sizeof *m); return NULL;
}
unsigned fonteregra_versao(void) { return 0; }
int fonteregra_no_escopo(const FonteRegraCfg *c, int plugin) { (void)c; (void)plugin; return 1; }
int fonteregra_ordem_n(void) { return 0; }
int fonteregra_ordem_rank(const char *nome) { (void)nome; return FR_ORDEM_SEM; }
int fonteregra_regex_estado(void) { return 0; }
int fonteregra_ativa(const FonteRegraCfg *c) { (void)c; return 0; }
int fonteregra_n(int plugin) { (void)plugin; return 0; }
int fonteauto_primeira(const int *fila, int nf, int (*v)(int, void *),
                       void (*f)(int, void *), void *u, int *tocadas) {
  for (int q = 0; q < nf; q++) {
    if (v(fila[q], u)) { if (tocadas) *tocadas = q + 1; return fila[q]; }
    if (f) f(fila[q], u);
  }
  if (tocadas) *tocadas = nf; return -1;
}
int fonteauto_tentativas(int modo, int teto) { (void)modo; return teto; }
int fonteauto_fila_o(int modo, int total, int pref, const long *pontos,
                     const unsigned char *acima, const unsigned char *excl,
                     const signed char *grupo, const int *rank, int ordemUso,
                     int limite, int *fila) {
  (void)pontos; (void)acima; (void)grupo; (void)rank; (void)ordemUso;
  int nf = 0;
  if (pref >= 0 && pref < total && !excl[pref]) fila[nf++] = pref;
  for (int i = 0; i < total && nf < limite; i++)
    if (i != pref && !excl[i]) fila[nf++] = i;
  return modo == 1 && nf > 1 ? 1 : nf;
}
int fonteparalela_prefixo(const int *fila, int nf, int max,
                          int (*pronta)(int, void *), void *u) {
  int k = 0;
  while (k < nf && k < max && pronta(fila[k], u)) k++;
  return k;
}
int fonteparalela_soltando(const int *fila, int nf, int kk,
                           int (*v)(int, void *), void (*f)(int, void *),
                           void *u, int *tocadas, int prazoMs,
                           void (*soltar)(void *)) {
  (void)kk; (void)prazoMs;
  int r = fonteauto_primeira(fila, nf, v, f, u, tocadas);
  if (soltar) soltar(u);
  return r;
}

int main(int argc, char **argv) {
  (void)argv;
  int sucesso = argc > 1;
  assert(!fontefalha_ativa());
  prepararOrcamento(sucesso ? 2 : 3);
  // Primeira sonda falha; app.c tem duas reposicoes no mesmo orcamento.
  fonteEscolhida = stream_verificar_proxima_vod(-1, 1, NULL);
  aguardandoFonte = 2;
  consumirFonte();
  for (int rodada = 0; rodada < 3 && fioFonteVivo; rodada++) {
    for (int i = 0; i < 2000 && atomic_load(&fonteJob.estado) != FJOB_DONE; i++) usleep(1000);
    assert(atomic_load(&fonteJob.estado) == FJOB_DONE);
    processarFonteJob();
    consumirFonte();
  }
  assert(totalSondas() == 3 && "limite total tres permitiu a quarta sonda");
  if (sucesso) {
    assert(fonteEscolhida == 2 && fonteVODTentativas == 3);
  } else assert(fonteEscolhida == -1 && "quarta candidata escolhida fora do orcamento");
  assert(stream_automatico_disponivel(3));
  stream_definir_lista(NULL, 0);
  puts("fontesmortas: PASS (worker e consumidor reais, tres sondas, quarta intocada)");
}
