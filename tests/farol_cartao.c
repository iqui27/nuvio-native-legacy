// Teste dirigido da issue #21 do Farol: cartao de erro do player com veredito.
// Compila src/player.c real junto com src/farol.c, src/farolregra.c,
// src/farolacao_fonte.c, src/streams.c, src/farolanel.c, src/farolerro.c e
// src/extplayer.c. Exercita o alvo desktop/webOS e o alvo Android (-DNV_ANDROID).
#define _POSIX_C_SOURCE 200809L
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Estado controlado pelos cenarios antes de incluir src/player.c.
static const char *c_erroTexto = "";
static const char *c_urlAtual = "https://cdn.invalid/f0.mkv";
static int c_xtreamLimitada = 0;
static int c_regNegado = 0;
static int c_videoFalhou = 0;
static int c_videoErrIdent = 0;
static int c_offline = 0;
static int c_addonsN = 1;
static int c_debridSemPlano = 0;
static int c_debridRecusa = 0;
static int c_telefoneUi = 0;
static int c_androidExtDisp = 1;
static int c_androidAbrirN = 0;
static char c_androidAbrirUrl[512];

static Uint32 c_ticks = 10000;
Uint32 SDL_GetTicks(void) { return c_ticks; }
Uint64 SDL_GetTicks64(void) { return c_ticks; }
char *SDL_GetBasePath(void) { return NULL; }
void SDL_free(void *mem) { free(mem); }

// Captura dos botoes desenhados por corpoErro e hitboxes do ponteiro.
typedef struct {
  char rotulo[128];
  char icone[64];
  float foco;
  float x, y, w, h;
} BotaoDesenhado;
static BotaoDesenhado c_botoes[8];
static int c_nBotoes = 0;
typedef struct {
  float x, y, w, h;
  void (*at)(int, int);
  int a, b;
} AlvoCapturado;
static AlvoCapturado c_alvos[16];
static int c_nAlvos = 0;

#include "debrid.h"
#include "ilhaacao.h"
#include "fonteregra.h"
#include "selospacote.h"
#include "farolsonda.h"
#include "fontesmortas.h"
#include "credfonte.h"
#include "progresso.h"
#include "partes.h"
#include "watchstate_envio.h"
#include "capmkv.h"
#include "ltv.h"
#include "../src/player.c"

// ------------------------------------------------------------------ stubs UI/OS
const char *i18n(const char *s) { return s ? s : ""; }
void marco(const char *s) { (void)s; }
int nv_webos_major(void) { return 5; }
int dts_playback_enabled(void) { return 0; }
int dts_tpk_disponivel(void) { return 0; }
int seguro_perfil_ativo(void) { return 0; }
int perfil_infantil_ativo(void) { return 0; }
int perfis_ativo(void) { return 1; }
int perfis_sessao_atual(uint64_t g) { return g == 1; }
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_travar(uint64_t g) { return g == 1; }
void perfis_sessao_destravar(void) {}
const char *sessao_usuario(void) { return "u"; }

// Catalogo
static CatItem c_catItem;
const CatItem *cat_item(int i) { return i == 0 ? &c_catItem : NULL; }
int cat_n(void) { return 1; }
int cat_n_episodios(int i) { (void)i; return 0; }
const CatEp *cat_episodio(int i, int k) { (void)i; (void)k; return NULL; }
int cat_ep_indice(int i, int t, int e) { (void)i; (void)t; (void)e; return -1; }
int cat_indice_titulo(const char *imdb, int p) { (void)imdb; return p; }
int cat_indice_por_imdb(const char *imdb) { (void)imdb; return 0; }
int cat_copiar_item(int i, CatItem *d) { if (i == 0 && d) { *d = c_catItem; return 1; } return 0; }
int cat_copiar_por_id(const char *id, const char *tp, CatItem *d) { (void)id; (void)tp; (void)d; return 0; }
int cat_fileira_imdb(const char *c, char (*o)[64], int m, int s) { (void)c; (void)o; (void)m; (void)s; return 0; }
unsigned cat_revisao(void) { return 1; }
unsigned cat_versao_episodios(void) { return 1; }
int cat_visto(const CatItem *ci) { (void)ci; return 0; }
int cat_remover_fileiras_prefixo(const char *d, const char *p) { (void)d; (void)p; return 0; }

// Video backend controlado
const char *video_erro_texto(void) { return c_erroTexto; }
const char *video_url_atual(void) { return c_urlAtual; }
int video_registro_negado(void) { return c_regNegado; }
int video_falhou(void) { return c_videoFalhou; }
int video_erro_identificado(void) { return c_videoErrIdent; }
int video_tocar(const char *u) { (void)u; return 0; }
int video_tocar_posicao(const char *u, double s) { (void)u; (void)s; return 0; }
int video_tocar_retomada(const char *u, double s, double p) { (void)u; (void)s; (void)p; return 0; }
void video_parar(void) {}
void video_pausar(int p) { (void)p; }
void video_buscar(double s) { (void)s; }
void video_buscar_preciso(double s) { (void)s; }
int video_pausa_confirmada(void) { return 0; }
int video_tocando(void) { return 0; }
int video_pronto(void) { return 0; }
int video_ativo(void) { return 0; }
int video_terminou(void) { return 0; }
int video_conflito_recurso(void) { return 0; }
int video_reconectando(void) { return 0; }
int video_recarregar(void) { return 0; }
double video_pos(void) { return 0.0; }
double video_duracao(void) { return 0.0; }
double video_pos_amostra(Uint64 *ms) { if (ms) *ms = 0; return 0.0; }
double video_buffer_fim(void) { return 0.0; }
unsigned video_bufferando_ms(void) { return 0; }
void video_janela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void video_janela_fonte(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
  (void)sx; (void)sy; (void)sw; (void)sh; (void)dx; (void)dy; (void)dw; (void)dh;
}
int video_dimensoes(int *w, int *h) { if (w) *w = 0; if (h) *h = 0; return 0; }
int video_n_audio(void) { return 0; }
const VideoFaixa *video_audio(int i) { (void)i; return NULL; }
int video_audio_atual(void) { return -1; }
int video_faixa_dts(const VideoFaixa *f) { (void)f; return 0; }
int video_dts_estado(void) { return 0; }
int video_audio_nao_suportado(void) { return 0; }
int video_n_leg(void) { return 0; }
const VideoFaixa *video_leg(int i) { (void)i; return NULL; }
int video_leg_atual(void) { return -1; }
void video_definir_audio(int i) { (void)i; }
void video_definir_leg(int i) { (void)i; }
void video_leg_url(const char *u) { (void)u; }
void video_leg_estilo(const VideoLegendaEstilo *e) { (void)e; }
void video_velocidade(int v) { (void)v; }
int video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void) { return "none"; }
const char *video_cabecalhos(void) { return ""; }
int video_prebusca_hdr(void) { return 0; }
double video_creditos(void) { return 0.0; }
void video_dv_definir(int d) { (void)d; }
void video_definir_dv(int d) { (void)d; }
void video_definir_cabecalhos(const char *c) { (void)c; }
void video_definir_mp4(int m) { (void)m; }
int video_tem_trilha_dts(void) { return 0; }
void video_libass_usar(int u) { (void)u; }
int video_legenda_nativa(char *b, int n) { (void)b; (void)n; return 0; }
int video_retomada_inicial_estado(void) { return -1; }
int video_superficie_estavel(void) { return 1; }
int video_quadro_visto(void) { return 0; }
#ifdef NV_ANDROID
void faixas_reiniciar_preparada(void) {}
int video_precarregar(const char *u, const char *c, double s, double p, int b, int cm, unsigned t, const char *la, int k) {
  (void)u; (void)c; (void)s; (void)p; (void)b; (void)cm; (void)t; (void)la; (void)k; return 0;
}
int video_precarregar_proximo(const char *u, const char *c, double s, double p, int b, int cm, unsigned t, const char *la, int k) {
  (void)u; (void)c; (void)s; (void)p; (void)b; (void)cm; (void)t; (void)la; (void)k; return 0;
}
int video_precarregar_estado(unsigned t) { (void)t; return 0; }
void video_precarregar_cancelar(unsigned t) { (void)t; }
void video_precarregar_adotar(unsigned t) { (void)t; }
void android_player_tela_cheia(int v) { (void)v; }
char *android_listar_players(void) {
  if (!c_androidExtDisp) return strdup("");
  return strdup("org.videolan.vlc\tVLC\n");
}
int android_abrir_externo(const char *u, const char *t, const char *p) {
  (void)t; (void)p;
  c_androidAbrirN++;
  snprintf(c_androidAbrirUrl, sizeof c_androidAbrirUrl, "%s", u ? u : "");
  return c_androidExtDisp;
}
const SeekrProvedor *miniatura_provedor(void) { return NULL; }
int miniatura_direto(void) { return 0; }
int seekr_provedor_e(const SeekrProvedor *p) { (void)p; return 0; }
void mkvass_preparo_cancelar(MkvassPreparo **p) { if (p) *p = NULL; }
#endif

