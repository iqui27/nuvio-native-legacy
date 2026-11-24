// TESTE DE DESPACHO REAL DO ROTEADOR (src/home.c + src/faroldica.c + trechos
// reais de src/app.c: abrirTitulo, home_pediu_abrir e home_pediu_tocar).
// O runner tests/farol_dica.sh injeta os blocos literais de src/app.c nos
// marcadores APP_ABRIR_TITULO e APP_HOME_DISPATCH abaixo.
// Prova de ponta a ponta (sem arte, fora do catalogo):
//   1. Serie parada (progresso T2E4 + Diario nome, sem arte) -> OK abrir ->
//      app.c chama detail_abrir() com tipo="series", T2E4, arte=NULL.
//   2. Serie fora do catalogo (Diario T3E7, sem arte) -> OK tocar ->
//      app.c chama detail_abrir() + desc_episodios() com cwTocarT=3, cwTocarE=7.
//   3. Filme fora do catalogo (Diario nome, sem arte) -> OK tocar ->
//      app.c chama detail_abrir() + detail_pedir_reproduzir().
//   4. Serie fora do catalogo SEM episodio (Diario T0E0, sem arte) -> OK tocar ->
//      app.c chama detail_abrir() (pagina abre) sem detail_pedir_reproduzir().
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "avisodisp.h"
#include "diario.h"
#include "progresso.h"
#include "salvos.h"
#include "juntos.h"
#include "partes.h"
#include "detail.h"
#include "deeplink.h"
#include "fontepref.h"
#include "ilha.h"
#include "ilhacart.h"

static Uint32 fakeTicks = 1000u;
#include "home_real.inc"
Uint32 SDL_GetTicks(void) { return fakeTicks; }

// ---- dubles minimos da UI/ambiente + espias do roteador de detalhe ----
static int stubPerfil = 1;
static uint64_t stubSessao = 1;
static long long stubAgoraS = 1767225600LL + 350LL * 86400LL + 36000LL;
static unsigned stubCenaGer = 1u;

static int espDetailAbrir;
static int stubDetailAberto;
static int stubDetailSaindo;
static unsigned stubDetailCiclo;
static HomeItem espDetailItem;
static char espDetailImdb[64];
static int espDetailReproduzir;
static int espDetailPedReproduzir;
static int espDetailPedReproduzirExterno;
static int espDetailPedAbrir = -1;
static int espDetailFocoT, espDetailFocoE;
static int stubRetained, espRetainedReopened, stubValidIsland = 1;
static int espDescartarRetido;
static int espDescEpisodios;
static int espDetailIdx = -1;
static int espPlayerAbrir;
static int stubPlayerAberto;
static int espPlayerIdx = -1;
static int espPlayerT, espPlayerE;
static int espPlayerEncerrar;
static int espAddonsBuscar;
static char espAddonsAlvo[64];
static char espStreamAlvo[64];
static char espStreamListaAlvo[64];
static int espLimparFonteVod;

