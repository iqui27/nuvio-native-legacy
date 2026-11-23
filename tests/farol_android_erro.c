// Prova dirigida do BLOQUEIO r4 item 1 (FAROL6E):
// Vincula a notificacao de erro Android (Kotlin -> JNI -> video_android.c) a
// identidade (geracao) da carga desde nativeErro ate o consumo em video_bombear
// e leitura por video_erro_texto / video_erro_decoder_codigo / player_erro_fonte,
// mantendo atomicidade entre identidade, codigo e texto sob mutex e descartando
// erro obsoleto de A antes de autorizar/despachar B no Resolver.
#define NV_ANDROID 1
#include <SDL2/SDL.h>
void *SDL_AndroidGetJNIEnv(void);
void *SDL_AndroidGetActivity(void);
#define NV_TICKS_H
#include <stdatomic.h>
#include <stdint.h>
static uint64_t nv_ticks64(void);

#include "../src/video_android.c"

// Stubs minimos exigidos por video_android.c (como em video_android_retomada.c).
int juntos_precarregando(void) { return 0; }
int player_eh_canal(void) { return 0; }
int stream_kbps_atual(void) { return 0; }
const Stream *stream_fonte_reproducao(const char *u, const char *c) { (void)u; (void)c; return NULL; }
int stream_kbps(const Stream *s) { (void)s; return 0; }
int xtream_url_limitada(const char *u) { (void)u; return 0; }
int xtream_conexoes_limitadas(void) { return 0; }
int xtream_conta_ler(XtreamConta *c) { memset(c, 0, sizeof *c); return 0; }
void marco(const char *s) { (void)s; }
const char *ling_audio(void) { return ""; }
const char *ling_audio_preferido(void) { return ""; }
int ajustes_audio_normalizar(void) { return 0; }
void tex_pressao_reproducao(int l) { (void)l; }
void cacheboost_cache_relato(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
void cacheboost_ganho_relato(int g) { (void)g; }
int cacheboost_volume(void) { return 100; }
void audsync_backend(void (*fn)(int)) { (void)fn; }
void audsync_formato(int f) { (void)f; }
int audsync_pcm(const int16_t *b, int n, int64_t p) { (void)b; (void)n; (void)p; return 0; }
void streamfitpassiva_entregar(long long r, int g, const char *o, const int *k, int n, long long f) {
  (void)r; (void)g; (void)o; (void)k; (void)n; (void)f;
}
void stream_definir_tela(int h, int d) { (void)h; (void)d; }
void stream_definir_audio_capacidades(int a, int b, int c, int d, int e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
void stream_definir_decoder4k(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
int ajustes_tunneling(void) { return 0; }
int barraspretas_ativo(void) { return 0; }
int ajustes_afr(void) { return 0; }
void audioesc_fonte_binge(const char *b) { (void)b; }
int audioesc_escolher(const AudioCand *c, int n, int *o) { (void)c; (void)n; (void)o; return -1; }
const char *audioesc_origem_nome(int o) { (void)o; return ""; }
void audioinfo_texto_noop(void) {}
int audioinfo_texto(const char *m, int ch, int a, char *dst, size_t n) {
  (void)m; (void)ch; (void)a; if (dst && n) dst[0] = 0; return 0;
}
int ling_tipo_legenda(const char *n, int f, int s) { (void)n; (void)f; (void)s; return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t; return NULL; }
const char *ling_nome(const char *c) { return c ? c : ""; }
void vel_log(int v, const char *m) { (void)v; (void)m; }
void video_taxa_recusar(void) {}

#include "resolver.h"
#include "resolverregra.h"
#include "farolregra.h"
#include "farolerro.h"
#include "farolcod.h"
#include "farolanel.h"
#include "faroltexto.h"
#include "fontesonda.h"
#include "extplayer.h"
#include "addonguia.h"
#include "addonmodo.h"
#include "addons.h"
#include "catalogo.h"
#include "avisos.h"
#include "celbotao.h"
#include "ilha.h"
#include "p2pmotor.h"
#include "perfis.h"
#include "plrilha.h"
#include "gfx.h"
#include "text.h"
#include "ponteiro.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Fake JNI & clock
#ifdef NV_NDK_JNI
static struct JNINativeInterface jni;
static const struct JNINativeInterface *envPtr = &jni;
#else
static struct JNINativeInterface_ jni;
static const struct JNINativeInterface_ *envPtr = &jni;
#endif
static _Atomic unsigned relogioMs = 1000;
static _Atomic int geracaoAberta;

void *SDL_AndroidGetJNIEnv(void) { return &envPtr; }
void *SDL_AndroidGetActivity(void) { return NULL; }
Uint32 SDL_GetTicks(void) { return atomic_load(&relogioMs); }
Uint64 SDL_GetTicks64(void) { return atomic_load(&relogioMs); }
static uint64_t nv_ticks64(void) { return atomic_load(&relogioMs); }
SDL_mutex *SDL_CreateMutex(void) { return (SDL_mutex *)(uintptr_t)1; }
int SDL_LockMutex(SDL_mutex *m) { (void)m; return 0; }
int SDL_UnlockMutex(SDL_mutex *m) { (void)m; return 0; }

static jobject JNICALL jni_global(JNIEnv *e, jobject o) { (void)e; return o; }
static jmethodID JNICALL jni_metodo(JNIEnv *e, jclass c, const char *nome, const char *sig) {
  (void)e; (void)c; (void)sig;
  if (!strcmp(nome, "abrir")) return (jmethodID)(uintptr_t)10;
  if (!strcmp(nome, "abrirPosicao")) return (jmethodID)(uintptr_t)11;
  if (!strcmp(nome, "parar")) return (jmethodID)(uintptr_t)12;
  return (jmethodID)(uintptr_t)1;
}
static int g_recusarNewString;
static jstring JNICALL jni_texto(JNIEnv *e, const jchar *u, jsize n) {
  if (g_recusarNewString > 0) {
    g_recusarNewString--;
    return NULL;
  }
  char *t = malloc((size_t)n + 1);
  (void)e;
  for (jsize i = 0; i < n; i++) t[i] = (char)u[i];
  t[n] = 0;
  return (jstring)t;
}
static void JNICALL jni_apagar(JNIEnv *e, jobject o) { (void)e; free(o); }
static jboolean JNICALL jni_temExc(JNIEnv *e) { (void)e; return 0; }
static void JNICALL jni_limparExc(JNIEnv *e) { (void)e; }
static void JNICALL jni_chamar(JNIEnv *e, jclass c, jmethodID m, ...) {
  va_list ap;
  (void)e; (void)c;
  va_start(ap, m);
  if ((uintptr_t)m == 11) {
    (void)va_arg(ap, jstring);
    (void)va_arg(ap, jstring);
    (void)va_arg(ap, jint);
    atomic_store(&geracaoAberta, (int)va_arg(ap, jint));
  }
  va_end(ap);
}

// Estado espelhado de src/player.c / src/app.c para o puente real do Farol.
static int aberto, saindo, pediuSair, mini;
static int temFixo, canalSessao, epT, epE, esperandoFonte, erroFonte, visivel, soBarra, erroBotao;
static CatItem itemFixo, itemCanal;
static uint64_t itemSessao = 1, c_perfilSessao = 1;
static FarolErro ultimoFarol;
static int temUltimoFarol, ultimoFarolDebrid;
static uint64_t ultimoFarolAddonId;
static uint64_t farolTentativaSeq, ultimaFonteTentativa, ultimoFarolTentativa;
static char ultimaFonteUrl[NV_MEDIA_URL_MAX];
static int ultimaFonteTemCabs, ultimaFonteTemItem, ultimaFonteIdx, ultimaFonteEpT, ultimaFonteEpE;
static CatItem ultimaFonteItem;
static uint64_t ultimaFonteSessao;
static char erroTitulo[160], erroDica[160], linhaEp[220];
static int c_tocarInternoN;
static char c_androidApps[256] = "org.videolan.vlc\tVLC\n";
static int c_androidAbrirOk = 1, c_androidAbrirN;
static char c_androidAbrirUrl[NV_MEDIA_URL_MAX], c_androidAbrirTit[160];
static int c_envioChamadas;
static AvisosEnvio c_envio;
static Stream c_stream;

static const CatItem *item(void) { return canalSessao ? &itemCanal : (temFixo ? &itemFixo : NULL); }
static int idxAtual(void) { return 0; }
int cat_indice_titulo(const char *imdb, int p) { (void)imdb; return p; }
int player_aberto(void) { return aberto; }
static void tocarFonteInterno(const char *url) { (void)url; c_tocarInternoN++; }
static int nv_perfil_sessao_atual(uint64_t s) { return s == c_perfilSessao; }
static int travaui_segura(const char *imdb, int ix, int t, int e, int *p, int *n) {
  (void)imdb; (void)ix; (void)t; (void)e; (void)p; if (n) *n = 0; return 0;
}
void player_erro_fonte_motivo(const char *titulo, const char *dica) { (void)titulo; (void)dica; }

const char *i18n(const char *s) { return s; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *v) { (void)n; (void)v; return 1; }
int rede_saude_offline(void) { return 0; }
int addons_n(void) { return 1; }
int addons_ocupado(void) { return 0; }
int addons_ativo(int i) { (void)i; return 1; }
int addons_sondado(int i) { (void)i; return 1; }
const char *addons_base(int i) { (void)i; return "https://addon.example"; }
const char *addons_nome(int i) { (void)i; return "Addon"; }
int addons_fornece(int i, int rec) { (void)i; return rec == ADD_STREAM; }
int addons_motivo_vazio(char *d, unsigned n) { (void)d; (void)n; return 0; }
int debrid_sem_plano(void) { return 0; }
const char *debrid_sem_plano_frase(int m) { (void)m; return NULL; }
int debrid_recusa(char *d, unsigned n) { (void)d; (void)n; return 0; }
int debrid_ativo(void) { return 1; }
void player_toast(const char *t, unsigned ms) { (void)t; (void)ms; }
void player_abrir(int idx, const char *url) { (void)idx; (void)url; }
void player_definir_episodio(int t, int e) { (void)t; (void)e; }
void player_voltar_a_esperar(void) {}
char *android_listar_players(void) {
  char *s = malloc(strlen(c_androidApps) + 1);
  if (s) strcpy(s, c_androidApps);
  return s;
}
int android_abrir_externo(const char *url, const char *tit, const char *pkg) {
  (void)pkg;
  c_androidAbrirN++;
  snprintf(c_androidAbrirUrl, sizeof c_androidAbrirUrl, "%s", url ? url : "");
  snprintf(c_androidAbrirTit, sizeof c_androidAbrirTit, "%s", tit ? tit : "");
  return c_androidAbrirOk;
}
int ajustes_player_externo(int a) { (void)a; return EXTPLAYER_INTERNO; }
const char *ajustes_player_pacote(int a) { (void)a; return ""; }
int ajustes_debrid_celular(const char *s, const char *c) { (void)s; (void)c; return 1; }
void ajustes_definir_p2p_ligado(int o) { (void)o; }
int ajustes_p2p_ligado(void) { return 0; }
const char *ajustes_p2p_url(void) { return ""; }
int ajustes_idioma(void) { return 0; }
int ajustes_vidro(void) { return 0; }
int ajustes_animacoes_reduzidas(void) { return 1; }
int ajustes_animacoes_reduzidas_usuario(void) { return 1; }
void ajustes_acento(float *r, float *g, float *b) { *r = *g = *b = 1.0f; }
float ajustes_acento_tinta(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
int ajustes_tinta_foco(void) { return 255; }
int ajustes_tinta_foco2(void) { return 200; }
void ajustes_abrir_na_espera_fonte(void) {}
void ajustes_abrir_fileiras(void) {}
int ajustes_iniciar(void) { return 0; }
int celb_disponivel(void) { return 0; }
int celb_embutido(int d) { (void)d; return 0; }
int celb_abrir_embutido(int d, const char *t) { (void)d; (void)t; return 0; }
void celb_fechar_dono(int d) { (void)d; }
int celb_pegar(int d, char *dst, size_t n) { (void)d; (void)dst; (void)n; return 0; }
void celb_desenhar_em(int d, GfxRect r, float a) { (void)d; (void)r; (void)a; }
float celb_qr_lado(void) { return 100.0f; }
void celb_qr_desenhar(const char *u, float x, float y, float a) { (void)u; (void)x; (void)y; (void)a; }
void ilha_avisar(const char *k, int t, const char *i, const char *tx, unsigned m, int tc) {
  (void)k; (void)t; (void)i; (void)tx; (void)m; (void)tc;
}
void ilha_avisar_ex(const IlhaAvisoEx *a) { (void)a; }
int ilha_tem(const char *k) { (void)k; return 0; }
int ilha_modal_aberto(void) { return 0; }
int ilha_modal_visivel(void) { return 0; }
int stream_atual(void) { return -1; }
const Stream *stream_item(int i) { return i >= 0 ? &c_stream : NULL; }
int stream_ordem_addon(int i) { (void)i; return -1; }
int stream_proxima_vod(int i) { (void)i; return -1; }
int stream_incompativel(int i) { (void)i; return 0; }
int stream_automatico_excluir(int i) { (void)i; return 0; }
void stream_folha_abrir(void) {}
long memlog_livre_mb(void) { return -1; }
int avisos_envio_info(AvisosEnvio *o) { *o = c_envio; c_envioChamadas++; return 1; }
int p2p_ativo(void) { return 0; }
void p2p_cancelar(void) {}
int p2pmotor_disponivel(void) { return 1; }
void p2pmotor_estado(P2pmEstado *e) { memset(e, 0, sizeof *e); }
void p2pmotor_cancelar(void) {}
const char *nuvem_base_login(void) { return ""; }
const char *perfis_dono(void) { return "u"; }
int perfis_ativo_addons(void) { return 0; }
const ContaPerfil *perfis_item_ativo(void) { return NULL; }
const char *sessao_usuario(void) { return "u"; }
int sync_manual_addons(void) { return 0; }
int sync_manual_addons_rodando(void) { return 0; }
int plrilha_centro_visivel(void) { return 0; }
void plrilha_pedir(const PlrIlhaPedido *p) { (void)p; }
void plrilha_desenhar(Uint32 a) { (void)a; }
void plrui_botao_repouso(GfxRect r, float a) { (void)r; (void)a; }
void plrui_linha_foco(GfxRect r, float ra, float f) { (void)r; (void)ra; (void)f; }
void plrui_material(GfxRect r, float ra, int m, float a) { (void)r; (void)ra; (void)m; (void)a; }
void addonsui_ponteiro(int a, int b) { (void)a; (void)b; }
void rec_avatar_estilo(GfxRect r, const char *u, const char *n, const char *f, float a, TxtEstilo e) {
  (void)r; (void)u; (void)n; (void)f; (void)a; (void)e;
}
float gfx_opacidade_grupo = 1.0f;
int gfx_vidro_sem_aro = 0;
float botao_largura(const char *r, const char *i, int p) { (void)r; (void)i; (void)p; return 100.0f; }
void botao_pilula(GfxRect r, const char *ro, const char *i, float f, int p, int al, float a) {
  (void)r; (void)ro; (void)i; (void)f; (void)p; (void)al; (void)a;
}
float botao_cor_foco(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
void botao_luz(GfxRect r, float f, float a) { (void)r; (void)f; (void)a; }
void gfx_cor(GfxRect r, float ra, float cr, float cg, float cb, float ca) { (void)r; (void)ra; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_anel(GfxRect r, float ra, float e, float cr, float cg, float cb, float ca) { (void)r; (void)ra; (void)e; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) { (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_luz_canto(GfxRect r, float ra, float lx, float ly, float lr, float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)lx; (void)ly; (void)lr; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro(GfxRect r, float ra, int t, float c, float d, float a) { (void)r; (void)ra; (void)t; (void)c; (void)d; (void)a; }
float gfx_vidro_opacidade(void) { return 1.0f; }
void gfx_sombra_sob(GfxRect s, float f, float px, float r, float cr, float cg, float cb, float a, GfxRect rc, float rr, float ra) {
  (void)s; (void)f; (void)px; (void)r; (void)cr; (void)cg; (void)cb; (void)a; (void)rc; (void)rr; (void)ra;
}
void gfx_transformar(float x, float y, float s, float dx, float dy) { (void)x; (void)y; (void)s; (void)dx; (void)dy; }
void gfx_sem_transformar(void) {}
float gfx_escala_ui(void) { return 1.0f; }
float gfx_escala(void) { return 1.0f; }
float gfx_escala_entrar(void) { return 1.0f; }
void gfx_escala_sair(float a) { (void)a; }
float gfx_escala_txt(void) { return 1.0f; }
float gfx_escala_txt_entrar(void) { return 1.0f; }
int ponteiro_ativo(void) { return 0; }
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn fo, PonteiroFn at, int a, int b) {
  (void)x; (void)y; (void)w; (void)h; (void)fo; (void)at; (void)a; (void)b;
}
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; return l;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float mw) {
  TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; (void)mw; return l;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml; return 0.0f;
}
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml; return 0.0f;
}
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float a, float t) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)t; return 0.0f;
}
static void menu_fechar(void) {}
static void menu_definir_destino(int d) { (void)d; }
static void trocarTela(int n) { (void)n; }
int cat_indice_reabrir(int idx, const CatItem *c) { (void)idx; (void)c; return -1; }
enum { TELA_AJUSTES = 1, MENU_AJUSTES = 5 };
static int tela = TELA_AJUSTES;
#define NV_FAROL_PLAT FP_ANDROID
#define NV_FAROL_BACKEND "media3"

#include "farol_android_bridge.inc"

static void abrir_titulo(const char *imdb, const char *tipo, const char *titulo,
                         int t, int e, const char *url, unsigned *outGen) {
  aberto = 1;
  temFixo = 1;
  canalSessao = 0;
  memset(&itemFixo, 0, sizeof itemFixo);
  snprintf(itemFixo.imdb, sizeof itemFixo.imdb, "%s", imdb);
  snprintf(itemFixo.tipo, sizeof itemFixo.tipo, "%s", tipo);
  snprintf(itemFixo.titulo, sizeof itemFixo.titulo, "%s", titulo);
  epT = t; epE = e;
  farolTentativaIniciar();
  farolFonteRegistrar(url, 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  assert(video_tocar_retomada(url, 0.0, 0.0));
  *outGen = video_android_sessao();
  assert(*outGen != 0);
}

static void apertar_ok_app(void) {
  SDL_Event ev = { .type = SDL_KEYDOWN };
  ev.key.keysym.sym = SDLK_RETURN;
  app_resolver_despachar(&ev);
}

static void zerar_farol(void) {
  aberto = 0; temFixo = 0; canalSessao = 0;
  temUltimoFarol = 0; ultimoFarolDebrid = 0; ultimoFarolAddonId = 0;
  farolTentativaSeq = ultimaFonteTentativa = ultimoFarolTentativa = 0;
  ultimaFonteUrl[0] = 0; ultimaFonteTemCabs = 0; ultimaFonteTemItem = 0;
  c_androidAbrirN = 0; c_androidAbrirUrl[0] = 0; c_androidAbrirTit[0] = 0;
  c_envioChamadas = 0; c_tocarInternoN = 0;
  farolanel_zerar();
  resolverregra_falha_limpar();
  resolver_teste_esquecer();
}

typedef struct {
  unsigned genA;
  unsigned genB;
  _Atomic int pubValida;
  _Atomic int parar;
} CorridaCtx;

static void *fio_jni_corrida(void *arg) {
  CorridaCtx *ctx = arg;
  int i = 0;
  while (!atomic_load_explicit(&ctx->parar, memory_order_acquire)) {
    unsigned g = (i & 1) ? ctx->genA : ctx->genB;
    int cod = (i & 1) ? 4003 : 2004;
    int tipo = (i & 1) ? 2 : 0;
    Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)g, cod, tipo);
    if (g == ctx->genB)
      atomic_store_explicit(&ctx->pubValida, 1, memory_order_release);
    i++;
  }
  return NULL;
}

typedef struct {
  _Atomic unsigned genAtual;
  _Atomic int parar;
} TransicaoCtx;

static void *fio_jni_transicao(void *arg) {
  TransicaoCtx *ctx = arg;
  int i = 0;
  while (!atomic_load_explicit(&ctx->parar, memory_order_acquire)) {
    unsigned g = atomic_load_explicit(&ctx->genAtual, memory_order_acquire);
    if (g) {
      if (i & 1) {
        unsigned obsoleta = g > 1 ? g - 1 : 99999u;
        Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)obsoleta, 2004, 0);
      } else {
        Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)g, 4003, 2);
      }
      if ((i & 7) == 0)
        Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TOCANDO, 0, 0);
    }
    i++;
  }
  return NULL;
}