// Xtream / Rede / Debrid / Addons / Farol Home stubs
int xtream_url_limitada(const char *u) { (void)u; return c_xtreamLimitada; }
int xtream_conexoes_limitadas(void) { return 0; }
int rede_saude_offline(void) { return c_offline; }
int addons_n(void) { return c_addonsN; }
int addons_ocupado(void) { return 0; }
int addons_motivo_vazio(char *d, unsigned n) { if (d && n) d[0] = 0; return 0; }
int addons_busca_parcial(void) { return 0; }
AddEstado addons_estado(void) { return ADD_PRONTO; }
int addons_parcial_abas(AddonAba *o, int m) { (void)o; (void)m; return 0; }
int debrid_sem_plano(void) { return c_debridSemPlano; }
int debrid_recusa(char *d, unsigned n) {
  if (!c_debridRecusa) { if (d && n) d[0] = 0; return 0; }
  snprintf(d, n, "auth"); return 1;
}
int debrid_ativo(void) { return 1; }
int debrid_servico_disponivel(int s) { return s == 0; }
const char *debrid_servico_nome(int s) { return s == 0 ? "Real-Debrid" : ""; }
int debrid_servico_por_nome(const char *n) { (void)n; return 0; }
int debrid_resolver_escolhido(const char *h, int a, char *u, unsigned t, char *sv, unsigned ns, int *p) {
  (void)h; (void)a; (void)u; (void)t; (void)sv; (void)ns; (void)p; return 0;
}
int debrid_resolver_servico_cache(int s, const char *h, int a, char *u, unsigned t, int *f) {
  (void)s; (void)h; (void)a; (void)f; snprintf(u, t, "https://debrid.invalid/ok.mp4"); return 1;
}
int debrid_resolver_cache(const char *h, int a, char *u, unsigned t, int *f) {
  return debrid_resolver_servico_cache(0, h, a, u, t, f);
}
void debrid_contexto_capturar(DebridContexto *c) { memset(c, 0, sizeof *c); }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
int p2p_ativo(void) { return 0; }
int p2p_resolver(const char *h, int a, const char *f, char *u, unsigned t) { (void)h; (void)a; (void)f; (void)u; (void)t; return 0; }
int p2p_ultimo_erro(void) { return 0; }
int p2pmotor_e_url(const char *u) { (void)u; return 0; }
int rede_url_final_tipo(const char *u, int s, const char *const *c, char *f, unsigned t, int *st, char *m, unsigned mt, long *cp) {
  (void)s; (void)c; snprintf(f, t, "%s", u); snprintf(m, mt, "video/mp4"); *st = 200; *cp = 1024; return 1;
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; (void)st; return NULL; }
int naovideo_mime(const char *m, const char *u, long b) { (void)m; (void)u; (void)b; return 0; }
int naovideo_nome(const char *a, const char *b, int h, long t) { (void)a; (void)b; (void)h; (void)t; return 0; }
long memlog_livre_mb(void) { return 512; }
void memlog_devolver(void) {}
const char *atualizacao_nova(void) { return NULL; }

// Stubs da cola Home de farol.c
int home_fileiras_na_tela(void) { return 0; }
const char *app_tela_nome(void) { return "player"; }
int recomenda_pessoa_canonica(const char *id, char *d, size_t n) { (void)id; if (d && n) d[0] = 0; return 0; }
int farolcena_home(char *d, size_t c, const void *x, void *a, size_t b) { (void)d; (void)c; (void)x; (void)a; (void)b; return 0; }
int farolpeca_ler(void *p, const char *j, int nc) { (void)p; (void)j; (void)nc; return 0; }
int farolpeca_texto_tem(const void *t) { (void)t; return 0; }
const char *farolpeca_x(const void *c, const void *t) { (void)c; (void)t; return ""; }
int farolrec_nivel(const void *g) { (void)g; return 0; }
int farolficha_ler_item(const CatItem *ci, void *d) { (void)ci; (void)d; return 0; }
uint32_t farolficha_chave(const void *f) { (void)f; return 0; }
int farolsinal_modelo(void *d) { (void)d; return 0; }
int farolsinal_ficha_do_item(const CatItem *ci, void *d) { (void)ci; (void)d; return 0; }
void farolsinal_mostrado(const CatItem *ci) { (void)ci; }
int farolsinal_recomendar(void) { return 0; }
void farolsinal_zerar_gosto(void) {}
void farolsinal_definir_recomendar(int l) { (void)l; }
int fil_oculta(const char *c) { (void)c; return 0; }
int diario_n(int p) { (void)p; return 0; }
int diario_entrada(int p, int i, DiarioEntrada *d) { (void)p; (void)i; (void)d; return 0; }
int diario_titulo(int p, const char *id, DiarioTitulo *d) { (void)p; (void)id; (void)d; return 0; }
int diario_nome(const char *id, char *t, size_t nt, int *a, char *p, size_t np) {
  (void)id; if (t && nt) t[0] = 0; (void)a; if (p && np) p[0] = 0; return 0;
}
void diario_marcar(const CatItem *c, int t, int e, double p, double d) { (void)c; (void)t; (void)e; (void)p; (void)d; }
void diariofontes_registrar(const char *id, int t, int e, const char *r, const char *p) { (void)id; (void)t; (void)e; (void)r; (void)p; }
int salvos_n(void) { return 0; }
const void *salvos_item(int i) { (void)i; return NULL; }
int32_t mapa_hash_nome(const char *s) { (void)s; return -1; }
uint32_t mapa_hash_titulo(const char *s) { (void)s; return 1; }