static long long relogioFake(void) { return stubAgoraS; }
int perfis_ativo(void) { return stubPerfil; }
uint64_t perfis_sessao_geracao(void) { return stubSessao; }
int perfis_sessao_atual(uint64_t s) { return s == stubSessao; }
int perfis_sessao_travar(uint64_t s) { return s == stubSessao; }
void perfis_sessao_destravar(void) {}
int perfil_infantil_ativo(void) { return 0; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
int mapa_recomendacoes_tmdb(const char *id, long tmdb, int serie,
                            const char *chave, const char *idioma,
                            MapaRecomendacoes *out) {
  (void)id; (void)tmdb; (void)serie; (void)chave; (void)idioma;
  if (out) memset(out, 0, sizeof *out);
  return 0;
}
int seguro_perfil_ativo(void) { return 0; }
int farolsinal_recomendar(void) { return 1; }
int farolsinal_modelo(FarolGosto *out) {
  if (out) { memset(out, 0, sizeof *out); out->titulos = 12; }
  return 1;
}
void farolsinal_origem(const CatItem *ci, int origem) { (void)ci; (void)origem; }
int farol_fileira(const char *ch) {
  return ch && (!strcmp(ch, "farol_gosto1") || !strcmp(ch, "farol_gosto2"));
}
unsigned farol_cena_geracao(void) { return stubCenaGer; }
const char *farol_motivo(const char *ch, int col, const char *imdb) {
  (void)ch; (void)col; (void)imdb; return NULL;
}

int ajustes_animacoes_reduzidas(void) { return 0; }
void ajustes_acento(float *r, float *g, float *b) { *r = 0.92f; *g = 0.25f; *b = 0.48f; }
int ajustes_vidro(void) { return 1; }
int ajustes_idioma(void) { return 0; }
int ajustes_home_layout(void) { return 0; }
int ajustes_hero_ligado(void) { return 1; }
int ajustes_cw_ligado(void) { return 1; }
int ajustes_cw_estilo(void) { return 0; }
int ajustes_cw_ordem(void) { return 0; }
int ajustes_cw_ok_toca(void) { return 1; }
int ajustes_cw_minimo(void) { return 5; }
int ajustes_cw_concluido(void) { return 90; }
int ajustes_posteres_deitados(void) { return 0; }
int ajustes_rotulos_poster(void) { return 0; }
int ajustes_ocultar_filmes_vistos(void) { return 0; }
int ajustes_ocultar_nao_lancados(void) { return 0; }
int ajustes_bib_home(void) { return 0; }
int ajustes_fonte_biblioteca(void) { return 0; }
int ajustes_social(void) { return 0; }
int ajustes_relogio_ligado(void) { return 1; }
int ajustes_menu_topo(void) { return 0; }
int ajustes_borda_foco(void) { return 1; }
int ajustes_itens_fileira(void) { return 12; }
int ajustes_expandir_poster(void) { return 0; }
float ajustes_expandir_poster_atraso(void) { return 2.0f; }
int ajustes_fundo_din(void) { return 0; }
void ajustes_fundo_din_cor(float c[3]) { c[0]=c[1]=c[2]=0.1f; }
int ajustes_amigos_estilo(void) { return 0; }
int ajustes_trailer_auto(void) { return 0; }
int ajustes_poster_escala_pct(void) { return 100; }
int ajustes_hero_fonte(void) { return 0; }
int ajustes_hero_diferente(void) { return 0; }
int ajustes_progresso_poster(void) { return 1; }
int ajustes_tinta_foco(void) { return 255; }
int ajustes_tinta_foco2(void) { return 200; }
int ajustes_sinopse_tamanho(void) { return 100; }
int ajustes_sufixo_tipo(void) { return 0; }
int ajustes_titulo_fil(void) { return 0; }
int ajustes_qualidade_posters(void) { return 0; }
int ajustes_poster_provider(void) { return 0; }
const char *ajustes_rpdb_chave(void) { return ""; }
const char *ajustes_topposters_chave(void) { return ""; }
int ajustes_trakt_notas(void) { return 0; }
int ajustes_imdb_notas(void) { return 0; }
int ajustes_interface_mobile(void) { return 0; }
int ajustes_tv_home(void) { return 1; }
int ajustes_largura_poster_dp(void) { return 126; }
int ajustes_trailer_hero(void) { return 0; }
unsigned ajustes_trailer_hero_espera_ms(void) { return 5000u; }
int ajustes_hero_arte_diferente(void) { return 0; }
int ajustes_cw_thumb_episodio(void) { return 0; }
float ajustes_espaco_fileiras(void) { return 28.0f; }
float ajustes_espaco_titulos(void) { return 24.0f; }
int ajustes_palco_fundo(void) { return 0; }
float ajustes_conteudo_x(void) { return 48.0f; }
float ajustes_hero_escala_logo(void) { return 1.0f; }
int ajustes_hero_lista(void) { return 10; }
float gfx_escala_txt_entrar(void) { return 1.0f; }
void gfx_escala_sair(float anterior) { (void)anterior; }
GLuint tex_obter_logo_larg_qualquer(const char *c, float l) { (void)c; (void)l; return 0; }
void marco(const char *nome) { (void)nome; }
const char *i18n(const char *s) { return s ? s : ""; }
int cacavisto_filme(const CatItem *ci) { (void)ci; return 0; }
int addons_n(void) { return 1; }
unsigned addons_versao(void) { return 0; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }
int contalib_tem_conta(void) { return 0; }
int trakt_ativo(void) { return 0; }
int simkl_ativo(void) { return 0; }
int conta_ativa(void) { return 0; }
const char *sessao_usuario(void) { return "teste"; }
int desc_catalogos_fora(void) { return 0; }
const char *desc_genero_pt(const char *g) { return g ? g : ""; }
void desc_ocultar_vistos_atualizar(void) {}
void desc_sinopse_hero(const int *l, int n) { (void)l; (void)n; }
void sync_proteger_ajustes_locais(void) {}
void biblioteca_alternar_lista(int i) { (void)i; }
int prox_seguinte(const CatItem *ci, const CatEp *eps, int n, int limiarPct,
                  long long agoraMs, ProxSugestao *saida) {
  (void)limiarPct; (void)agoraMs;
  if (!ci || !eps || n <= 0 || !saida) return 0;
  for (int i = 0; i < n; i++) {
    if (eps[i].temporada == ci->temporada && eps[i].episodio == ci->episodio + 1) {
      memset(saida, 0, sizeof *saida);
      saida->temporada = eps[i].temporada;
      saida->episodio = eps[i].episodio;
      return 1;
    }
  }
  return 0;
}
int menu_ocupado(Uint32 a) { (void)a; return 0; }
void menu_fechar(void) {}
int detail_aberto(void) { return stubDetailAberto; }
unsigned detail_ciclo(void) { return (stubDetailAberto && !stubDetailSaindo) ? stubDetailCiclo : 0u; }
float detail_progresso(void) { return 0.0f; }
int player_aberto(void) { return stubPlayerAberto; }
int trailer_aberto(void) { return 0; }
int trailer_dono(void) { return 0; }
void trailer_fechar(void) {}
int ctx_aberto(void) { return 0; }
void ctx_abrir(int i) { (void)i; }
void ctx_abrir_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_abrir_filmenu(const char *c, const char *t, const char *p, int v) {
  (void)c; (void)t; (void)p; (void)v;
}
void ctx_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_dispensar_retomar(int o) { (void)o; }
void vertudo_abrir(const char *a, const char *b, const char *c, const char *d) {
  (void)a; (void)b; (void)c; (void)d;
}
void vertudo_colecao(const ColFolder *f) { (void)f; }
int homevazio_ativo(int n) { return n <= 0; }
int homevazio_evento(const SDL_Event *e) { (void)e; return 0; }
void cachearte_limpar_referencias_grupo(int g) { (void)g; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int g, const char *u, float l, int a, int b) {
  (void)g; (void)u; (void)l; (void)a; (void)b;
}
const char *tex_arquivo(const char *u) { return u; }
GLuint tex_obter_hero_quente(const char *u) { (void)u; return 0; }
unsigned long tex_hash_public(const char *u) { (void)u; return 0; }
GLuint tex_obter_logo_larg(const char *u, float w) { (void)u; (void)w; return 0; }
float tex_aspecto(const char *u) { (void)u; return 1.0f; }
int tex_falhou(const char *u) { (void)u; return 0; }
int tex_largura_fonte(const char *u) { (void)u; return 0; }
int ponteiro_ok_longo(void) { return 0; }
float gfx_escala(void) { return 1.0f; }
float gfx_escala_txt(void) { return 1.0f; }
float gfx_escala_tela_txt(void) { return 1.0f; }
float gfx_poster_k(void) { return 1.0f; }
void gfx_mini_liberar(GfxMini *m) { if (m) memset(m, 0, sizeof *m); }
void gfx_cor(GfxRect r, float ra, float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_anel(GfxRect r, float ra, float e, float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)e; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro_ou_solido(GfxRect r, float ra, float d, float a,
                         float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)d; (void)a; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) {
  (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; (void)w;
  TxtLinha l = { 0, 40, 20, 0, 0 }; return l;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b,
                      float x, float y, float w, float ld, float a, int ml) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml;
  return 40.0f;
}
void colfileiras_sincronizar(void) {}
int canal_e_chave(const char *c) { (void)c; return 0; }

// Espias usados diretamente pelo bloco real de src/app.c:
void player_descartar_retido(void) { espDescartarRetido++; }
void detail_abrir(const HomeItem *it) {
  espDetailAbrir++;
  stubDetailAberto = 1;
  stubDetailSaindo = 0;
  if (!++stubDetailCiclo) stubDetailCiclo = 1;
  espDetailPedReproduzir = 0;
  espDetailPedReproduzirExterno = 0;
  espDetailPedAbrir = -1;
  espDetailFocoT = espDetailFocoE = 0;
  espDetailImdb[0] = 0;
  if (it) {
    const CatItem *ci = cat_item(it->indice);
    espDetailItem = *it;
    espDetailIdx = it->indice;
    if (ci) snprintf(espDetailImdb, sizeof espDetailImdb, "%s", ci->imdb);
  }
}
static void detail_voltar_sair(void) {
  if (!stubDetailAberto || stubDetailSaindo) return;
  stubDetailSaindo = 1;
  espDetailPedReproduzir = 0;
  espDetailPedReproduzirExterno = 0;
}
int detail_indice(void) {
  if (stubDetailAberto && espDetailImdb[0]) {
    const CatItem *ci = cat_item(espDetailIdx);
    if (!ci || !idbase_mesmo(ci->imdb, espDetailImdb)) {
      int novo = cat_indice_por_imdb(espDetailImdb);
      if (novo >= 0) espDetailIdx = novo;
    }
  }
  return espDetailIdx;
}
void detail_pedir_reproduzir(void) {
  espDetailReproduzir++;
  if (stubDetailAberto && !stubDetailSaindo && !espDetailPedReproduzir) {
    espDetailPedReproduzir = 1;
    espDetailPedReproduzirExterno = 1;
  }
}
int detail_pediu_reproduzir(void) {
  int v = espDetailPedReproduzir;
  espDetailPedReproduzir = 0;
  espDetailPedReproduzirExterno = 0;
  return v && stubDetailAberto && !stubDetailSaindo;
}
int detail_pedido_usuario_pendente(void) {
  return stubDetailAberto && !stubDetailSaindo && espDetailPedReproduzir && !espDetailPedReproduzirExterno;
}
int detail_pediu_do_inicio(void) { return 0; }
int detail_pediu_abrir(void) { int v = espDetailPedAbrir; espDetailPedAbrir = -1; return v; }
void detail_volta_notar(int n) { (void)n; }
void detail_mostrar_pessoa(long t, const char *n, const char *a) { (void)t; (void)n; (void)a; }
int detail_ep_foco(int *t, int *e) {
  if (t) *t = espDetailFocoT;
  if (e) *e = espDetailFocoE;
  return espDetailFocoE > 0;
}
static int ctxlista_tomar(int n) { (void)n; return 0; }
int desc_titulo_pronto(void) { return -1; }
int ajustes_retomada_morna(void) { return 0; }
int cacheboost_backend_quadro(int n) { (void)n; return 0; }
static int player_retomar_retido(const char *id, int t, int e) {
  (void)id; (void)t; (void)e;
  if (!stubRetained) return 0;
  espRetainedReopened++;
  stubPlayerAberto = 1;
  return 1;
}
int ilhacart_vivo_vale(const IlhaCartao *c) { (void)c; return stubValidIsland; }
void ilhacart_dispensar(int c) { (void)c; }
void ilhacart_adiar(int c) { (void)c; }
int juntos_reentrar(void) { return 0; }
void avisos_marcar_visto(const char *id) { (void)id; }
void ilha_modal_fechar(int a) { (void)a; }
int menu_aberto(void) { return 0; }
static void spainel_abrir(void) {}
static void spainel_abrir_de(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
int ilha_rect(float *x, float *y, float *w, float *h) { (void)x; (void)y; (void)w; (void)h; return 0; }
void desc_episodios(int indice, int forcar) {
  (void)indice; (void)forcar; espDescEpisodios++;
}
int desc_elenco_precisa_pessoas(const CatItem *ci) { (void)ci; return 0; }
static int explorar_pediu_abrir(int *idx) { (void)idx; return 0; }
static void detail_fechar_seco_(void) {
  if (stubDetailAberto) {
    stubDetailSaindo = 1;
    espDetailPedReproduzir = 0;
    espDetailPedReproduzirExterno = 0;
  }
}
void detail_fechar_seco(void) { detail_fechar_seco_(); }
static int spainel_aberto(void) { return 0; }
static void spainel_fechar(void) {}
static int player_retido(void) { return 0; }
static void player_encerrar(void) { espPlayerEncerrar++; stubPlayerAberto = 0; }
static void preparoInvalidar(void) {}
static void fa_nova_rodada(void) {}
static void p2pmotor_cancelar(void) {}
static void p2p_cancelar(void) {}
void menu_definir_destino(int m) { (void)m; }
static void busca_definir_consulta(const char *s) { (void)s; }
void desc_pedir_titulo(const char *s) { (void)s; }
void desc_pedir_titulo_tmdb(long tmdb, const char *tipo) { (void)tmdb; (void)tipo; }
int desc_titulo_buscando(void) { return 0; }

static int player_visivel(void) { return stubPlayerAberto; }
static int player_indice(void) { return espPlayerIdx; }
static const char *player_id_canal(void) { return ""; }
static const char *player_tipo_canal(void) { return ""; }
static void player_episodio_atual(int *t, int *e) {
  if (t) *t = espPlayerT;
  if (e) *e = espPlayerE;
}
static void player_abrir(int idx, const char *p) {
  (void)p;
  espPlayerAbrir++;
  stubPlayerAberto = 1;
  espPlayerIdx = idx;
  espPlayerT = espPlayerE = 0;
}
static void player_definir_episodio(int t, int e) { espPlayerT = t; espPlayerE = e; }
static void player_rewatch(int r) { (void)r; }
static void player_do_inicio(void) {}
static void episodioDoDetalhe(void) {
  int t = 0, e = 0;
  if (detail_ep_foco(&t, &e) && t >= 0 && e > 0) player_definir_episodio(t, e);
}
static void limparFonteVOD(void) { espLimparFonteVod++; }
static void diagnostico_cancelar_vazao(void) {}
static void aquecerAoAbrirTitulo(void) {}
static void aquecerQuandoChegarem(void) {}
static void prepararAoAbrir(void) {}
static int aguardandoFonte = 0;
static int autoEsperaN = 0, stalkerRenovando = 0;
static int preparoEspera = 0, preparoLembradaPlay = -1, fonteEscolhida = -2, fonteFalha = 0;
static unsigned fontePedidoGeracao = 0;
static unsigned torrentSessao = 0;
static int torrentPendente = -1;
enum { FJOB_FALHA = 1 };
static unsigned novaGeracaoFonte(void) { return 1u; }
static void limparFontePendente(void) {}
static int pedirFonteJob(int a, unsigned b, const char *c, int d) {
  (void)a; (void)b; (void)c; (void)d; return 0;
}
static int preparoUsar(const char *a, int b, int *c) { (void)a; (void)b; (void)c; return 0; }
static int preparoEmCurso(const char *a, int b) { (void)a; (void)b; return 0; }
static int tocarParteGuardada(void) { return 0; }
static int tocarFonteGuardada(void) { return 0; }
int ajustes_fonte_manual(void) { return 0; }
int fontepref_escolher(const char *base) { (void)base; return -1; }
void fontepref_id_base(const char *id, char *dst, unsigned tam) {
  idbase_copiar(id, dst, tam);
}
static void idBaseDoTitulo(char *dst, size_t tam) {
  const CatItem *c = cat_item(detail_indice());
  if (tam) dst[0] = 0;
  if (c && c->imdb[0]) idbase_copiar(c->imdb, dst, (unsigned)tam);
}
static void idDoAlvo(const CatItem *ci, char *dst, size_t n) {
  int t = 0, e = 0;
  if (!ci) { if (n) dst[0] = 0; return; }
  if (!strcmp(ci->tipo, "series") && detail_ep_foco(&t, &e) && t >= 0 && e > 0) {
    cat_id_stream(detail_indice(), t, e, dst, (unsigned)n);
    return;
  }
  snprintf(dst, n, "%s", ci->imdb);
}
const JsMotor *juntos_motor(void) { return NULL; }
int juntos_no_player(void) { return 0; }
void juntos_escolher_titulo(const char *a, int b, int c, const char *d, const char *e) {
  (void)a; (void)b; (void)c; (void)d; (void)e;
}
static void juntosui_sala_abriu(void) {}
static int travaui_segurar_detalhe(void) { return 0; }
static int perfilPermiteReproduzir(int indice) { (void)indice; return 1; }
int partes_disponivel(void) { return 0; }
static int debrid_tb_disponivel(void) { return 0; }
int partes_ponto_pegar(const char *a, const char *b, int c, PartesPonto *d) {
  (void)a; (void)b; (void)c; (void)d; return 0;
}
int stream_folha_aberta(void) { return 0; }
void stream_definir_alvo(const char *alvo) {
  snprintf(espStreamAlvo, sizeof espStreamAlvo, "%s", alvo ? alvo : "");
}
Uint32 stream_idade_ms(void) { return 0u; }
int stream_lista_do_alvo(const char *alvo) {
  return alvo && espStreamListaAlvo[0] && !strcmp(alvo, espStreamListaAlvo);
}
int addons_base_por_id_copiar(const char *id, char *dst, unsigned tam) {
  (void)id; if (tam) dst[0] = 0; return 0;
}
void addons_definir_origem(const char *base) { (void)base; }
static void origemFontes(const CatItem *c) { (void)c; }
static void servidores_dica(const char *a, const char *b, int c) { (void)a; (void)b; (void)c; }
void addons_buscar(const char *alvo, const char *tipo) {
  (void)tipo;
  espAddonsBuscar++;
  snprintf(espAddonsAlvo, sizeof espAddonsAlvo, "%s", alvo ? alvo : "");
  snprintf(espStreamListaAlvo, sizeof espStreamListaAlvo, "%s", alvo ? alvo : "");
}
void addons_buscar_legendas(const char *alvo, const char *tipo) { (void)alvo; (void)tipo; }
static int srvidx_ano_de(const char *meta) { (void)meta; return 0; }

#define NV_LINK_VALIDO_MS 60000
#define CW_ESPERA_MS 8000u
enum { TELA_HOME = 0, TELA_EXPLORAR = 1, TELA_LOGIN = 2, TELA_ESCOLHA_PERFIL = 3, TELA_BUSCA = 4 };
static int tela = TELA_HOME;
static void trocarTela(int t) { tela = t; }
static int homeAcaoDet = -1;
static Uint32 homeAcaoDetEm = 0;
static int cwRewatch, cwTocarT, cwTocarE, cwEspera;
static CatItem cwEsperaItem;
static Uint32 cwEsperaDesde;

/* APP_FAROL_PEND */

/* APP_ABRIR_TITULO */

/* APP_ABRIR_POR_INDICE */

static struct { long tmdb; const char *nome, *arte; } spotPessoaDepois;
static Uint32 spotPessoaDesde;
#define SPOT_PESSOA_PRAZO_MS 30000u
static void player_definir_episodio(int t, int e);
/* APP_TROCA_TITULO */

static void despacharIlhaApp(int o, int qual, IlhaCartao ic) {
/* APP_ILHA_DISPATCH */
}

/* APP_DEEPLINK_ABRIR */

/* APP_ALVO_PLAYER */

/* APP_RENOVAR_LISTA */

static void despacharAppHome(void) {
  int idx = -1;
  HomeItem it;
  (void)idx;
/* APP_HOME_DISPATCH */
}

static void despacharDeeplinkApp(void) {
/* APP_DEEPLINK_DISPATCH */
}

static void passoEsperaApp(void) {
/* APP_CW_ESPERA */
}

static void passoPrefetchApp(void) {
  if (detail_aberto()) {
/* APP_DETAIL_PREFETCH */
  }
}

static void passoConsumoApp(void) {
  if (detail_aberto()) {
/* APP_DETAIL_CONSUMO */
  }
/* APP_DETAIL_POST */
}

static void teclaOk(void) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  home_evento(&e);
  fakeTicks += 50u;
  e.type = SDL_KEYUP;
  home_evento(&e);
}

static void montarCatalogoComFarol(void) {
  CatItem it[12];
  CatFileira fils[2];
  int i;
  memset(it, 0, sizeof it);
  memset(fils, 0, sizeof fils);
  for (i = 0; i < 12; i++) {
    snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
    strcpy(it[i].tipo, "movie");
    snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
    strcpy(it[i].meta, "2024 · 140 min");
  }
  strcpy(fils[0].chave, "farol_gosto1");
  strcpy(fils[0].titulo, "Para voce");
  fils[0].ini = 0; fils[0].n = 6;
  strcpy(fils[1].chave, "addon_pop");
  strcpy(fils[1].titulo, "Populares");
  strcpy(fils[1].base, "https://a.invalid");
  strcpy(fils[1].catId, "pop");
  strcpy(fils[1].tipo, "movie");
  fils[1].ini = 6; fils[1].n = 6;
  cat_definir_tudo(it, 12, fils, 2);
}

static int acharFileira(const char *chave) {
  for (int r = 0; r < nFileiras; r++)
    if (!strcmp(fileiras[r].chave, chave)) return r;
  return -1;
}

static void zerarEspias(void) {
  espDetailAbrir = 0;
  stubDetailAberto = 0;
  stubDetailSaindo = 0;
  memset(&espDetailItem, 0, sizeof espDetailItem);
  espDetailImdb[0] = 0;
  espDetailReproduzir = 0;
  espDetailPedReproduzir = 0;
  espDetailPedReproduzirExterno = 0;
  espDetailPedAbrir = -1;
  espDetailFocoT = espDetailFocoE = 0;
  stubRetained = 0;
  espRetainedReopened = 0;
  stubValidIsland = 1;
  espDescartarRetido = 0;
  espDescEpisodios = 0;
  espDetailIdx = -1;
  espPlayerAbrir = 0;
  stubPlayerAberto = 0;
  espPlayerIdx = -1;
  espPlayerT = espPlayerE = 0;
  espPlayerEncerrar = 0;
  espAddonsBuscar = 0;
  espAddonsAlvo[0] = 0;
  espStreamAlvo[0] = 0;
  espStreamListaAlvo[0] = 0;
  espLimparFonteVod = 0;
  cwRewatch = cwTocarT = cwTocarE = cwEspera = 0;
  memset(&cwEsperaItem, 0, sizeof cwEsperaItem);
  cwEsperaDesde = 0;
  farolPendLimpar();
}

static void limparDir(const char *dir) {
  char cmd[512];
  snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
  if (system(cmd) != 0) {}
}

int main(void) {
  const char *tmp = getenv("TMPDIR");
  char dir[256];
  if (!tmp || !tmp[0]) {
    fprintf(stderr, "farol_dica_app: TMPDIR obrigatorio\n");
    return 1;
  }
  snprintf(dir, sizeof dir, "%s/nuvio-farol-dica-app-%ld", tmp, (long)getpid());
  limparDir(dir);
  assert(mkdir(dir, 0700) == 0);
  setenv("HOME", dir, 1);
  setenv("NUVIO_DADOS", dir, 1);

  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  salvos_perfil(1); salvos_iniciar();
  avisodisp_esquecer();
  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  montarCatalogoComFarol();

  // 1) Serie fora do catalogo (progresso T2E4 + Diario nome, sem arte) -> abrir
  {
    ProgRegistro pr;
    memset(&pr, 0, sizeof pr);
    pr.perfil = 1;
    strcpy(pr.chave, "tt7770010_s2e4");
    strcpy(pr.contentId, "tt7770010");
    strcpy(pr.tipo, "series");
    pr.temporada = 2; pr.episodio = 4;
    pr.posSeg = 1100; pr.durSeg = 2700;
    pr.lastWatchedMs = (stubAgoraS - 35LL * 86400LL) * 1000LL;
    pr.tocou = 1;
    assert(prog_aplicar_remoto(&pr) == 1);
    diario_nome_definir("tt7770010", "Andor", 2022, "");
  }
  home_atualizar(0.016f, fakeTicks);
  int r1 = acharFileira("farol_gosto1");
  assert(r1 >= 0 && fileiras[r1].temDica == 1 && fileiras[r1].dicaFocavel == 1);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDescartarRetido == 1);
  assert(espDetailItem.arte == NULL);
  {
    const CatItem *ci = cat_item(espDetailItem.indice);
    assert(ci && !strcmp(ci->imdb, "tt7770010"));
    assert(!strcmp(ci->tipo, "series"));
    assert(ci->temporada == 2 && ci->episodio == 4);
  }

  // 2) Serie fora do catalogo (Diario DIARIO_SERIE T3E7, sem arte) -> tocar
  stubAgoraS += 86400LL;
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7770011");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_PLAYER;
    de.temporada = 3; de.episodio = 7;
    de.quando = stubAgoraS - 10LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7770011", "Dark", 2017, "");
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_tocar_ser\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7770011\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7770011\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDetailItem.arte == NULL);
  assert(cwTocarT == 3 && cwTocarE == 7);
  assert(cwEspera == 2 && espDescEpisodios == 1);
  {
    const CatItem *ci = cat_item(espDetailItem.indice);
    assert(ci && !strcmp(ci->imdb, "tt7770011") && !strcmp(ci->tipo, "series"));
  }
  {
    CatEp ep;
    char streamId[96] = "";
    memset(&ep, 0, sizeof ep);
    ep.temporada = 3; ep.episodio = 7;
    strcpy(ep.vid, "tt7770011:3:7");
    cat_definir_episodios(detail_indice(), &ep, 1);
    passoEsperaApp();
    assert(cwEspera == 0 && espDetailReproduzir == 1);
    assert(cwTocarT == 3 && cwTocarE == 7);
    assert(cat_id_stream(detail_indice(), cwTocarT, cwTocarE, streamId, sizeof streamId) == 1);
    assert(!strcmp(streamId, "tt7770011:3:7"));
  }

  // 3) Filme fora do catalogo (Diario nome, sem arte) -> tocar
  stubAgoraS += 86400LL;
  diario_nome_definir("tt7770012", "Her", 2013, "");
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_tocar_mov\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7770012\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7770012\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDetailItem.arte == NULL);
  assert(espDetailReproduzir == 1);

  // 4) Serie fora do catalogo SEM episodio (Diario T0E0, sem arte) -> tocar
  //    Abre a pagina do detalhe (sem exigir arte) e nao pede reproducao de ep 0.
  stubAgoraS += 86400LL;
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7770013");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_MANUAL;
    de.temporada = 0; de.episodio = 0;
    de.quando = stubAgoraS - 5LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7770013", "Shogun", 2024, "");
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_tocar_ser0\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7770013\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7770013\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDetailItem.arte == NULL);
  assert(espDetailReproduzir == 0);
  {
    const CatItem *ci = cat_item(espDetailItem.indice);
    assert(ci && !strcmp(ci->imdb, "tt7770013") && !strcmp(ci->tipo, "series"));
    assert(ci->episodio == 0);
  }

  // 5) Regressao r2: P2 formado com Diario T4E6 sem arte, e descoberta republica
  //    tt7723120 sem arte com T/E zerados (T0E0) entre composicao e OK.
  //    Preserva T4E6 ate o despacho, entra em espera e resolve ID tt7723120:4:6
  //    sem escrever T/E no item global do catalogo.
  stubAgoraS += 86400LL;
  montarCatalogoComFarol();
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7723120");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_PLAYER;
    de.temporada = 4; de.episodio = 6;
    de.quando = stubAgoraS - 6LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7723120", "Severance", 2022, "");
  }
  assert(cat_indice_por_imdb("tt7723120") < 0);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r2_t0e0\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  assert(dicaCache.alvoTemporada == 4 && dicaCache.alvoEpisodio == 6);
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    it[0].temporada = 0; it[0].episodio = 0;
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  assert(r1 >= 0 && fileiras[r1].temDica == 1 && fileiras[r1].dicaFocavel == 1);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDetailItem.arte == NULL);
  assert(cwTocarT == 4 && cwTocarE == 6);
  assert(cwEspera == 2 && espDescEpisodios == 1);
  assert(cwEsperaItem.temporada == 4 && cwEsperaItem.episodio == 6);
  assert(cat_item(espDetailItem.indice)->temporada == 0 &&
         cat_item(espDetailItem.indice)->episodio == 0);
  {
    CatEp eps[2];
    char streamId[96] = "";
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 1; eps[0].episodio = 1; strcpy(eps[0].vid, "tt7723120:1:1");
    eps[1].temporada = 4; eps[1].episodio = 6; strcpy(eps[1].vid, "tt7723120:4:6");
    cat_definir_episodios(detail_indice(), eps, 2);
    passoEsperaApp();
    assert(cwEspera == 0 && espDetailReproduzir == 1);
    assert(cwTocarT == 4 && cwTocarE == 6);
    assert(cat_id_stream(detail_indice(), cwTocarT, cwTocarE, streamId, sizeof streamId) == 1);
    assert(!strcmp(streamId, "tt7723120:4:6"));
  }

  // 6) Regressao r2: republicacao com OUTRO episodio (T1E2) e progresso concluido
  //    no catalogo NAO desvia T4E6 nem aciona prox_seguinte do item global.
  stubAgoraS += 86400LL;
  montarCatalogoComFarol();
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7723121");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_PLAYER;
    de.temporada = 4; de.episodio = 6;
    de.quando = stubAgoraS - 6LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7723121", "Foundation", 2021, "");
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r2_t1e2\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723121\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723121\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  assert(dicaCache.alvoTemporada == 4 && dicaCache.alvoEpisodio == 6);
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723121");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Foundation");
    it[0].temporada = 1; it[0].episodio = 2; it[0].progresso = 100;
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(cwTocarT == 4 && cwTocarE == 6);
  assert(cwEspera == 2 && espDescEpisodios == 1);
  assert(cwEsperaItem.temporada == 4 && cwEsperaItem.episodio == 6);
  assert(cat_item(espDetailItem.indice)->temporada == 1 &&
         cat_item(espDetailItem.indice)->episodio == 2);
  {
    CatEp eps[3];
    char streamId[96] = "";
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 1; eps[0].episodio = 2; strcpy(eps[0].vid, "tt7723121:1:2");
    eps[1].temporada = 1; eps[1].episodio = 3; strcpy(eps[1].vid, "tt7723121:1:3");
    eps[2].temporada = 4; eps[2].episodio = 6; strcpy(eps[2].vid, "tt7723121:4:6");
    cat_definir_episodios(detail_indice(), eps, 3);
    passoEsperaApp();
    assert(cwEspera == 0 && espDetailReproduzir == 1);
    assert(cwTocarT == 4 && cwTocarE == 6);
    assert(cat_id_stream(detail_indice(), cwTocarT, cwTocarE, streamId, sizeof streamId) == 1);
    assert(!strcmp(streamId, "tt7723121:4:6"));
  }

  // 7) Guardas entre OK e despacho: troca de revisao de catalogo, troca de sessao,
  //    troca de perfil e dispensa rejeitam o pedido pendente.
  // 7a) Revisao de catalogo muda apos OK:
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  montarCatalogoComFarol();
  despacharAppHome();
  assert(espDetailAbrir == 0 && espDetailReproduzir == 0 && cwEspera == 0);

  // 7b) Sessao muda apos OK:
  stubAgoraS += 86400LL;
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r2_guard\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  stubSessao++;
  despacharAppHome();
  assert(espDetailAbrir == 0 && espDetailReproduzir == 0 && cwEspera == 0);

  // 7c) Perfil muda apos OK (mesmo sem alterar stubSessao):
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  stubPerfil = 2;
  despacharAppHome();
  assert(espDetailAbrir == 0 && espDetailReproduzir == 0 && cwEspera == 0);
  stubPerfil = 1;

  // 7d) Dispensa apos OK:
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  assert(faroldica_dispensar() == 1);
  despacharAppHome();
  assert(espDetailAbrir == 0 && espDetailReproduzir == 0 && cwEspera == 0);

  // 8) Cartao comum de Continuar assistindo preserva semantica normal (sem P2):
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723122");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Pachinko");
    it[0].temporada = 1; it[0].episodio = 5; it[0].progresso = 40;
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "continue_watching");
    strcpy(fils[0].titulo, "Continuar assistindo");
    fils[0].ini = 0; fils[0].n = 2;
    strcpy(fils[1].chave, "farol_gosto1");
    strcpy(fils[1].titulo, "Para voce");
    fils[1].ini = 2; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  int rCw = acharFileira("continue_watching");
  assert(rCw >= 0);
  focoHero = 0; foco.fileira = rCw; foco.coluna = 0;
  itemFoco.indice = fileiraItemIndice(&fileiras[rCw], 0);
  itemFoco.rect = (GfxRect){ 48, 200, 320, 180 };
  itemFoco.arte = NULL;
  itemFoco.titulo = "Pachinko";
  itemFoco.genero = NULL;
  itemFoco.meta = NULL;
  publicarItemFoco();
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(cwTocarT == 1 && cwTocarE == 5);
  assert(cwEspera == 2 && espDescEpisodios == 1);
  {
    CatEp ep;
    memset(&ep, 0, sizeof ep);
    ep.temporada = 1; ep.episodio = 5; strcpy(ep.vid, "tt7723122:publicado:1:5");
    cat_definir_episodios(detail_indice(), &ep, 1);
    passoEsperaApp();
    assert(cwEspera == 0 && espDetailReproduzir == 1);
    passoConsumoApp();
    assert(espPlayerAbrir == 1 && espPlayerT == 1 && espPlayerE == 5);
    assert(espAddonsBuscar == 1 && !strcmp(espAddonsAlvo, "tt7723122:publicado:1:5"));
  }

  // 8b) Cartao comum de Continuar assistindo concluido (T1E2 100%) espera proximo (T1E3):
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723122");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Pachinko");
    it[0].temporada = 1; it[0].episodio = 2; it[0].progresso = 100;
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "continue_watching");
    strcpy(fils[0].titulo, "Continuar assistindo");
    fils[0].ini = 0; fils[0].n = 2;
    strcpy(fils[1].chave, "farol_gosto1");
    strcpy(fils[1].titulo, "Para voce");
    fils[1].ini = 2; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  rCw = acharFileira("continue_watching");
  focoHero = 0; foco.fileira = rCw; foco.coluna = 0;
  itemFoco.indice = fileiraItemIndice(&fileiras[rCw], 0);
  itemFoco.rect = (GfxRect){ 48, 200, 320, 180 };
  itemFoco.arte = NULL;
  itemFoco.titulo = "Pachinko";
  itemFoco.genero = NULL;
  itemFoco.meta = NULL;
  publicarItemFoco();
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 1 && espDescEpisodios == 1);
  {
    CatEp eps[2];
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 1; eps[0].episodio = 2; strcpy(eps[0].vid, "tt7723122:publicado:1:2");
    eps[1].temporada = 1; eps[1].episodio = 3; strcpy(eps[1].vid, "tt7723122:publicado:1:3");
    cat_definir_episodios(detail_indice(), eps, 2);
    passoEsperaApp();
    assert(cwEspera == 0 && espDetailReproduzir == 1);
    passoConsumoApp();
    assert(espPlayerAbrir == 1 && espPlayerT == 1 && espPlayerE == 3);
    assert(espAddonsBuscar == 1 && !strcmp(espAddonsAlvo, "tt7723122:publicado:1:3"));
  }

  // 9) Regressao r3 #1: em B aberto apos P2 em A, chegada da lista de A NAO
  //    produz pedido, NAO abre player e NAO consulta fontes.
  stubAgoraS += 86400LL;
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    strcpy(it[1].imdb, "tt7723199");
    strcpy(it[1].tipo, "series");
    strcpy(it[1].titulo, "Silo B");
    it[1].temporada = 1; it[1].episodio = 1;
    for (int i = 2; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r3_b\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && espDetailAbrir == 1 && !strcmp(cat_item(detail_indice())->imdb, "tt7723120"));
  // Troca do Detalhe para B (tt7723199) antes da chegada da lista de A:
  {
    HomeItem hb = {0};
    hb.indice = 1;
    hb.titulo = "Silo B";
    detail_abrir(&hb);
  }
  espDetailReproduzir = 0;
  espAddonsBuscar = 0;
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6; strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 1);
  }
  passoEsperaApp();
  assert(cwEspera == 0 && espDetailReproduzir == 0 && espDetailPedReproduzir == 0);
  passoConsumoApp();
  assert(espPlayerAbrir == 0 && espAddonsBuscar == 0);

  // 10) Regressao r3 #2: depois de Voltar (saindo=1 com detail_aberto ainda 1),
  //     chegada da lista de A NAO produz pedido, NAO abre player e NAO consulta fontes.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r3_back\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2);
  // Voltar marca saindo=1 mantendo detail_aberto() == 1 durante animacao de saida:
  detail_voltar_sair();
  assert(detail_aberto() == 1 && detail_ciclo() == 0);
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6; strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 1);
  }
  passoEsperaApp();
  assert(cwEspera == 0 && espDetailReproduzir == 0 && espDetailPedReproduzir == 0);
  passoPrefetchApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 0 && espAddonsBuscar == 0);

  // 11) Regressao r3 #3: troca de geracao de sessao (1 -> 2) e troca de perfil
  //     invalidam a espera P2 mesmo se o mesmo IMDb reaparece em outro indice.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r3_sess\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2);
  stubSessao++;
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    for (int i = 0; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(it[5].imdb, "tt7723120");
    strcpy(it[5].tipo, "series");
    strcpy(it[5].titulo, "Severance");
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  home_abrir_sessao();
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6; strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(5, epsA, 1);
  }
  passoEsperaApp();
  assert(cwEspera == 0 && espDetailReproduzir == 0 && espDetailPedReproduzir == 0);
  passoConsumoApp();
  assert(espPlayerAbrir == 0 && espAddonsBuscar == 0);

  // 11b) Troca de perfil (stubPerfil 1 -> 2) tambem invalida a espera P2:
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r3_prof\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2);
  stubPerfil = 2;
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6; strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 1);
  }
  passoEsperaApp();
  assert(cwEspera == 0 && espDetailReproduzir == 0 && espDetailPedReproduzir == 0);
  passoConsumoApp();
  assert(espPlayerAbrir == 0 && espAddonsBuscar == 0);
  stubPerfil = 1;

  // 11c) Troca de sessao entre o disparo do pedido e o consumidor bloqueia player/fontes:
  stubAgoraS += 86400LL;
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r3_late\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2);
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6; strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 1);
  }
  passoEsperaApp();
  assert(cwEspera == 0 && espDetailReproduzir == 1 && espDetailPedReproduzir == 1);
  stubSessao++;
  passoConsumoApp();
  assert(espPlayerAbrir == 0 && espAddonsBuscar == 0);

  // 12) Controles verdes:
  // 12a) Republicacao da mesma serie em NOVO indice (0 -> 5), lista parcial
  //      espera, lista final publica vid e toca UMA vez com T4E6 pelo ID publicado.
  stubAgoraS += 86400LL;
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r3_ok\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && detail_indice() == 0);
  // Republica a mesma serie no indice 5 com T1E2 (30%) e lista parcial:
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    for (int i = 0; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(it[5].imdb, "tt7723120");
    strcpy(it[5].tipo, "series");
    strcpy(it[5].titulo, "Severance");
    it[5].temporada = 1; it[5].episodio = 2; it[5].progresso = 30;
    it[5].epsParciais = 1;
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  {
    CatEp eps[1];
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 4; eps[0].episodio = 6; strcpy(eps[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(5, eps, 1);
  }
  passoEsperaApp();
  assert(cwEspera == 2 && espDetailReproduzir == 0); // lista parcial ainda espera
  // Chega lista final no indice 5:
  {
    CatItem fin = *cat_item(5);
    fin.epsParciais = 0;
    cat_atualizar_item(5, &fin);
    CatEp eps[2];
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 1; eps[0].episodio = 2; strcpy(eps[0].vid, "tt7723120:publicado:1:2");
    eps[1].temporada = 4; eps[1].episodio = 6; strcpy(eps[1].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(5, eps, 2);
  }
  passoEsperaApp();
  assert(cwEspera == 0 && espDetailReproduzir == 1 && detail_indice() == 5);
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 5 && espPlayerT == 4 && espPlayerE == 6);
  assert(espAddonsBuscar == 1 && !strcmp(espAddonsAlvo, "tt7723120:publicado:4:6"));
  assert(cat_item(5)->temporada == 1 && cat_item(5)->episodio == 2 && cat_item(5)->progresso == 30);

  // 12b) Lista final sem T4E6 e prazo vencido -> nenhuma reproducao, detalhe preservado.
  stubAgoraS += 86400LL;
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r3_miss\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2);
  {
    CatEp eps[1];
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 1; eps[0].episodio = 2; strcpy(eps[0].vid, "tt7723120:publicado:1:2");
    cat_definir_episodios(detail_indice(), eps, 1);
  }
  passoEsperaApp();
  assert(cwEspera == 2 && espDetailReproduzir == 0);
  fakeTicks += CW_ESPERA_MS + 1u;
  passoEsperaApp();
  assert(cwEspera == 0 && espDetailReproduzir == 0 && detail_aberto() == 1);
  passoConsumoApp();
  assert(espPlayerAbrir == 0 && espAddonsBuscar == 0);

  // 13) Regressao r4: link externo valido para B (nuvio-legacy://play/series/tt7723199/1/3)
  //     durante espera P2 para A (tt7723120 T4E6, home_t4e6 + Diario T4E6) deve tocar
  //     B no episodio pedido sem ser descartado por farolPendExec de A, e A tardio
  //     permanece invalidado.
  stubAgoraS += 86400LL;
  zerarEspias();
  montarCatalogoComFarol();
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7723120");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_PLAYER;
    de.temporada = 4; de.episodio = 6;
    de.quando = stubAgoraS - 6LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7723120", "Severance", 2022, "");
  }
  assert(cat_indice_por_imdb("tt7723120") < 0);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"home_t4e6\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  assert(dicaCache.alvoTemporada == 4 && dicaCache.alvoEpisodio == 6);
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    strcpy(it[1].imdb, "tt7723199");
    strcpy(it[1].tipo, "series");
    strcpy(it[1].titulo, "Silo B");
    strcpy(it[1].poster, "https://a.invalid/b.jpg");
    it[1].temporada = 1; it[1].episodio = 1;
    for (int i = 2; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  {
    CatEp epsB[1];
    memset(epsB, 0, sizeof epsB);
    epsB[0].temporada = 1; epsB[0].episodio = 3;
    strcpy(epsB[0].vid, "tt7723199:publicado:1:3");
    cat_definir_episodios(1, epsB, 1);
  }
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && farolPendExec == 1);
  assert(!strcmp(cat_item(detail_indice())->imdb, "tt7723120"));
  deeplink_postar("nuvio-legacy://play/series/tt7723199/1/3");
  despacharDeeplinkApp();
  assert(cwEspera == 0 && farolPendExec == 0);
  assert(!strcmp(cat_item(detail_indice())->imdb, "tt7723199"));
  assert(espDetailPedReproduzir == 1);
  passoPrefetchApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 1 && espPlayerT == 1 && espPlayerE == 3);
  assert(espAddonsBuscar >= 1 && !strcmp(espAddonsAlvo, "tt7723199:publicado:1:3"));
  // Chegada tardia dos metadados de A nao pode disparar reproducao:
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6;
    strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 1);
  }
  passoEsperaApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 1 && espPlayerT == 1 && espPlayerE == 3);

  // 14) Controle positivo do link externo para B sem espera P2:
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    strcpy(it[1].imdb, "tt7723199");
    strcpy(it[1].tipo, "series");
    strcpy(it[1].titulo, "Silo B");
    strcpy(it[1].poster, "https://a.invalid/b.jpg");
    for (int i = 2; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  {
    CatEp epsB[1];
    memset(epsB, 0, sizeof epsB);
    epsB[0].temporada = 1; epsB[0].episodio = 3;
    strcpy(epsB[0].vid, "tt7723199:publicado:1:3");
    cat_definir_episodios(1, epsB, 1);
  }
  deeplink_postar("nuvio-legacy://play/series/tt7723199/1/3");
  despacharDeeplinkApp();
  passoPrefetchApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 1 && espPlayerT == 1 && espPlayerE == 3);
  assert(espAddonsBuscar >= 1 && !strcmp(espAddonsAlvo, "tt7723199:publicado:1:3"));

  // 15a) Link externo para o MESMO A em T1E3 durante espera P2 de A T4E6:
  //      novo ciclo abre A em T1E3 e metadados tardios de A nao reacionam T4E6.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    strcpy(it[0].poster, "https://a.invalid/a.jpg");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r4_same\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && farolPendExec == 1);
  {
    CatEp epsA[2];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 1; epsA[0].episodio = 3; strcpy(epsA[0].vid, "tt7723120:publicado:1:3");
    epsA[1].temporada = 4; epsA[1].episodio = 6; strcpy(epsA[1].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 2);
  }
  deeplink_postar("nuvio-legacy://play/series/tt7723120/1/3");
  despacharDeeplinkApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 0 && espPlayerT == 1 && espPlayerE == 3);
  assert(espAddonsBuscar >= 1 && !strcmp(espAddonsAlvo, "tt7723120:publicado:1:3"));

  // 15b) Reabrir A em novo ciclo apos enfileirar Play P2 e antes do consumo cancela P2:
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r4_reopen\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && farolPendExec == 1);
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6; strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 1);
  }
  passoEsperaApp();
  assert(espDetailPedReproduzir == 1);
  {
    HomeItem ha = {0};
    ha.indice = 0;
    ha.titulo = "Severance";
    detail_abrir(&ha);
  }
  passoConsumoApp();
  assert(espPlayerAbrir == 0 && espAddonsBuscar == 0);

  // 16) Play manual direto no Detalhe A durante espera P2 no mesmo ciclo:
  //     nao e descartado nem sequestrado pelo T4E6 da espera P2, e cancela espera.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r4_manual\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && farolPendExec == 1 && !farolPendPediu);
  espDetailPedReproduzir = 1; // Simula pedReproduzir = 1 direto no Detalhe no mesmo ciclo.
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 0);
  assert(espPlayerT == 0 && espPlayerE == 0); // T4E6 da espera P2 nao sobrescreveu o Play manual.
  assert(cwEspera == 0 && !farolPendExec);
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6; strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 1);
  }
  passoEsperaApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 1);

  // 17) Abertura concorrente sem link externo (abrirPorIndice) durante espera P2
  //     cancela a espera P2 para que metadados tardios de A nao toquem em B.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    strcpy(it[1].imdb, "tt7723199");
    strcpy(it[1].tipo, "series");
    strcpy(it[1].titulo, "Silo B");
    strcpy(it[1].poster, "https://a.invalid/b.jpg");
    for (int i = 2; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r4_open\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && farolPendExec == 1);
  abrirPorIndice(1);
  assert(cwEspera == 0 && !farolPendExec);
  {
    CatEp epsA[1];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 4; epsA[0].episodio = 6; strcpy(epsA[0].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 1);
  }
  passoEsperaApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 0);

  // 18) r5 #1: Ilha Play B T1E3 com B sem arte durante espera P2 de A T4E6:
  //     nao grava T1E3/Play em A; preserva P2 e usa ID publicado T4E6 quando A chega.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    strcpy(it[1].imdb, "tt7723199");
    strcpy(it[1].tipo, "series");
    strcpy(it[1].titulo, "Silo B");
    for (int i = 2; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r5_island_noart\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && farolPendExec == 1);
  {
    IlhaCartao ic = {0};
    ic.serie = 1; ic.t = 1; ic.e = 3;
    strcpy(ic.imdb, "tt7723199");
    despacharIlhaApp(ILHA_PEDIU_TOCAR, ILHA_VIVO, ic);
  }
  assert(detail_indice() == 0 && cwEspera == 2 && farolPendExec == 1 && !espDetailPedReproduzir);
  passoConsumoApp();
  assert(espPlayerAbrir == 0);
  {
    CatEp epsA[2];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 1; epsA[0].episodio = 3; strcpy(epsA[0].vid, "tt7723120:publicado:1:3");
    epsA[1].temporada = 4; epsA[1].episodio = 6; strcpy(epsA[1].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 2);
  }
  passoEsperaApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 0 && espPlayerT == 4 && espPlayerE == 6);
  assert(!strcmp(espAddonsAlvo, "tt7723120:publicado:4:6"));

  // 19) r5 #2: Relacionados (pedAbrir) de B sem arte durante espera P2 de A:
  //     mantem A aberto e P2 ativo, tocando T4E6 quando metadados de A chegam.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    strcpy(it[1].imdb, "tt7723199");
    strcpy(it[1].tipo, "series");
    strcpy(it[1].titulo, "Silo B");
    for (int i = 2; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r5_rel_noart\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && farolPendExec == 1);
  espDetailPedAbrir = 1;
  trocaDeTituloSeSolicitada();
  assert(detail_indice() == 0 && cwEspera == 2 && farolPendExec == 1);
  {
    CatEp epsA[2];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 1; epsA[0].episodio = 3; strcpy(epsA[0].vid, "tt7723120:publicado:1:3");
    epsA[1].temporada = 4; epsA[1].episodio = 6; strcpy(epsA[1].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 2);
  }
  passoEsperaApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 0 && espPlayerT == 4 && espPlayerE == 6);
  assert(!strcmp(espAddonsAlvo, "tt7723120:publicado:4:6"));

  // 20) r5 #3a: Play manual A T1E3 na fila antes do pump em que a lista final de A chega:
  //     Play manual vence P2 T4E6 e toca tt7723120:publicado:1:3.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r5_manual_ready\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  assert(cwEspera == 2 && farolPendExec == 1);
  espDetailFocoT = 1; espDetailFocoE = 3;
  espDetailPedReproduzir = 1;
  {
    CatEp epsA[2];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 1; epsA[0].episodio = 3; strcpy(epsA[0].vid, "tt7723120:publicado:1:3");
    epsA[1].temporada = 4; epsA[1].episodio = 6; strcpy(epsA[1].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 2);
  }
  passoEsperaApp();
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 0 && espPlayerT == 1 && espPlayerE == 3);
  assert(!strcmp(espAddonsAlvo, "tt7723120:publicado:1:3"));

  // 21) r5 #3b: P2 ja enfileirado (pedReproduzir=1, pedReproduzirExterno=1, farolPendPediu=1), seguido de selecao
  //     manual posterior no Detalhe antes do consumo: selecao manual T1E3 vence.
  stubAgoraS += 86400LL;
  zerarEspias();
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r5_manual_after_p2\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaOk();
  despacharAppHome();
  {
    CatEp epsA[2];
    memset(epsA, 0, sizeof epsA);
    epsA[0].temporada = 1; epsA[0].episodio = 3; strcpy(epsA[0].vid, "tt7723120:publicado:1:3");
    epsA[1].temporada = 4; epsA[1].episodio = 6; strcpy(epsA[1].vid, "tt7723120:publicado:4:6");
    cat_definir_episodios(0, epsA, 2);
  }
  passoEsperaApp();
  assert(farolPendPediu == 1 && espDetailPedReproduzir == 1 && espDetailPedReproduzirExterno == 1);
  espDetailFocoT = 1; espDetailFocoE = 3;
  espDetailPedReproduzir = 1; espDetailPedReproduzirExterno = 0;
  passoConsumoApp();
  assert(espPlayerAbrir == 1 && espPlayerIdx == 0 && espPlayerT == 1 && espPlayerE == 3);
  assert(!strcmp(espAddonsAlvo, "tt7723120:publicado:1:3"));

  limparDir(dir);
  puts("farol_dica (app): ok");
  return 0;
}