typedef struct {
  unsigned gen;
  int legado;
  int cod;
  int tipo;
  const char *esperadoTok;
  const char *esperadoTxt;
  int esperadoDec;
  int esperadoId;
  int disparado;
} IntercalarCtx;

static IntercalarCtx g_intercalar;

void __real_farol_ev(FarolMod mod, int ev, int sev, int32_t a, int32_t b,
                     const char *tok);

static void *fio_jni_intercalar(void *arg) {
  IntercalarCtx *c = arg;
  if (c->legado)
    Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, c->cod, c->tipo);
  else
    Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)c->gen, c->cod, c->tipo);
  return NULL;
}

void __wrap_farol_ev(FarolMod mod, int ev, int sev, int32_t a, int32_t b,
                     const char *tok) {
  if (ev == FE_ERRO_FONTE && g_intercalar.esperadoTok) {
    IntercalarCtx c = g_intercalar;
    pthread_t th;
    g_intercalar.esperadoTok = NULL;
    g_intercalar.disparado = 1;
    assert(tok && !strcmp(tok, c.esperadoTok));
    assert(video_erro_identificado() == c.esperadoId);
    assert(pthread_create(&th, NULL, fio_jni_intercalar, &c) == 0);
    assert(pthread_join(th, NULL) == 0);
    // O snapshot consumido deve permanecer imutavel durante o diagnostico/autorizacao;
    // o evento novo pendente nao pode ser classificado antes de seu proprio consumo.
    if (c.gen == video_android_sessao() || c.legado)
      assert(erroPendente() == 1);
    else
      assert(erroPendente() == 0);
    assert(video_falhou() == 1);
    assert(video_erro_identificado() == c.esperadoId);
    assert(!strcmp(video_erro_texto(), c.esperadoTxt));
    assert(video_erro_decoder_codigo() == c.esperadoDec);
  }
  __real_farol_ev(mod, ev, sev, a, b, tok);
}