// Stubs de UI / Ajustes / Subs / Outros usados em player.c
float nv_layout_w = 1080.0f, nv_layout_h = 1920.0f;
int layout_modo_mobile(void) { return c_telefoneUi; }
void ponteiro_camada(void) {}
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn fo, PonteiroFn at, int a, int b) {
  (void)fo;
  if (at && c_nAlvos < (int)(sizeof c_alvos / sizeof c_alvos[0])) {
    c_alvos[c_nAlvos++] = (AlvoCapturado){ x, y, w, h, at, a, b };
  }
}
void ponteiro_alvo_arrastavel(void) {}
int ponteiro_ativo(void) { return 0; }
int ponteiro_tem_toque(void) { return 0; }
float gfx_escala_ui(void) { return 1.0f; }
void gfx_cor(GfxRect r, float ra, float cr, float cg, float cb, float ca) { (void)r; (void)ra; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float a, float b, float c, float d, float e, float f, float g, float h) {
  (void)r; (void)t; (void)m; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h;
}
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) { (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_veu_css(GfxRect r, int b, float s, float c, float a) { (void)r; (void)b; (void)s; (void)c; (void)a; }
GLuint gfx_desfocado(GLuint t, const char *k) { (void)t; (void)k; return 0; }
float gfx_tex_aspect_atual = 0.0f;
GLuint tex_obter_larg(const char *u, float w) { (void)u; (void)w; return 0; }
float tex_aspecto(const char *u) { (void)u; return 1.77f; }
void tex_pressao_reproducao(int l) { (void)l; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  TxtLinha l = {0}; (void)e; (void)r; (void)g; (void)b; (void)a;
  l.w = s ? (int)strlen(s) * 10 : 0; l.h = 24;
  return l;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) {
  (void)w; return txt_linha(e, s, r, g, b, a);
}
int txt_largura(TxtEstilo e, const char *s) { (void)e; return s ? (int)strlen(s) * 10 : 0; }
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)a; (void)ml; return ld;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float plrui_botao_largura(const char *rot, const char *ic) {
  return (float)txt_largura(TXT_G21B, rot) + 56.0f + (ic ? 34.0f : 0.0f);
}
float plrui_botao_video(float x, float y, const char *rot, const char *ic, float foco, float a) {
  float w = plrui_botao_largura(rot, ic);
  (void)a;
  if (c_nBotoes < (int)(sizeof c_botoes / sizeof c_botoes[0])) {
    BotaoDesenhado *bd = &c_botoes[c_nBotoes++];
    snprintf(bd->rotulo, sizeof bd->rotulo, "%s", rot ? rot : "");
    snprintf(bd->icone, sizeof bd->icone, "%s", ic ? ic : "");
    bd->foco = foco; bd->x = x; bd->y = y; bd->w = w; bd->h = 60.0f;
  }
  return w;
}
void plrui_pilula_foco_video(GfxRect r, float a) { (void)r; (void)a; }
void plrui_botao_repouso_video(GfxRect r, float a) { (void)r; (void)a; }
int plrui_tinta(void) { return 16; }

void ilhaacao_feita(const IlhaAcao *a) { (void)a; }
void plrilha_pedir(const PlrIlhaPedido *p) { (void)p; }
void plrilha_relogio(float a, double f) { (void)a; (void)f; }
void plrilha_velocidade(int c) { (void)c; }
void plrilha_titulo(const char *t) { (void)t; }
int plrilha_direita(void) { return 0; }
void plrilha_esconder(void) {}
float plrilha_corpo_alfa(void) { return 1.0f; }

int addonguia_aberto(void) { return 0; }
int juntos_no_player(void) { return 0; }
int juntos_precarregando(void) { return 0; }
void juntos_sair(void) {}
int dvtela_ativa(void) { return 0; }
int dvtela_evento(const SDL_Event *e, Uint32 a) { (void)e; (void)a; return 0; }
void dvtela_sair(int m, Uint32 a) { (void)m; (void)a; }
int reacao_evento(const SDL_Event *e, int v) { (void)e; (void)v; return 0; }
void reacao_fechar(void) {}
int partescartao_evento(const SDL_Event *e) { (void)e; return 0; }
void partescartao_zerar(void) {}
int posplay_evento(const SDL_Event *e) { (void)e; return 0; }
void posplay_fechar(void) {}
int posplay_abrir_relacionados(int i) { (void)i; return 0; }
int pausao_visivel(void) { return 0; }
void pausao_fechar(void) {}
void pausao_activity(void) {}
int abrecine_tecla(AbreCine *c, SDL_Keycode k) { (void)c; (void)k; return 0; }
int abrecine_cinema(const AbreCine *c) { (void)c; return 0; }
int audioext_tocando(void) { return 0; }
int audioext_passo(int p, int r) { (void)p; (void)r; return 0; }
int audioext_offset_somar(int d) { (void)d; return 0; }
void audioext_formatar_offset(int o, char *b, unsigned n, const char *a, const char *c) { (void)o; (void)b; (void)n; (void)a; (void)c; }
void audioext_alvo(const char *a) { (void)a; }
int audioext_serie_procura(const char *a, AudioExtIdent *i) { (void)a; (void)i; return 0; }
void audioext_serie_achou(const char *u, const char *c, const char *n) { (void)u; (void)c; (void)n; }
void central_abrir(void) {}
void episodios_abrir(int i, int t, int e) { (void)i; (void)t; (void)e; }
void episodios_fechar(void) {}
int episodios_aberto(void) { return 0; }
int faixas_aberta(void) { return 0; }
void faixas_definir_fonte(const char *u) { (void)u; }
void intro_desligar(void) {}
int intro_creditos_aniskip(void) { return 0; }
double intro_creditos_seg(void) { return 0.0; }
void seekr_desligar(void) {}
void mkvass_parar(void) {}
void mkvass_video_aberto(int a) { (void)a; }
int mkvass_prebuscar(const char *u, MkvassEscolher fn, double p) { (void)u; (void)fn; (void)p; return 0; }
void legenda_abrir(void) {}
void legenda_desligar(void) {}
void legenda_pre_descartar(const char *m) { (void)m; }
void aovivo_erro_desenhar(const char *t, const char *p, const char *et, const char *ed, float e) { (void)t; (void)p; (void)et; (void)ed; (void)e; }
void aovivo_toque_reiniciar(void) {}
void aovivo_toque_definir(void (*fn)(int, int, int), int q) { (void)fn; (void)q; }
void guia_carregar(void) {}
void epg_iniciar(void) {}
void parental_pedir(const char *id) { (void)id; }
int parental_bloqueado(const char *id) { (void)id; return 0; }
int cacheboost_backend_quadro(int m) { (void)m; return 0; }
void retomamorna_cancelar(void) {}
void cacheboost_cache_relato(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
void cacheboost_ganho_relato(int g) { (void)g; }
int cacheboost_volume(void) { return 100; }
void conquistas_registrar_inicio(const char *id, const char *tp) { (void)id; (void)tp; }
void conquistas_registrar_fim(const char *id, const char *tp, double d) { (void)id; (void)tp; (void)d; }
void travaui_definir_abertura(int i, int t, int e) { (void)i; (void)t; (void)e; }
int travaui_segura(const char *id, int ix, int t, int e, int *p, int *n) { (void)id; (void)ix; (void)t; (void)e; (void)p; if (n) *n = 0; return 0; }
void diario_progresso_registrar(const char *id, int t, int e, double p, double d) { (void)id; (void)t; (void)e; (void)p; (void)d; }
void sync_sujar_progresso(void) {}
int trakt_scrobble(int ev, const char *id, double p, double d, double as, int a) { (void)ev; (void)id; (void)p; (void)d; (void)as; (void)a; return 0; }
int simkl_scrobble(int ev, const char *id, double p, double d, double as, int a) { (void)ev; (void)id; (void)p; (void)d; (void)as; (void)a; return 0; }
void ws_envio_marcar(const char *id, const char *tp, int t, int e, int p) { (void)id; (void)tp; (void)t; (void)e; (void)p; }
const char *artehero_url(const CatItem *c) { (void)c; return NULL; }
int artehero_eh_anime(const CatItem *c) { (void)c; return 0; }
char *dados_caminho(char *b, unsigned n, const char *a) { snprintf(b, n, "/tmp/%s", a); return b; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *v) { (void)n; (void)v; return 1; }
int partes_disponivel(void) { return 0; }

// Ajustes / fontes / streamfit / selos stubs
int ajustes_fonte_manual(void) { return 1; }
int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_regex_modo(void) { return 0; }
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_fonte_prioridade(void) { return 0; }
int ajustes_fonte_hdr(void) { return 1; }
int ajustes_fonte_idioma_audio(void) { return 0; }
int ajustes_fonte_preparar(void) { return 0; }
int ajustes_fonte_precarregar(void) { return 0; }
int ajustes_buffer_preset(void) { return 0; }
int ajustes_cache_seek_mb(void) { return 0; }
int ajustes_fonte_conferir_varias(void) { return 0; }
int ajustes_fonte_tocar_conferindo(void) { return 0; }
int ajustes_fonte_primeira(void) { return 0; }
int ajustes_fonte_ordem_uso(void) { return 0; }
int ajustes_fonte_ordem_addon(void) { return 0; }
int ajustes_dolby_vision(void) { return 0; }
int ajustes_dolby_atmos(void) { return 0; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_dts_desligado(void) { return 0; }
int ajustes_fonte_ocultar(int r) { (void)r; return 0; }
int ajustes_fonte_texto_addon(void) { return 0; }
int ajustes_audio_externo(void) { return 0; }
int ajustes_audio_externo_passo(void) { return 50; }
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
int ajustes_selos_coloridos(void) { return 0; }
int ajustes_idioma(void) { return 0; }
const char *ajustes_qualidade(void) { return "Automatica"; }
const char *ajustes_p2p_url(void) { return ""; }
int ajustes_player_externo(int a) { (void)a; return EXTPLAYER_INTERNO; }
const char *ajustes_player_pacote(int a) { (void)a; return ""; }
int ajustes_classif_player(void) { return 0; }
int ajustes_animacoes_reduzidas(void) { return 1; }
int ajustes_vidro(void) { return 0; }
float ajustes_vidro_opacidade(void) { return 1.0f; }
void ajustes_acento(float *r, float *g, float *b) { *r = *g = *b = 1.0f; }
int ajustes_tinta_foco(void) { return 16; }
int ajustes_manter_video(void) { return 0; }
int ajustes_saida_player_home(void) { return 0; }
int ajustes_retomada_morna(void) { return 0; }
int ajustes_velocidade_lembrar(void) { return 0; }
float ajustes_velocidade_padrao(void) { return 1.0f; }
int ajustes_relogio_12h(void) { return 0; }
int ajustes_pular_intro(void) { return 0; }
int ajustes_pular_creditos(void) { return 0; }
int ajustes_seek_preview(void) { return 0; }
int ajustes_leg_tamanho(void) { return 100; }
int ajustes_leg_pos(void) { return 0; }
int ajustes_osd_brilho(void) { return 100; }
int farolsonda_estado(FarolSonda *out) { if (out) memset(out, 0, sizeof *out); return 0; }
long fontesmortas_idade(const Stream *s) { (void)s; return -1; }
int ajustes_proporcao_padrao(void) { return 0; }
int ajustes_capitulos(void) { return 0; }
int ajustes_livetv_modo(void) { return 0; }
int ajustes_legenda_sync_auto(void) { return 0; }
int ajustes_cw_concluido(void) { return 90; }
int ajustes_show_skip_intro(void) { return 0; }
int ajustes_show_skip_recap(void) { return 0; }
int ajustes_show_skip_credits(void) { return 0; }
int ajustes_show_skip_cena(void) { return 0; }
int fontepref_escolher(const char *id) { (void)id; return -1; }
int fontepref_escolher_manual(const char *id) { (void)id; return -1; }
int fontepref_escolher_lista_next(const char *id, const Stream *s, int n,
                                  int (*pode)(int, int, void *), void *ctx) {
  (void)id; (void)s; (void)n; (void)pode; (void)ctx; return -1;
}
unsigned fontepref_revisao(void) { return 0; }
unsigned fonteregra_versao(void) { return 1; }
int fonteregra_grupo(const FonteRegraCfg *c, const char *n, int p, const char *t) { (void)c; (void)n; (void)p; (void)t; return 0; }
int fonteregra_no_escopo(const FonteRegraCfg *c, int p) { (void)c; (void)p; return 1; }
int fonteregra_ordem_rank(const char *p) { (void)p; return 0; }
uint64_t streamfit_agora_ms(void) { return 0; }
void streamfit_foto(StreamfitFoto *s, uint64_t ms) { (void)ms; memset(s, 0, sizeof *s); }
StreamfitClasse streamfit_classificar(const StreamfitFoto *f, const char *u, uint64_t b, double d, StreamfitResultado *s) {
  (void)f; (void)u; (void)b; (void)d; memset(s, 0, sizeof *s); return 0;
}
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int n, unsigned short *o, int m) { (void)p; (void)n; (void)o; (void)m; return 0; }
void selospacote_colorido(int c) { (void)c; }
int selospacote_embutidos_ok(void) { return 0; }
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }
int rewatch_ativa(const char *id) { (void)id; return 0; }
int rewatch_parcial_de(const char *id, int *t, int *e, int *p) { (void)id; (void)t; (void)e; (void)p; return 0; }
void visto_marcar(const CatItem *c, int t, int e, double p, double d) { (void)c; (void)t; (void)e; (void)p; (void)d; }
int vistoep_estado(const char *id, int t, int e) { (void)id; (void)t; (void)e; return 0; }
void vistoep_definir(const char *id, int t, int e, int v) { (void)id; (void)t; (void)e; (void)v; }
int home_retorno_vale(int i, double p, double d) { (void)i; (void)p; (void)d; return 0; }
double cred_estimar(const char *id, double d, int t, int e) { (void)id; (void)d; (void)t; (void)e; return 0.0; }
void cred_aprender(const char *id, double d, double p, int f) { (void)id; (void)d; (void)p; (void)f; }
int cred_carregado(const char *id) { (void)id; return 0; }
double cred_escolher(const CredEntrada *e, int *f) { (void)e; if (f) *f = 0; return 0.0; }
int cred_viz_ler(const char *id, int t, double *i, double *d) { (void)id; (void)t; (void)i; (void)d; return 0; }
double cred_aprendido_resto(const char *id) { (void)id; return 0.0; }
void credfonte_pedir(const char *id, int t, int e) { (void)id; (void)t; (void)e; }
double credfonte_inicio(const char *id, int t, int e) { (void)id; (void)t; (void)e; return 0.0; }
int proximo_pedir(const char *id, int t, int e, int *nt, int *ne) { (void)id; (void)t; (void)e; (void)nt; (void)ne; return 0; }
int prox_indice_seguinte(const CatEp *b, int n, int t, int e) { (void)b; (void)n; (void)t; (void)e; return -1; }
float duracaook_esperada(const CatItem *c, int t, int e) { (void)c; (void)t; (void)e; return 0.0f; }
void duracaook_guardar(const char *c, int p, const char *t) { (void)c; (void)p; (void)t; }

int video_largura(void) { return 0; }
int video_altura(void) { return 0; }
int video_recorte_fonte(void) { return 0; }
void video_definir_reconexao(int s) { (void)s; }
void video_definir_modo_live(int m) { (void)m; }
int cat_acrescentar(const CatItem *i) { (void)i; return -1; }
unsigned cat_revisao_itens(void) { return 1; }
int cat_tipo_confirmado_por_id(const char *id, char *t, unsigned n) { (void)id; if (t && n) t[0] = 0; return 0; }
int cat_id_stream(int i, int t, int e, char *d, unsigned n) { (void)i; (void)t; (void)e; if (d && n) d[0] = 0; return 0; }
void dados_marcar_sujo(int l) { (void)l; }
void desc_episodios_garantir(int i) { (void)i; }
int detail_arte_titulo(const CatItem *t, char *s, size_t n, int *p) { (void)t; if (s && n) s[0] = 0; (void)p; return 0; }
void artehero_logo_sessao_iniciar(const CatItem *i) { (void)i; }
void audioesc_titulo(const char *c) { (void)c; }
void faixas_reiniciar(void) {}
void cacheboost_sessao(void) {}
void cacheboost_backend_cache(int m) { (void)m; }
void cacheboost_backend_buffer(int m) { (void)m; }
int cacheboost_backend_buffer_mb(void) { return 0; }
int cacheboost_buffer_mb(int o, long l) { (void)o; (void)l; return 0; }
void cacheboost_backend_morno(int l) { (void)l; }
void cacheboost_backend_titulo(const char *id) { (void)id; }
const char *directcache_preparar(const char *u, const char *c, char *b, size_t n) { (void)c; (void)b; (void)n; return u; }
void directcache_encerrar(void) {}
const char *proxyts_resolver(const char *u, char *b, size_t n) { (void)b; (void)n; return u; }
void memlog_evento(const char *e) { (void)e; }
void playinfo_close(void) {}
void trailer_fechar(void) {}
int trailer_aberto(void) { return 0; }
unsigned trailer_geracao(void) { return 0; }
const char *extras_idioma_original(void) { return ""; }
int extras_n_relacionados(void) { return 0; }
void relogio_zerar(Relogio *r) { if (r) memset(r, 0, sizeof *r); }
void velmed_zerar(VelMedidor *m) { if (m) memset(m, 0, sizeof *m); }
void ltv_dig_limpar(LtvDigitos *g) { if (g) memset(g, 0, sizeof *g); }
int ltv_dig_ativo(const LtvDigitos *g) { (void)g; return 0; }
int ltv_dig_apertar(LtvDigitos *g, int d, unsigned a) { (void)g; (void)d; (void)a; return 0; }
int ltv_dig_confirmar(LtvDigitos *g, int *n) { (void)g; (void)n; return 0; }
void aovivo_erro_foco(int f) { (void)f; }
void aovivo_zap_apertar(AoVivoZap *z, int d, Uint32 a) { (void)z; (void)d; (void)a; }
int guia_modo_do_canal(const char *id, int p) { (void)id; return p; }
int guia_info_canal(const char *id, int *n, int *t, char *c, size_t s) { (void)id; (void)n; (void)t; if (c && s) c[0] = 0; return 0; }
void guia_alternar_favorito(const char *id) { (void)id; }
int rotahost_modo(const char *u) { (void)u; return 0; }
void abrecine_acordou(AbreCine *a, int c) { (void)a; (void)c; }
void legsync_auto_habilitar(int l) { (void)l; }
void legsync_iniciar(const char *u) { (void)u; }
void legsync_encerrar(void) {}
void legsync_ui_ligar(void) {}
LegendaDocumento *mkvass_referencia(const char *u, uint64_t s, const int *f, int n) { (void)u; (void)s; (void)f; (void)n; return NULL; }
void legref_fonte_local(LegendaDocumento *(*f)(const char *, uint64_t, const int *, int)) { (void)f; }
void legref_fonte_cabecalhos(const char *(*f)(void)) { (void)f; }
void mkv_capitulos_idioma(const char *i) { (void)i; }
int capmkv_lista(MkvCap *o, int m) { (void)o; (void)m; return 0; }
void imgsub_gl_limpar(void) {}
void intro_pedir_ids(const char *id, long tm, int t, int e, double da, double dp) { (void)id; (void)tm; (void)t; (void)e; (void)da; (void)dp; }
void intro_pedir_ids_mapeados(const char *id, long tm, int t, int e, int it, int ie, double da, double dp) { (void)id; (void)tm; (void)t; (void)e; (void)it; (void)ie; (void)da; (void)dp; }
int intro_botao_visivel(double *f, int *t) { (void)f; (void)t; return 0; }
void intro_botao_esconder(double a) { (void)a; }
void intro_auto_recuar(double d, double p) { (void)d; (void)p; }
double intro_creditos_janela(double d) { (void)d; return 0.0; }
double intro_fim_estimado(double d) { (void)d; return 0.0; }
void intro_contrib_registrar(const char *id, int t, int i, int f) { (void)id; (void)t; (void)i; (void)f; }
void posplay_recuar(void) {}
void posplay_busca_usuario(void) {}
void posplay_troca_manual(void) {}
void seekprobe_iniciar(const char *u, const char *h) { (void)u; (void)h; }
void seekr_ocioso(void) {}
int juntos_ao_alternar(int t) { (void)t; return 0; }
int juntos_ao_buscar(double s) { (void)s; return 0; }
int juntos_ao_voltar(void) { return 0; }
void prog_chave(char *d, unsigned n, const char *c, int t, int e) { (void)c; (void)t; (void)e; if (d && n) d[0] = 0; }
int prog_por_chave(const char *c, ProgRegistro *s) { (void)c; (void)s; return 0; }
void servidores_reproducao_fim(const char *u, double p, double d) { (void)u; (void)p; (void)d; }
void fontevolta_esquecer_alvo(const char *a, const char *c, int p, const char *q) { (void)a; (void)c; (void)p; (void)q; }
int scrobble_concluiu(double p, double d, int f) { (void)p; (void)d; (void)f; return 0; }
void wsenvio_emitir(const WsEvento *e) { (void)e; }
int partes_virtual(const char *a, double p, double d, double c, double *vp, double *vd, int *f) { (void)a; (void)p; (void)d; (void)c; if (vp) *vp = p; if (vd) *vd = d; if (f) *f = 0; return 0; }
const PartesLista *partes_sessao_lista(void) { return NULL; }
int partes_sessao_indice(void) { return -1; }
int partes_sessao_de(const char *a) { (void)a; return 0; }
int partes_seguinte(const PartesLista *l, int k) { (void)l; (void)k; return -1; }
int partes_numero(const PartesLista *l, int k) { (void)l; (void)k; return 0; }
int partes_principais(const PartesLista *l) { (void)l; return 0; }
int partes_barra_achar(double v, double d, int *k, double *p) { (void)v; (void)d; (void)k; (void)p; return 0; }
void partes_ponto_guardar(const char *a, const char *c, int pf, const PartesPonto *p) { (void)a; (void)c; (void)pf; (void)p; }
void partes_ponto_esquecer(const char *a, const char *c, int pf) { (void)a; (void)c; (void)pf; }
int partes_arquivo_de(const char *a, PartesPonto *p) { (void)a; (void)p; return 0; }

// ------------------------------------------------------------------ helpers
static void tecla(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  player_evento(&e);
}

static Stream mkStream(const char *rot, const char *url, int alt, long mb, int dv) {
  Stream s = {0};
  snprintf(s.provedor, sizeof s.provedor, "Addon A");
  fontefalha_addon_chave("https://addon-a.invalid/manifest.json", s.addonChave, sizeof s.addonChave);
  snprintf(s.rotulo, sizeof s.rotulo, "%s", rot);
  snprintf(s.descricao, sizeof s.descricao, "%s", rot);
  snprintf(s.url, sizeof s.url, "%s", url);
  s.altura = alt;
  s.tamanhoMB = mb;
  s.tamanhoBytes = (uint64_t)mb * 1000000ULL;
  s.dolbyVision = dv;
  s.badges = badges_detectar(rot);
  return s;
}

static void resetCenario(void) {
  farol_teste_fatos(NULL);
  farolacao_fonte_encerrar();
  farol_sessao_reiniciar();
  farolanel_zerar();
  resolverregra_falha_limpar();
  stream_definir_lista(NULL, 0);
  int desc;
  (void)stream_farol_troca_escolheu(&desc);
  c_erroTexto = "";
  c_urlAtual = "https://cdn.invalid/f0.mkv";
  c_xtreamLimitada = 0;
  c_regNegado = 0;
  c_videoFalhou = 0;
  c_videoErrIdent = 0;
  c_offline = 0;
  c_addonsN = 1;
  c_debridSemPlano = 0;
  c_debridRecusa = 0;
  c_telefoneUi = 0;
  c_androidExtDisp = 1;
  c_androidAbrirN = 0;
  c_androidAbrirUrl[0] = 0;
  c_nBotoes = 0;
  c_nAlvos = 0;
  c_ticks += 100;
}

static void desenharCartao(void) {
  c_nBotoes = 0;
  c_nAlvos = 0;
  corpoErro((GfxRect){ 0.0f, 0.0f, erroLargura(), alturaErro() }, 1.0f, NULL);
}

static void testar_veredito_3_botoes_e_fontes(void) {
  resetCenario();
  Stream l[3] = {
    mkStream("2160p Remux", "https://cdn.invalid/f0.mkv", 2160, 4000, 0),
    mkStream("1080p WEB-DL", "https://cdn.invalid/f1.mkv", 1080, 1500, 0),
    mkStream("720p WEB-DL",  "https://cdn.invalid/f2.mkv",  720,  700, 0),
  };
  stream_definir_lista(l, 3);
  stream_definir_atual(0);

  // Falha HTTP 403 real (webOS/Linux/Android): ultimoFarol e FE_ERRO_FONTE sao
  // gravados ANTES de farol_julgar(RES_S_NAO_COMECA) para que o veredito enxergue a falha atual.
  c_erroTexto = "40403 server error:40403";
  c_videoFalhou = 1;
  c_videoErrIdent = 1;
  player_abrir(0, NULL);
  player_erro_fonte();

  assert(player_fonte_falhou() == 1);
  assert(erroBotoesN() == 3);
  assert(erroBotao == 0);
  assert(erroFarolAcao == FAROL_A_PROXIMA_FONTE);
  assert(!strcmp(erroTitulo, FT_FONTE_RECUSOU_L1));
  assert(!strcmp(erroDica, FT_FONTE_RECUSOU_L2));
  assert(farol_veredito() && farol_veredito()->regra->codigo == FAROL_C_FONTE_RECUSOU);

  // Ordem no anel: FE_ERRO_FONTE antes de FE_FAROL_VEREDITO.
  FarolEvento ev[16];
  int nev = farol_eventos(ev, 16);
  int iErro = -1, iVer = -1;
  for (int i = 0; i < nev; i++) {
    if (ev[i].ev == FE_ERRO_FONTE) iErro = i;
    if (ev[i].ev == FE_FAROL_VEREDITO) iVer = i;
  }
  assert(iErro >= 0 && iVer > iErro);

  // Desenho: 3 botoes com o primario em foco (0).
  desenharCartao();
  assert(c_nBotoes == 3);
  assert(!strcmp(c_botoes[0].rotulo, FT_ACAO_PROXIMA_FONTE) && c_botoes[0].foco == 1.0f);
  assert(!strcmp(c_botoes[1].rotulo, "Abrir Fontes") && c_botoes[1].foco == 0.0f);
  assert(!strcmp(c_botoes[2].rotulo, "Voltar") && c_botoes[2].foco == 0.0f);
  assert(c_nAlvos == 3);

  // D-pad Esquerda/Direita 0 <-> 1 <-> 2 sem beco.
  tecla(SDLK_LEFT);  assert(erroBotao == 0);
  tecla(SDLK_RIGHT); assert(erroBotao == 1);
  tecla(SDLK_RIGHT); assert(erroBotao == 2);
  tecla(SDLK_RIGHT); assert(erroBotao == 2);
  tecla(SDLK_LEFT);  assert(erroBotao == 1);
  tecla(SDLK_LEFT);  assert(erroBotao == 0);

  // OK no 0 executa farolacao_fonte_executar(FAROL_A_PROXIMA_FONTE, 0),
  // reinicia sessao e limpa erroFonte = 0.
  tecla(SDLK_RETURN);
  int escolhida = -1;
  assert(stream_farol_troca_escolheu(&escolhida) == 1 && escolhida == 1);
  assert(player_fonte_falhou() == 0);
  assert(farol_veredito() == NULL);
  assert(player_pediu_fontes() == 0);
  assert(player_quer_sair() == 0);

  // OK no 1 abre Fontes (tanto desktop quanto Android).
  player_abrir(0, NULL);
  player_erro_fonte();
  assert(erroBotoesN() == 3);
  tecla(SDLK_RIGHT); assert(erroBotao == 1);
  tecla(SDLK_RETURN);
  assert(player_pediu_fontes() == 1);
  assert(player_quer_sair() == 0);
#ifdef NV_ANDROID
  assert(player_fonte_falhou() == 0);
#endif

  // OK no 2 volta (desktop sai do player; Android #409 volta as fontes).
  player_abrir(0, NULL);
  player_erro_fonte();
  assert(erroBotoesN() == 3);
  tecla(SDLK_RIGHT);
  tecla(SDLK_RIGHT); assert(erroBotao == 2);
  tecla(SDLK_RETURN);
#ifdef NV_ANDROID
  assert(player_pediu_fontes() == 1);
  assert(player_fonte_falhou() == 0);
  assert(player_quer_sair() == 0);
#else
  assert(player_quer_sair() == 1);
  assert(player_pediu_fontes() == 0);
#endif
  player_encerrar();
  puts("ok  cartao 3 botoes: foco 0, Esquerda/Direita 0<->1<->2, OK 0/1/2");
}

static void testar_acoes_fonte_dv_dts_menor(void) {
  // SEM_DTS e MENOR via veredito real (RES_S_NAO_COMECA), mais SEM_DV no
  // executor do cartao, mais guarda quando nao ha candidata elegivel.
  struct {
    int acao;
    int expectedIdx;
    FarolFatos fx;
  } casos[2];

  memset(casos, 0, sizeof casos);
  for (int i = 0; i < 2; i++) {
    farolregra_fatos_iniciar(&casos[i].fx);
    casos[i].fx.plat = FP_WEBOS;
    casos[i].fx.addonsQtd = 1;
  }

  // 1) FONTE_SEM_DTS (AUDIO_SEM_SAIDA).
  casos[0].acao = FAROL_A_FONTE_SEM_DTS;
  casos[0].expectedIdx = 2;
  casos[0].fx.audioRecusado = 1;

  // 2) FONTE_MENOR (REDE_CURTA).
  casos[1].acao = FAROL_A_FONTE_MENOR;
  casos[1].expectedIdx = 1;
  casos[1].fx.paradas5min = 2;
  casos[1].fx.vazaoMbps = 5;
  casos[1].fx.fonteMbps = 25;

  for (int i = 0; i < 2; i++) {
    resetCenario();
    Stream l[3] = {
      mkStream("2160p DV DTS", "https://cdn.invalid/f0.mkv", 2160, 4000, 1),
      mkStream("1080p SDR DTS", "https://cdn.invalid/f1.mkv", 1080, 1200, 0),
      mkStream("1080p DV AAC",  "https://cdn.invalid/f2.mkv", 1080, 1800, 1),
    };
    stream_definir_lista(l, 3);
    stream_definir_atual(0);
    farol_teste_fatos(&casos[i].fx);
    player_abrir(0, NULL);
    player_erro_fonte();
    assert(erroBotoesN() == 3);
    assert(erroFarolAcao == casos[i].acao);
    tecla(SDLK_RETURN);
    int idx = -1;
    assert(stream_farol_troca_escolheu(&idx) == 1 && idx == casos[i].expectedIdx);
    assert(player_fonte_falhou() == 0);
    assert(farol_veredito() == NULL);
    player_encerrar();
  }

  // 3) FONTE_SEM_DV (disparado por erroFarolAcao = FAROL_A_FONTE_SEM_DV).
  resetCenario();
  {
    Stream l[3] = {
      mkStream("2160p DV DTS", "https://cdn.invalid/f0.mkv", 2160, 4000, 1),
      mkStream("1080p SDR DTS", "https://cdn.invalid/f1.mkv", 1080, 1200, 0),
      mkStream("1080p DV AAC",  "https://cdn.invalid/f2.mkv", 1080, 1800, 1),
    };
    stream_definir_lista(l, 3);
    stream_definir_atual(0);
    player_abrir(0, NULL);
    player_erro_fonte();
    erroFarolAcao = FAROL_A_FONTE_SEM_DV;
    erroBotao = 0;
    assert(erroBotoesN() == 3);
    tecla(SDLK_RETURN);
    int idx = -1;
    assert(stream_farol_troca_escolheu(&idx) == 1 && idx == 1);
    assert(player_fonte_falhou() == 0);
    assert(farol_veredito() == NULL);
    player_encerrar();
  }

  // Quando nao ha candidata elegivel, farolacao_fonte_executar devolve 0:
  // erroFonte permanece 1 e a sessao nao e reiniciada.
  resetCenario();
  Stream unica[1] = {
    mkStream("2160p DV", "https://cdn.invalid/only.mkv", 2160, 4000, 1),
  };
  stream_definir_lista(unica, 1);
  stream_definir_atual(0);
  c_erroTexto = "40403 server error:40403";
  player_abrir(0, NULL);
  player_erro_fonte();
  assert(erroBotoesN() == 3);
  tecla(SDLK_RETURN);
  int idx = -1;
  assert(stream_farol_troca_escolheu(&idx) == 0);
  assert(player_fonte_falhou() == 1);
  assert(farol_veredito() != NULL);
  player_encerrar();

  puts("ok  acoes SEM_DV, SEM_DTS, MENOR e guarda sem candidata");
}

static void testar_demais_acoes_e_toque(void) {
  // Debrid desconectou -> FAROL_A_RECONECTAR_DEBRID delegado via player_pediu_farol_acao().
  resetCenario();
  c_debridRecusa = 1;
  player_abrir(0, NULL);
  player_erro_fonte();
  assert(erroBotoesN() == 3);
  assert(erroFarolAcao == FAROL_A_RECONECTAR_DEBRID);
  assert(!strcmp(erroTitulo, FT_DEBRID_DESCONECTOU_L1));
  assert(!strcmp(erroDica, FT_DEBRID_DESCONECTOU_L2));
  tecla(SDLK_RETURN);
  assert(player_pediu_farol_acao() == FAROL_A_RECONECTAR_DEBRID);
  assert(player_pediu_farol_acao() == 0);

  // Toque / ponteiro coerente nos 3 botoes.
  desenharCartao();
  assert(c_nAlvos == 3);
  c_alvos[1].at(c_alvos[1].a, c_alvos[1].b);
#ifdef NV_ANDROID
  assert(player_fonte_falhou() == 0);
#else
  assert(erroBotao == 1);
#endif
  assert(player_pediu_fontes() == 1);
  player_encerrar();

#ifdef NV_TOUCH_UI
  resetCenario();
  c_telefoneUi = 1;
  c_debridSemPlano = 1;
  player_abrir(0, NULL);
  player_erro_fonte();
  assert(erroBotoesN() == 3);
  assert(erroFarolAcao == FAROL_A_ABRIR_DEBRID);
  desenharCartao();
  // O bloco touch usa 1 alvo de fundo + 3 botoes = 4 alvos.
  assert(c_nAlvos == 3);
  c_alvos[0].at(c_alvos[0].a, c_alvos[0].b);
  assert(player_pediu_farol_acao() == FAROL_A_ABRIR_DEBRID);
  player_encerrar();
#endif

  // Player externo (Android + externoDisponivel).
  resetCenario();
  FarolFatos fx;
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_ANDROID;
  fx.addonsQtd = 1;
  fx.erro.classe = FC_FORMATO;
  fx.falhasSeguidas = 2;
  fx.externoDisponivel = 1;
  farol_teste_fatos(&fx);
  player_abrir(0, NULL);
  farolFonteRegistrar("https://cdn.invalid/video.mkv", 0);
  c_videoFalhou = 1;
  c_videoErrIdent = 1;
  player_erro_fonte();
  assert(erroBotoesN() == 3);
  assert(erroFarolAcao == FAROL_A_PLAYER_EXTERNO);
  assert(!strcmp(erroTitulo, FT_TV_FORMATO_REPETE_L1));
  assert(!strcmp(erroDica, FT_L2_PLAYER_EXTERNO));
#ifdef NV_ANDROID
  tecla(SDLK_RETURN);
  assert(c_androidAbrirN == 1);
  assert(strcmp(c_androidAbrirUrl, "https://cdn.invalid/video.mkv") == 0);
  assert(player_fonte_falhou() == 0);
#endif
  player_encerrar();

  // Addon nenhum -> acao e FAROL_A_ABRIR_FONTES (excluida: fica cartao com 2 botoes).
  resetCenario();
  c_addonsN = 0;
  player_abrir(0, NULL);
  player_erro_fonte();
  assert(!strcmp(erroTitulo, FT_ADDON_NENHUM_L1));
  assert(!strcmp(erroDica, FT_ADDON_NENHUM_L2));
  assert(erroBotoesN() == 2);
  assert(erroFarolAcao == FAROL_A_NENHUMA);
  player_encerrar();

  puts("ok  demais acoes (debrid, player externo, abrir fontes) e hitbox de ponteiro");
}

static void testar_prioridades_e_legado(void) {
  // 1) Prioridade 1: Xtream "linha em uso" vence sobre o Farol (2 botoes, texto Xtream).
  resetCenario();
  c_xtreamLimitada = 1;
  c_erroTexto = "40403 server error:40403";
  player_abrir(0, NULL);
  player_erro_fonte();
  assert(!strcmp(erroTitulo, "Esta linha está em uso em outra tela"));
  assert(!strcmp(erroDica, "Feche o Xtream em outro aparelho e tente de novo."));
  assert(erroBotoesN() == 2);
  assert(erroFarolAcao == FAROL_A_NENHUMA);
  desenharCartao();
  assert(c_nBotoes == 2);
  assert(!strcmp(c_botoes[0].rotulo, "Abrir Fontes"));
  assert(!strcmp(c_botoes[1].rotulo, "Voltar"));
  player_encerrar();

  // 2) FAROL_C_NADA_ACHADO: texto legado vazio (cartao usa frases genericas) e 2 botoes.
  resetCenario();
  player_abrir(0, NULL);
  player_erro_fonte();
  assert(farol_veredito() && farol_veredito()->regra->codigo == FAROL_C_NADA_ACHADO);
  assert(erroTitulo[0] == 0 && erroDica[0] == 0);
  assert(erroBotoesN() == 2);
  assert(erroBotao == 0);
  tecla(SDLK_LEFT);  assert(erroBotao == 0);
  tecla(SDLK_RIGHT); assert(erroBotao == 1);
  tecla(SDLK_RIGHT); assert(erroBotao == 1);
  tecla(SDLK_LEFT);  assert(erroBotao == 0);
  tecla(SDLK_RETURN);
  assert(player_pediu_fontes() == 1);

  // Voltar no cartao legado (2 botoes).
  player_abrir(0, NULL);
  player_erro_fonte();
  tecla(SDLK_RIGHT); assert(erroBotao == 1);
  tecla(SDLK_RETURN);
#ifdef NV_ANDROID
  assert(player_pediu_fontes() == 1 && !player_fonte_falhou() && !player_quer_sair());
#else
  assert(player_quer_sair() == 1 && !player_pediu_fontes());
#endif
  player_encerrar();

  // 3) player_erro_fonte_motivo preserva texto explicito e 2 botoes.
  resetCenario();
  c_erroTexto = "40403 server error:40403";
  player_abrir(0, NULL);
  player_erro_fonte_motivo("Motivo explicito", "Dica explicita");
  assert(!strcmp(erroTitulo, "Motivo explicito"));
  assert(!strcmp(erroDica, "Dica explicita"));
  assert(erroBotoesN() == 2);
  player_encerrar();

#ifdef NV_ANDROID
  // 4) Android #409: teclas Back em cartao com 2 ou 3 botoes sempre voltam as fontes sem sair.
  SDL_Keycode backs[] = { SDLK_AC_BACK, SDLK_ESCAPE, SDLK_BACKSPACE, SDLK_DELETE };
  for (size_t i = 0; i < sizeof backs / sizeof backs[0]; i++) {
    resetCenario();
    c_erroTexto = "40403 server error:40403";
    player_abrir(0, NULL);
    player_erro_fonte();
    assert(erroBotoesN() == 3);
    tecla(backs[i]);
    assert(player_pediu_fontes() == 1);
    assert(player_fonte_falhou() == 0);
    assert(player_quer_sair() == 0);
    player_encerrar();
  }
#endif

  puts("ok  Xtream prioridade 1, NADA_ACHADO legado, motivo explicito e Android #409");
}

int main(void) {
  memset(&c_catItem, 0, sizeof c_catItem);
  snprintf(c_catItem.imdb, sizeof c_catItem.imdb, "tt0210000");
  snprintf(c_catItem.tipo, sizeof c_catItem.tipo, "movie");
  snprintf(c_catItem.titulo, sizeof c_catItem.titulo, "Filme Teste");

  fontefalha_dir(getenv("NUVIO_DADOS"));

  testar_veredito_3_botoes_e_fontes();
  testar_acoes_fonte_dv_dts_menor();
  testar_demais_acoes_e_toque();
  testar_prioridades_e_legado();

  puts("farol_cartao: PASS");
  return 0;
}