int main(void) {
  jni.NewGlobalRef = jni_global;
  jni.GetStaticMethodID = jni_metodo;
  jni.NewString = jni_texto;
  jni.DeleteLocalRef = jni_apagar;
  jni.ExceptionCheck = jni_temExc;
  jni.ExceptionClear = jni_limparExc;
  jni.CallStaticVoidMethod = jni_chamar;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeIniciar(&envPtr, (jclass)(uintptr_t)3);

  // 1) BLOQUEIO r4 item 1: erro tardio de A chega via nativeErro(genA, 4003, 2)
  // DEPOIS de B iniciar abertura. Deve ser descartado antes de falhar/diagnosticar.
  zerar_farol();
  unsigned genA = 0, genB = 0;
  abrir_titulo("tt1000001", "movie", "A", 0, 0, "https://example.com/a.mkv", &genA);
  abrir_titulo("tt2000002", "movie", "B", 0, 0, "https://example.com/b.mkv", &genB);
  assert(genB != genA);

  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genA, 4003, 2);
  video_bombear();
  assert(!video_falhou());
  assert(video_erro_texto()[0] == 0);
  assert(video_erro_decoder_codigo() == 0);
  assert(player_externo_disponivel() == 0);

  // Mesmo se o usuario abrir o Resolver agora, B NAO pode ser oferecido nem despachado.
  aberto = 0;
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_prim() != FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);

  // 2) Erro valido da carga corrente (genB) eh aceito e chega ao player externo.
  zerar_farol();
  abrir_titulo("tt2000002", "movie", "B", 0, 0, "https://example.com/b.mkv", &genB);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genB, 4003, 2);
  video_bombear();
  assert(video_falhou());
  assert(!strcmp(video_erro_texto(), "4003 player error:2"));
  assert(video_erro_decoder_codigo() == 4003);
  player_erro_fonte();
  aberto = 0;
  assert(player_externo_disponivel() == 1);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(!resolver_aberto());
  assert(c_androidAbrirN == 1);
  assert(!strcmp(c_androidAbrirUrl, "https://example.com/b.mkv"));
  assert(!strcmp(c_androidAbrirTit, "B"));

  // 3) video_parar() invalida genB: erro tardio apos parar eh descartado.
  video_parar();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genB, 4003, 2);
  video_bombear();
  assert(video_erro_texto()[0] == 0);
  assert(video_erro_decoder_codigo() == 0);

  // 4) Corrida concorrente entre fio JNI (nativeErro genA=4003/video vs genB=2004/rede)
  // e fio SDL (video_bombear + leitura de video_erro_texto/codigo): espera a primeira
  // publicacao valida antes de encerrar o consumo; zero corrida em TSan e identidade/
  // codigo/texto sempre coerentes com genB.
  zerar_farol();
  abrir_titulo("tt3000003", "movie", "A_corrida", 0, 0, "https://example.com/a2.mkv", &genA);
  abrir_titulo("tt4000004", "movie", "B_corrida", 0, 0, "https://example.com/b2.mkv", &genB);
  CorridaCtx ctx = { .genA = genA, .genB = genB, .pubValida = 0, .parar = 0 };
  pthread_t th;
  assert(pthread_create(&th, NULL, fio_jni_corrida, &ctx) == 0);
  {
    struct timespec t0, tNow;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (!atomic_load_explicit(&ctx.pubValida, memory_order_acquire)) {
      sched_yield();
      clock_gettime(CLOCK_MONOTONIC, &tNow);
      assert(tNow.tv_sec - t0.tv_sec < 10);
    }
  }
  int viuFalhaB = 0;
  for (int i = 0; i < 4000; i++) {
    atomic_fetch_add(&relogioMs, 1);
    video_bombear();
    if (video_falhou()) {
      const char *txt = video_erro_texto();
      int dec = video_erro_decoder_codigo();
      assert(!strcmp(txt, "2004 player error:0"));
      assert(dec == 0);
      assert(video_erro_identificado() == 1);
      viuFalhaB = 1;
      break;
    }
    sched_yield();
  }
  atomic_store_explicit(&ctx.parar, 1, memory_order_release);
  pthread_join(th, NULL);
  assert(viuFalhaB);

  // 5) FAROL6-r5 achado 3: evento legado EV_ERRO sem geracao preserva feedback e
  // compatibilidade de erro/diagnostico, mas NAO pode autorizar player externo
  // para tentativa B que nao identifica.
  zerar_farol();
  abrir_titulo("tt5000005", "movie", "A_legado", 0, 0, "https://example.com/a_leg.mkv", &genA);
  abrir_titulo("tt6000006", "movie", "B_legado", 0, 0, "https://example.com/b_leg.mkv", &genB);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 4003, 2);
  video_bombear();
  assert(video_falhou());
  assert(!strcmp(video_erro_texto(), "4003 player error:2"));
  assert(video_erro_decoder_codigo() == 4003);
  assert(video_erro_identificado() == 0);
  player_erro_fonte();
  {
    FarolErro fe;
    assert(player_ultimo_erro_farol(&fe, NULL));
    assert(fe.classe == FC_FORMATO);
  }
  assert(player_ultimo_erro_tentativa() == 0);
  assert(player_externo_disponivel() == 0);
  aberto = 0;
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  assert(resolver_teste_prim() != FAROL_A_PLAYER_EXTERNO);
  assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);
  assert(c_envioChamadas >= 1);
  assert(player_abrir_externo() == 0);
  assert(c_androidAbrirN == 0);

  // 6) FAROL6-r5 achado 2: transicao concorrente abrir/bombear/consumir/video_parar
  // com entrega JNI simultanea (ativo + sessao + erro + tocando sob TSan).
  zerar_farol();
  TransicaoCtx tctx = { .genAtual = 0, .parar = 0 };
  pthread_t thTrans;
  assert(pthread_create(&thTrans, NULL, fio_jni_transicao, &tctx) == 0);
  for (int ciclo = 0; ciclo < 400; ciclo++) {
    unsigned g = 0;
    abrir_titulo("tt7000007", "movie", "Trans", 0, 0, "https://example.com/trans.mkv", &g);
    atomic_store_explicit(&tctx.genAtual, g, memory_order_release);
    for (int passo = 0; passo < 3; passo++) {
      atomic_fetch_add(&relogioMs, 1);
      video_bombear();
      (void)video_ativo();
      (void)video_tocando();
      if (video_falhou()) {
        const char *txt = video_erro_texto();
        int dec = video_erro_decoder_codigo();
        int id = video_erro_identificado();
        assert(id == 1);
        assert(!strcmp(txt, "4003 player error:2"));
        assert(dec == 4003);
        break;
      }
      sched_yield();
    }
    video_parar();
    assert(!video_ativo());
    assert(video_erro_texto()[0] == 0);
    assert(video_erro_decoder_codigo() == 0);
    assert(video_erro_identificado() == 0);
  }
  atomic_store_explicit(&tctx.parar, 1, memory_order_release);
  pthread_join(thTrans, NULL);

  // 7) FAROL6-r6: intercalamento deterministico legado (EV_ERRO 4003 consumido)
  // seguido de nativeErro(genB, 2004, 0) durante FE_ERRO_FONTE antes da leitura de
  // identidade. O erro consumido preserva diagnostico m3_4003 e recusa player externo;
  // o erro 2004 pendente retem codigo/tipo/geracao/proveniencia e eh consumido depois.
  zerar_farol();
  abrir_titulo("tt8000001", "movie", "A intercalado", 0, 0,
               "https://example.invalid/a_r6.mkv", &genA);
  abrir_titulo("tt8000002", "movie", "B intercalado", 0, 0,
               "https://example.invalid/b_r6.mkv", &genB);
  assert(genA != genB);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 4003, 2);
  video_bombear();
  assert(video_falhou() && !video_erro_identificado());
  assert(!strcmp(video_erro_texto(), "4003 player error:2"));
  assert(video_erro_decoder_codigo() == 4003);
  g_intercalar = (IntercalarCtx){
    .gen = genB, .legado = 0, .cod = 2004, .tipo = 0,
    .esperadoTok = "m3_4003", .esperadoTxt = "4003 player error:2",
    .esperadoDec = 4003, .esperadoId = 0, .disparado = 0
  };
  player_erro_fonte();
  assert(g_intercalar.disparado == 1 && g_intercalar.esperadoTok == NULL);
  {
    FarolErro fe;
    assert(player_ultimo_erro_farol(&fe, NULL));
    assert(fe.classe == FC_FORMATO);
    assert(!strcmp(fe.cod, "m3_4003"));
  }
  assert(player_ultimo_erro_tentativa() == 0);
  assert(player_externo_disponivel() == 0);
  aberto = 0;
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);
  assert(c_envioChamadas >= 1);
  assert(player_abrir_externo() == 0);
  assert(c_androidAbrirN == 0);

  // Prova de consumo proprio do segundo evento (2004 pendente de genB):
  assert(erroPendente() == 1);
  video_bombear();
  assert(erroPendente() == 0);
  assert(video_falhou() == 1);
  assert(!strcmp(video_erro_texto(), "2004 player error:0"));
  assert(video_erro_decoder_codigo() == 0);
  assert(video_erro_identificado() == 1);
  player_erro_fonte();
  {
    FarolErro fe;
    assert(player_ultimo_erro_farol(&fe, NULL));
    assert(fe.classe == FC_HTTP);
    assert(!strcmp(fe.cod, "m3_2004"));
  }
  assert(player_ultimo_erro_tentativa() != 0);
  assert(player_externo_disponivel() == 1);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_FONTE_RECUSOU);
  assert(resolver_teste_prim() != FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);

  // 8a) Intercalamento inverso (controle positivo): falha identificada legítima
  // nativeErro(genB, 4003, 2) consumida nao eh corrompida por nativeErro(genB, 2004, 0)
  // pendente durante FE_ERRO_FONTE; despacho abre B e 2004 eh consumido depois.
  zerar_farol();
  abrir_titulo("tt8000003", "movie", "A inverso", 0, 0,
               "https://example.invalid/a_inv.mkv", &genA);
  abrir_titulo("tt8000004", "movie", "B inverso", 0, 0,
               "https://example.invalid/b_inv.mkv", &genB);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genB, 4003, 2);
  video_bombear();
  assert(video_falhou() && video_erro_identificado() == 1);
  g_intercalar = (IntercalarCtx){
    .gen = genB, .legado = 0, .cod = 2004, .tipo = 0,
    .esperadoTok = "m3_4003", .esperadoTxt = "4003 player error:2",
    .esperadoDec = 4003, .esperadoId = 1, .disparado = 0
  };
  player_erro_fonte();
  assert(g_intercalar.disparado == 1);
  {
    FarolErro fe;
    assert(player_ultimo_erro_farol(&fe, NULL));
    assert(fe.classe == FC_FORMATO && !strcmp(fe.cod, "m3_4003"));
  }
  assert(player_ultimo_erro_tentativa() != 0);
  assert(player_externo_disponivel() == 1);
  aberto = 0;
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(c_androidAbrirN == 1);
  assert(!strcmp(c_androidAbrirUrl, "https://example.invalid/b_inv.mkv"));
  assert(!strcmp(c_androidAbrirTit, "B inverso"));
  assert(erroPendente() == 1);
  video_bombear();
  assert(erroPendente() == 0);
  assert(!strcmp(video_erro_texto(), "2004 player error:0"));
  assert(video_erro_decoder_codigo() == 0);
  assert(video_erro_identificado() == 1);
  player_erro_fonte();
  {
    FarolErro fe;
    assert(player_ultimo_erro_farol(&fe, NULL));
    assert(fe.classe == FC_HTTP && !strcmp(fe.cod, "m3_2004"));
  }

  // 8b) Intercalamento inverso com evento legado posterior: falha identificada
  // nativeErro(genB, 4003, 2) consumida nao eh rebaixada por EV_ERRO(2004, 0)
  // pendente; EV_ERRO mantem proveniencia=0 ao ser consumido na chamada seguinte.
  zerar_farol();
  abrir_titulo("tt8000005", "movie", "A leg inv", 0, 0,
               "https://example.invalid/a_linv.mkv", &genA);
  abrir_titulo("tt8000006", "movie", "B leg inv", 0, 0,
               "https://example.invalid/b_linv.mkv", &genB);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genB, 4003, 2);
  video_bombear();
  g_intercalar = (IntercalarCtx){
    .gen = 0, .legado = 1, .cod = 2004, .tipo = 0,
    .esperadoTok = "m3_4003", .esperadoTxt = "4003 player error:2",
    .esperadoDec = 4003, .esperadoId = 1, .disparado = 0
  };
  player_erro_fonte();
  assert(g_intercalar.disparado == 1);
  assert(player_ultimo_erro_tentativa() != 0);
  assert(player_externo_disponivel() == 1);
  aberto = 0;
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(c_androidAbrirN == 1);
  assert(!strcmp(c_androidAbrirUrl, "https://example.invalid/b_linv.mkv"));
  assert(erroPendente() == 1);
  video_bombear();
  assert(erroPendente() == 0);
  assert(!strcmp(video_erro_texto(), "2004 player error:0"));
  assert(video_erro_decoder_codigo() == 0);
  assert(video_erro_identificado() == 0);
  player_erro_fonte();
  assert(player_ultimo_erro_tentativa() == 0);

  // 8c) Inverso por tipo de erro: 2004 (HTTP) identificado consumido nao pode ser
  // sobrescrito por nativeErro(genB, 4003, 2) pendente durante FE_ERRO_FONTE para
  // autorizar player externo; na rodada seguinte 4003 eh consumido e despacha B.
  zerar_farol();
  abrir_titulo("tt8000007", "movie", "A http", 0, 0,
               "https://example.invalid/a_http.mkv", &genA);
  abrir_titulo("tt8000008", "movie", "B http", 0, 0,
               "https://example.invalid/b_http.mkv", &genB);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genB, 2004, 0);
  video_bombear();
  g_intercalar = (IntercalarCtx){
    .gen = genB, .legado = 0, .cod = 4003, .tipo = 2,
    .esperadoTok = "m3_2004", .esperadoTxt = "2004 player error:0",
    .esperadoDec = 0, .esperadoId = 1, .disparado = 0
  };
  player_erro_fonte();
  assert(g_intercalar.disparado == 1);
  {
    FarolErro fe;
    assert(player_ultimo_erro_farol(&fe, NULL));
    assert(fe.classe == FC_HTTP && !strcmp(fe.cod, "m3_2004"));
  }
  assert(player_externo_disponivel() == 1);
  aberto = 0;
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_FONTE_RECUSOU);
  assert(resolver_teste_prim() != FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);
  // Agora consome o 4003 pendente com codigo/tipo/geracao/proveniencia intactos:
  assert(erroPendente() == 1);
  video_bombear();
  assert(erroPendente() == 0);
  assert(!strcmp(video_erro_texto(), "4003 player error:2"));
  assert(video_erro_decoder_codigo() == 4003);
  assert(video_erro_identificado() == 1);
  player_erro_fonte();
  assert(player_externo_disponivel() == 1);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(c_androidAbrirN == 1);
  assert(!strcmp(c_androidAbrirUrl, "https://example.invalid/b_http.mkv"));

  // 8d) Pendente identificado nao eh rebaixado por EV_ERRO legado antes do consumo,
  // e erro de geracao obsoleta (genA) intercalado durante FE_ERRO_FONTE eh descartado.
  zerar_farol();
  abrir_titulo("tt8000009", "movie", "A obs", 0, 0,
               "https://example.invalid/a_obs.mkv", &genA);
  abrir_titulo("tt8000010", "movie", "B obs", 0, 0,
               "https://example.invalid/b_obs.mkv", &genB);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genB, 4003, 2);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2004, 0);
  video_bombear();
  assert(video_falhou() && video_erro_identificado() == 1);
  assert(!strcmp(video_erro_texto(), "4003 player error:2"));
  assert(video_erro_decoder_codigo() == 4003);
  g_intercalar = (IntercalarCtx){
    .gen = genA, .legado = 0, .cod = 2004, .tipo = 0,
    .esperadoTok = "m3_4003", .esperadoTxt = "4003 player error:2",
    .esperadoDec = 4003, .esperadoId = 1, .disparado = 0
  };
  player_erro_fonte();
  assert(g_intercalar.disparado == 1);
  assert(erroPendente() == 0);
  assert(player_externo_disponivel() == 1);

  // 9) FAROL6-r7: recusa sincrona de NewString na primeira retentativa VOD nao
  // prende o backend em reconexao eterna; consome -1, avanca para tentativa 2
  // e recupera na segunda reabertura com nova geracao e posicao preservada.
  zerar_farol();
  g_recusarNewString = 0;
  video_definir_reconexao(1);
  abrir_titulo("tt9000001", "movie", "VOD recon NewString", 0, 0,
               "https://example.invalid/vod_recon.mkv", &genA);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 100000, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TOCANDO, 0, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 12345, 0, 0, -1, (jint)genA);
  video_bombear();
  assert(!video_falhou() && !video_reconectando() && video_pos() == 12.345);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genA, 2002, 0);
  video_bombear();
  assert(!video_falhou() && video_reconectando() == 1);
  assert(recon.tentativa == 1 && recon.pendente == 1);

  g_recusarNewString = 1;
  atomic_fetch_add(&relogioMs, 3000);
  video_bombear();
  unsigned genRecusada = video_android_sessao();
  assert(genRecusada != genA && g_recusarNewString == 0 && erroPendente());
  assert(video_falhou() && !video_erro_texto()[0] && !video_erro_decoder_codigo() && !video_erro_identificado());
  assert(video_reconectando() == 1);

  video_bombear();
  assert(!erroPendente() && !video_falhou());
  assert(recon.tentativa == 2 && recon.pendente == 1 && video_reconectando() == 2);

  atomic_fetch_add(&relogioMs, 10000);
  video_bombear();
  unsigned genFinal = video_android_sessao();
  assert(genFinal != genRecusada && (unsigned)atomic_load(&geracaoAberta) == genFinal);
  // Entrega obsoleta da geracao anterior eh descartada apos a recuperacao.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genA, 4003, 2);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genRecusada, 2004, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 100000, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TOCANDO, 0, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 12345, 0, 0, -1, (jint)genFinal);
  video_bombear();
  assert(!erroPendente() && !video_falhou() && !video_reconectando());
  assert(prontoLoad == 1 && video_pos() == 12.345);
  assert(!video_erro_texto()[0] && !video_erro_decoder_codigo() && !video_erro_identificado());

  // 10) Recusas sincronas repetidas esgotam o orcamento de retentativas e
  // desistem sem laco perpetuo de reconexao nem reinicio por 2004 tardio.
  zerar_farol();
  video_definir_reconexao(1);
  abrir_titulo("tt9000002", "movie", "VOD recon esgota", 0, 0,
               "https://example.invalid/vod_esgota.mkv", &genA);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 100000, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TOCANDO, 0, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 12345, 0, 0, -1, (jint)genA);
  video_bombear();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genA, 2002, 0);
  video_bombear();
  assert(recon.tentativa == 1 && recon.pendente == 1);
  for (int t = 1; t <= NV_RECON_MAX; t++) {
    g_recusarNewString = 1;
    atomic_fetch_add(&relogioMs, nv_recon_espera_ms(t));
    video_bombear();
    assert(g_recusarNewString == 0 && erroPendente());
    video_bombear();
    if (t < NV_RECON_MAX) {
      assert(!video_falhou() && recon.tentativa == t + 1 && recon.pendente == 1);
    } else {
      assert(video_falhou() && recon.esgotou && !recon.pendente && !video_reconectando());
    }
  }
  unsigned genEsgotada = video_android_sessao();
  int abertaAntes = atomic_load(&geracaoAberta);
  atomic_fetch_add(&relogioMs, 60000);
  video_bombear();
  assert(video_android_sessao() == genEsgotada && atomic_load(&geracaoAberta) == abertaAntes);
  assert(video_falhou() && recon.esgotou && !video_reconectando() && video_pos() == 12.345);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genEsgotada, 2004, 0);
  video_bombear();
  assert(video_falhou() && recon.esgotou && !recon.pendente && !video_reconectando());

  // 11) Erro 2004 tardio apos falha terminal 4003 nao reinicia reconexao.
  zerar_farol();
  video_definir_reconexao(1);
  abrir_titulo("tt9000003", "movie", "VOD terminal 4003", 0, 0,
               "https://example.invalid/vod_term.mkv", &genA);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 100000, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TOCANDO, 0, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 12345, 0, 0, -1, (jint)genA);
  video_bombear();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genA, 4003, 2);
  video_bombear();
  assert(video_falhou() && video_erro_decoder_codigo() == 4003 && !video_reconectando());
  assert(recon.tentativa == 0 && recon.pendente == 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genA, 2004, 0);
  video_bombear();
  assert(video_falhou() && !video_reconectando() && recon.tentativa == 0 && recon.pendente == 0);

  puts("farol_android_erro: tudo ok");
  return 0;
}
