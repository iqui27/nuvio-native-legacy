// FAROL — testes das telas 4, 5, 6 e estados §3.3 (issue 22).
//
// Dois modos de compilacao:
//   1. Padrao: testa espelho completo contra farolredator_schema(), D-pad sem
//      beco nas telas 4/5/6 e estados §3.3, toggle incluir_log nascendo 0 e
//      resetando ao reabrir, URL e ciclo de textura do QR, envio real contra
//      tests/farol_envio_server.py e fluxo RESOLVER_PEDIU_CONTAS via app.c.
//   2. -DFAROL_VAI_SHOT: linka o app inteiro com FBO GL 1920x1080 e grava PNGs
//      das telas 4, 5 e 6 em diretorio temporario.
#ifndef FAROL_VAI_SHOT

#define _POSIX_C_SOURCE 200809L
#define RESOLVER_TESTE 1
#include "farolvai.h"
#include "farolenvioui.h"
#include "farolenvio.h"
#include "farolpacote.h"
#include "farolperfil.h"
#include "farolredator.h"
#include "faroltexto.h"
#include "farol.h"
#include "farolacha.h"
#include "farolachaui.h"
#include "farolsonda.h"
#include "resolver.h"
#include "resolverregra.h"
#include "addonmodo.h"
#include "addons.h"
#include "atualizacao.h"
#include "avisos.h"
#include "debrid.h"
#include "gfx.h"
#include "layout.h"
#include "ponteiro.h"
#include "streams.h"
#include "text.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int falhas;
#define CONFERE(c) do { if (!(c)) { fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

/* ----------------------------------------------------- estado controlado -- */
static char dadosDir[512];
static char baseUrl[256];
static int  c_offline, c_temConta = 1, c_logPedidos, c_vidro, c_reduzidas = 1;
static int  c_atAbrirN, c_atProcurarN, c_traktAjustesN, c_menuFechouN;
static char c_atNova[48];
static int  c_tela, c_trakt = 1, c_simkl = 0, c_addons = 2;
static int  c_texLive, c_texGenN, c_texDelN;
static GLuint c_nextTex = 1;
static char c_txt[256][200];
static int  c_txtN;

static void anotar_txt(const char *s) {
  if (s && c_txtN < (int)(sizeof c_txt / sizeof c_txt[0]))
    snprintf(c_txt[c_txtN++], sizeof c_txt[0], "%s", s);
}
static int escreveu(const char *s) {
  for (int i = 0; i < c_txtN; i++) if (strstr(c_txt[i], s)) return 1;
  return 0;
}

/* --------------------------------------------- stubs de app / GL / texto -- */
const char *i18n(const char *s) { return s; }
int ajustes_idioma_ingles(void) { return 0; }
int ajustes_vidro(void) { return c_vidro; }
int ajustes_animacoes_reduzidas(void) { return c_reduzidas; }
void ajustes_acento(float *r, float *g, float *b) { *r = 0.25f; *g = 0.65f; *b = 1.0f; }
float ajustes_acento_tinta(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
void ajustes_abrir_no_trakt(void) { c_traktAjustesN++; }
void ajustes_abrir_na_espera_fonte(void) {}
void ajustes_iniciar(void) {}

size_t nv_rec_url_copiar(char *dst, size_t tam) {
  if (!dst || !tam) return 0;
  snprintf(dst, tam, "%s", baseUrl);
  return strlen(dst);
}
const char *atualizacao_nova(void) { return c_atNova; }
void atualizacao_abrir(void) { c_atAbrirN++; }
void atualizacao_procurar_agora(void) { c_atProcurarN++; }

int rede_saude_offline(void) { return c_offline; }
int addons_n(void) { return c_addons; }
int addons_ocupado(void) { return 0; }
int addons_ativo(int i) { (void)i; return 1; }
int addons_fornece(int i, int q) {
  if (i == 0 && q == ADD_STREAM) return 1;
  if (i == 1 && q == ADD_CATALOGO) return 1;
  return 0;
}
int addons_motivo_vazio(char *d, unsigned n) { (void)d; (void)n; return 0; }
int debrid_ativo(void) { return 1; }
int debrid_sem_plano(void) { return 0; }
const char *debrid_sem_plano_frase(int m) { (void)m; return NULL; }
int debrid_recusa(char *d, unsigned n) { (void)d; (void)n; return 0; }
const char *debrid_servico_id(int s) { return s == DEBRID_TB ? "torbox" : ""; }
const char *debrid_chave_mascarada(const char *s, char *d, unsigned n) {
  if (s && !strcmp(s, "torbox")) snprintf(d, n, "····1234");
  else if (d && n) d[0] = '\0';
  return d;
}
int trakt_ativo(void) { return c_trakt; }
int simkl_ativo(void) { return c_simkl; }
void trakt_http_recusada(void) {}

int stream_atual(void) { return -1; }
const Stream *stream_item(int i) { (void)i; return NULL; }
int stream_kbps_atual(void) { return 0; }
int stream_ordem_addon(int i) { (void)i; return -1; }
int video_registro_negado(void) { return 0; }
int video_audio_nao_suportado(void) { return 0; }
int video_n_audio(void) { return 0; }
typedef struct VideoFaixa VideoFaixa;
const VideoFaixa *video_audio(int i) { (void)i; return NULL; }
int video_audio_atual(void) { return -1; }
int video_faixa_dts(const VideoFaixa *f) { (void)f; return 0; }
int video_dts_estado(void) { return 0; }
int video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void) { return ""; }
int nv_webos_major(void) { return 0; }
int dts_playback_enabled(void) { return 0; }
int dts_tpk_disponivel(void) { return 0; }
long memlog_livre_mb(void) { return 512; }
int player_aberto(void) { return 0; }
int player_ultimo_erro_farol(FarolErro *o, int *d) { (void)o; (void)d; return 0; }
uint64_t player_ultimo_erro_tentativa(void) { return 0; }
uint64_t player_ultimo_erro_seq(void) { return 0; }
int player_externo_disponivel_tentativa(uint64_t t) { (void)t; return 0; }
int player_abrir_externo_tentativa(uint64_t t) { (void)t; return 0; }
uint64_t player_ultimo_addon_farol(void) { return 0; }
void stream_farol_fatos(int *a, int *b, int *c, int *d, int *e, int *f, int *g,
                        int *h, int *i, int *j, int *k, int *l) {
  *a = *b = *c = *d = *e = *f = *g = *h = *i = *j = *k = *l = -1;
}
int home_fileiras_na_tela(void) { return 0; }
const char *app_tela_nome(void) { return "resolver"; }
int recomenda_pessoa_canonica(const char *id, char *d, size_t n) { (void)id; if (d && n) d[0] = 0; return 0; }
int cat_indice_por_imdb(const char *i) { (void)i; return -1; }
int cat_copiar_item(int i, CatItem *d) { (void)i; (void)d; return 0; }
int cat_copiar_por_id(const char *i, const char *t, CatItem *d) { (void)i; (void)t; (void)d; return 0; }
int cat_fileira_imdb(const char *c, char (*o)[64], int m, int s) { (void)c; (void)o; (void)m; (void)s; return 0; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
int cat_n(void) { return 0; }
unsigned cat_revisao(void) { return 0; }
int cat_visto(const CatItem *c) { (void)c; return 0; }
int cat_remover_fileiras_prefixo(const char *d, const char *p) { (void)d; (void)p; return 0; }
int farolcena_home(char *d, size_t c, const void *x, void *a, size_t b) { (void)d; (void)c; (void)x; (void)a; (void)b; return 0; }
int farolpeca_ler(void *p, const char *j, int n) { (void)p; (void)j; (void)n; return 0; }
int farolpeca_texto_tem(const void *t) { (void)t; return 0; }
const char *farolpeca_x(const void *c, const void *t) { (void)c; (void)t; return ""; }
int farolrec_nivel(const void *g) { (void)g; return 0; }
int farolrec_escolher(const void *c, const void *k, int n, void *o, int m) {
  (void)c; (void)k; (void)n; (void)o; (void)m; return 0;
}
const char *player_id_canal(void) { return ""; }
int farolsonda_iniciar(const char *u, const char *const *c) { (void)u; (void)c; return 0; }
int farolsonda_estado(FarolSonda *o) { if (o) memset(o, 0, sizeof *o); return 0; }
void farolsonda_cancelar(void) {}
int farolficha_ler_item(const CatItem *c, void *d) { (void)c; (void)d; return 0; }
uint32_t farolficha_chave(const void *f) { (void)f; return 0; }
int farolsinal_modelo(void *d) { (void)d; return 0; }
int farolsinal_ficha_do_item(const CatItem *c, void *d) { (void)c; (void)d; return 0; }
void farolsinal_mostrado(const CatItem *c) { (void)c; }
int farolsinal_recomendar(void) { return 0; }
void farolsinal_zerar_gosto(void) {}
void farolsinal_definir_recomendar(int l) { (void)l; }
int seguro_perfil_ativo(void) { return 0; }
int perfil_infantil_ativo(void) { return 0; }
int perfis_ativo(void) { return 1; }
int fil_oculta(const char *c) { (void)c; return 0; }
int diario_n(int p) { (void)p; return 0; }
int diario_entrada(int p, int i, void *d) { (void)p; (void)i; (void)d; return 0; }
int diario_titulo(int p, const char *i, void *d) { (void)p; (void)i; (void)d; return 0; }
int diario_nome(const char *i, char *t, size_t nt, int *a, char *pn, size_t np) {
  (void)i; if (t && nt) t[0] = 0; (void)a; if (pn && np) pn[0] = 0; return 0;
}
int salvos_n(void) { return 0; }
const void *salvos_item(int i) { (void)i; return NULL; }
int32_t mapa_hash_nome(const char *x) { (void)x; return -1; }
uint32_t mapa_hash_titulo(const char *x) { (void)x; return 1; }

int avisos_envio_info(AvisosEnvio *o) { memset(o, 0, sizeof *o); return 0; }
void avisos_enviar_registro_atual(void) { c_logPedidos++; }
int avisos_registro_cabecalhos(const char **cab, char *aut, size_t nAut,
                               char *via, size_t nVia, char *chave, size_t nChave) {
  (void)chave; (void)nChave;
  if (!c_temConta) return 0;
  snprintf(aut, nAut, "Authorization: Bearer tok-farol-18");
  snprintf(via, nVia, "X-Nuvio-Auth: nuvio");
  cab[0] = aut; cab[1] = via; cab[2] = "Content-Type: application/json"; cab[3] = NULL;
  return 1;
}

static void arq_caminho(char *dst, size_t n, const char *nome) { snprintf(dst, n, "%s/%s", dadosDir, nome); }
char *dados_ler(const char *nome) {
  char c[700]; FILE *f; long n; char *b;
  arq_caminho(c, sizeof c, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = (char *)malloc((size_t)n + 1);
  if (!b) { fclose(f); return NULL; }
  if (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
  b[n] = 0; fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *conteudo) {
  char c[700], t[720]; FILE *f;
  arq_caminho(c, sizeof c, nome);
  snprintf(t, sizeof t, "%s.tmp", c);
  f = fopen(t, "wb");
  if (!f) return 0;
  fputs(conteudo, f);
  fclose(f);
  return rename(t, c) == 0;
}
int dados_apagar(const char *nome) { char c[700]; arq_caminho(c, sizeof c, nome); return unlink(c) == 0; }

/* GL / GFX / Text / Ponteiro stubs com contador de textura QR */
int perftv_enabled;
int perftv_uploads;
float gfx_tex_aspect_atual;
void glGenTextures(GLsizei n, GLuint *t) {
  for (GLsizei i = 0; i < n; i++) { t[i] = c_nextTex++; c_texLive++; c_texGenN++; }
}
void glDeleteTextures(GLsizei n, const GLuint *t) {
  for (GLsizei i = 0; i < n; i++) if (t[i]) { c_texLive--; c_texDelN++; }
}
void glBindTexture(GLenum target, GLuint t) { (void)target; (void)t; }
void glPixelStorei(GLenum p, GLint v) { (void)p; (void)v; }
#undef glTexImage2D
void glTexImage2D(GLenum target, GLint level, GLint intf, GLsizei w, GLsizei h,
                  GLint border, GLenum fmt, GLenum type, const void *px) {
  (void)target; (void)level; (void)intf; (void)border; (void)fmt; (void)type;
  CONFERE(w > 20 && h == w && px != NULL);
}
void glTexParameteri(GLenum t, GLenum p, GLint v) { (void)t; (void)p; (void)v; }
void gfx_tex_esquecer(GLuint t) { (void)t; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo m, float raio, float fx,
              float px, float py, float cr, float cg, float cb, float ca) {
  (void)r; (void)tex; (void)m; (void)raio; (void)fx; (void)px; (void)py;
  (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) {
  (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro_painel(GfxRect r, float raio, float fundo, float a) {
  (void)r; (void)raio; (void)fundo; (void)a;
}
float gfx_escala_ui(void) { return 1.0f; }
float gfx_escala(void) { return 1.0f; }
void gfx_escala_sair(float a) { (void)a; }
float botao_largura(const char *rot, const char *ic, int pri) {
  (void)rot; (void)ic; (void)pri; return 160.0f;
}
void botao_pilula(GfxRect r, const char *rot, const char *ic, float f, int p, int al, float a) {
  (void)r; (void)ic; (void)f; (void)p; (void)al; (void)a; anotar_txt(rot);
}
float botao_cor_foco(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
void botao_luz(GfxRect r, float f, float a) { (void)r; (void)f; (void)a; }
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn fo, PonteiroFn at, int a, int b) {
  (void)x; (void)y; (void)w; (void)h; (void)fo; (void)at; (void)a; (void)b;
}
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)r; (void)g; (void)b; (void)a;
  anotar_txt(s); return l;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float mw) {
  TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)r; (void)g; (void)b; (void)a; (void)mw;
  anotar_txt(s); return l;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) {
  (void)e; if (a > 0.0f) anotar_txt(s); (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)ml;
  return 0.0f;
}
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float a, float t) {
  (void)e; if (a > 0.0f) anotar_txt(s); (void)r; (void)g; (void)b; (void)x; (void)y; (void)t;
  return 0.0f;
}

/* Ponte real com trecho de app.c para RESOLVER_PEDIU_CONTAS */
enum { TELA_HOME = 0, TELA_AJUSTES = 1 };
enum { MENU_AJUSTES = 5 };
static void menu_fechar(void) { c_menuFechouN++; }
static void menu_definir_destino(int d) { (void)d; }
static void trocarTela(int t) { c_tela = t; }
#define tela c_tela
static void addonguia_reconectar_debrid(int a) { (void)a; }
const char *addons_base(int i) { (void)i; return ""; }
int farolacao_rede_executar(int acao, int persistir) { (void)acao; (void)persistir; return 0; }
static int abrirFarolPedido(const FarolAchaPedido *p) { (void)p; return 0; }
void ilha_atividade(const char *t, float p) { (void)t; (void)p; }
int segurar_venceu(Uint32 a, Uint32 d, const char *t) { (void)a; (void)d; (void)t; return 0; }
const char *farolacha_rotulo(FarolAchaTipo t) { (void)t; return ""; }
void farolachaui_reset(void) {}
void farolachaui_abrir(FarolAchaTipo t, int m, unsigned s) { (void)t; (void)m; (void)s; }
void farolachaui_abrir_op(const FarolAchaAbrir *op) { (void)op; }
void farolachaui_abrir_busca(const char *t) { (void)t; }
int farolachaui_abrir_busca_geracao(const char *t, uint64_t g) { (void)t; (void)g; return 0; }
void farolachaui_recarregar(void) {}
int farolachaui_evento(const SDL_Event *e, int *pv, int *pa) { (void)e; if (pv) *pv = 0; if (pa) *pa = 0; return 0; }
void farolachaui_atualizar(float dt, Uint32 a) { (void)dt; (void)a; }
void farolachaui_desenhar(float ox, float oy, float a, Uint32 ag) { (void)ox; (void)oy; (void)a; (void)ag; }
int farolachaui_pediu_abrir(FarolAchaPedido *p) { (void)p; return 0; }
int farolachaui_pediu_voz(void) { return 0; }
int farolachaui_resolver_pedido(const FarolAchaPedido *p, unsigned *r) { (void)p; (void)r; return 0; }
int farolachaui_pedido_bate(const CatItem *c, const FarolAchaPedido *p) { (void)c; (void)p; return 0; }
int farolachaui_n(void) { return 0; }
int farolachaui_foco(void) { return 0; }
#include "app_resolver_bridge.inc"
#undef tela

/* --------------------------------------------------------------- helpers -- */
static void dormir_ms(int ms) {
  struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
  nanosleep(&t, NULL);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

static void desenhar(void) {
  c_txtN = 0;
  resolver_atualizar(0.016f, 0);
  resolver_desenhar(0);
}

static void usar(const char *base, const char *pref) {
  snprintf(baseUrl, sizeof baseUrl, "%s%s", base, pref);
  farolenvio_definir_url(baseUrl);
  farolenvioui_definir_url(baseUrl);
}

static void esperar_tela_nao(FarolEnvioUiTela evitando, int maxMs) {
  int passos = maxMs / 10;
  for (int i = 0; i < passos; i++) {
    resolver_atualizar(0.016f, 0);
    if (farolenvioui_tela() != evitando) return;
    dormir_ms(10);
  }
}

static void limpar_estado(void) {
  farolenvio_encerrar();
  farolenvio_teste_esquecer();
  dados_apagar(FAROLENVIO_PENDENTE_ARQ);
  dados_apagar(FAROLENVIO_ENVIADOS_ARQ);
  resolverregra_falha_limpar();
  farolanel_zerar();
  resolver_teste_esquecer();
  c_offline = 0;
  c_temConta = 1;
  c_logPedidos = 0;
  c_atAbrirN = 0;
  c_atProcurarN = 0;
  c_traktAjustesN = 0;
  c_menuFechouN = 0;
  c_atNova[0] = '\0';
  c_tela = TELA_HOME;
}

static void varredura_envio(int esperados, FarolEnvioUiTela telaEsp) {
  enum { MAXF = 16, PASSOS = 300 };
  static const SDL_Keycode DIR[4] = { SDLK_RIGHT, SDLK_LEFT, SDLK_DOWN, SDLK_UP };
  int vis[MAXF], nvis = 0, saida[MAXF] = { 0 };
  unsigned char aresta[MAXF][MAXF];
  unsigned seed = 7777u;
  memset(aresta, 0, sizeof aresta);
  vis[nvis++] = resolver_foco();
  for (int p = 0; p < PASSOS; p++) {
    int antes = resolver_foco(), depois, a, b;
    seed = seed * 1103515245u + 12345u;
    tecla(DIR[(seed >> 16) & 3]);
    CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ENVIO);
    CONFERE(farolenvioui_tela() == telaEsp);
    depois = resolver_foco();
    for (b = 0; b < nvis && vis[b] != depois; b++) {}
    if (b == nvis && nvis < MAXF) vis[nvis++] = depois;
    for (a = 0; a < nvis && vis[a] != antes; a++) {}
    for (b = 0; b < nvis && vis[b] != depois; b++) {}
    if (antes != depois && a < nvis && b < nvis) { saida[a] = 1; aresta[a][b] = 1; }
  }
  CONFERE(nvis == esperados);
  if (esperados > 1) {
    for (int i = 0; i < nvis; i++) CONFERE(saida[i]);
    for (int k = 0; k < nvis; k++)
      for (int i = 0; i < nvis; i++)
        for (int j = 0; j < nvis; j++)
          if (aresta[i][k] && aresta[k][j]) aresta[i][j] = 1;
    for (int i = 0; i < nvis; i++)
      for (int j = 0; j < nvis; j++)
        if (i != j) CONFERE(aresta[i][j]);
  }
}

static void ir_chip_enviar(void) {
  for (int i = 0; i < 10 && resolver_foco() != RESOLVER_B_ENVIAR &&
                  resolver_foco() != RESOLVER_B_LISTA; i++)
    tecla(SDLK_DOWN);
  if (resolver_foco() == RESOLVER_B_LISTA) tecla(SDLK_LEFT);
  CONFERE(resolver_foco() == RESOLVER_B_ENVIAR);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_ENVIO);
  CONFERE(farolenvioui_tela() == FEUI_TELA4_CONFIRMA);
}

static void testar_espelho_schema(void) {
  size_t nVai = 0, nNunca = 0;
  const char *const *vai = farolvai_vai_junto(&nVai);
  const char *const *nunca = farolvai_nunca_vai(&nNunca);
  int temRede = 0, temAddons = 0, temServ = 0;

  CONFERE(farolvai_schema_coberto() == 1);
  CONFERE(nVai == 9 && nNunca == 6);
  CONFERE(strcmp(farolvai_para("aparelho.rede.offline"), FAROLVAI_REDE) == 0);
  CONFERE(strcmp(farolvai_para("cliente.addons.total"), FAROLVAI_ADDONS) == 0);
  CONFERE(strcmp(farolvai_para("cliente.addons.tipos[]"), FAROLVAI_ADDONS) == 0);
  CONFERE(strcmp(farolvai_para("cliente.debrid[]"), FAROLVAI_SERVICOS) == 0);
  CONFERE(strcmp(farolvai_para("cliente.contas[]"), FAROLVAI_SERVICOS) == 0);

  for (size_t i = 0; i < nVai; i++) {
    if (strcmp(vai[i], FAROLVAI_REDE) == 0) temRede = 1;
    if (strcmp(vai[i], FAROLVAI_ADDONS) == 0) temAddons = 1;
    if (strcmp(vai[i], FAROLVAI_SERVICOS) == 0) temServ = 1;
  }
  CONFERE(temRede && temAddons && temServ);
  CONFERE(strcmp(nunca[0], FT_NUNCA_TITULOS) == 0);
  CONFERE(strcmp(nunca[5], FT_NUNCA_HISTORICO) == 0);
}

int main(int argc, char **argv) {
  const char *srv = argc > 1 ? argv[1] : "";
  const char *dir = argc > 2 ? argv[2] : "";
  FarolFatos fx;
  char pbuf[700];
  struct stat st;

  CONFERE(srv[0] && dir[0]);
  snprintf(dadosDir, sizeof dadosDir, "%s", dir);
  farolenvio_teste_prazos(3000, 1200);

  /* 1. Espelho completo contra farolredator_schema() */
  testar_espelho_schema();

  /* 2. Tela 4: D-pad sem beco, textos, toggle incluir_log OFF -> ON -> reset, Agora nao */
  limpar_estado();
  usar(srv, "/ok");
  farolregra_fatos_iniciar(&fx);
  fx.addonsQtd = 2;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  ir_chip_enviar();
  CONFERE(resolver_foco() == FEUI_B_MANDAR);
  CONFERE(farolenvioui_incluir_log() == 0);
  desenhar();
  CONFERE(escreveu(FT_VAI_JUNTO) && escreveu(FT_NUNCA_VAI));
  CONFERE(escreveu(FAROLVAI_REDE) && escreveu(FAROLVAI_ADDONS) && escreveu(FAROLVAI_SERVICOS));
  CONFERE(escreveu(FT_VAI_MODELO) && escreveu(FT_VAI_VERSAO) && escreveu(FT_VAI_FORMATO));
  CONFERE(escreveu(FT_VAI_PONTO) && escreveu(FT_VAI_AJUSTES) && escreveu(FT_VAI_EVENTOS));
  CONFERE(escreveu(FT_SOME_30) && escreveu(FT_INCLUIR_TEXTO));
  varredura_envio(3, FEUI_TELA4_CONFIRMA);

  /* Liga toggle -> Agora nao volta sem enviar -> reabre com toggle = 0 */
  for (int i = 0; i < 10 && resolver_foco() != FEUI_B_TOGGLE_LOG; i++) tecla(SDLK_UP);
  CONFERE(resolver_foco() == FEUI_B_TOGGLE_LOG);
  tecla(SDLK_RETURN);
  CONFERE(farolenvioui_incluir_log() == 1);
  desenhar();
  CONFERE(escreveu(FT_TEXTO_AVISO));
  tecla(SDLK_DOWN);
  if (resolver_foco() != FEUI_B_AGORA_NAO) tecla(SDLK_RIGHT);
  CONFERE(resolver_foco() == FEUI_B_AGORA_NAO);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_SINTOMA && !farolenvioui_ativo());
  for (int i = 0; i < 50 && farolenvio_em_voo() && farolenvio_info(NULL) == FAROLENVIO_OCIOSO; i++)
    dormir_ms(10);
  CONFERE(!farolenvio_em_voo() && farolenvio_info(NULL) == FAROLENVIO_OCIOSO);
  ir_chip_enviar();
  CONFERE(farolenvioui_incluir_log() == 0);
  CONFERE(resolver_foco() == FEUI_B_MANDAR);

  /* 3. Envio padrao (incluir_log = 0) -> Tela 5 (NV-4F7K-92 + QR /v1/farol/r/4F7K92) */
  tecla(SDLK_RETURN);
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_TELA5_ENVIADO);
  CONFERE(resolver_foco() == FEUI_B_PRONTO);
  CONFERE(c_logPedidos == 0);
  CONFERE(resolver_enviados_n() == 1);
  CONFERE(strcmp(farolenvioui_codigo_exibir(), "NV-4F7K-92") == 0);
  {
    char espQr[320];
    snprintf(espQr, sizeof espQr, "%s/v1/farol/r/4F7K92", baseUrl);
    CONFERE(strcmp(farolenvioui_qr_url(), espQr) == 0);
    CONFERE(strchr(farolenvioui_qr_url(), '?') == NULL);
  }
  CONFERE(farolenvioui_qr_ativo() == 1 && c_texLive == 1);
  desenhar();
  CONFERE(escreveu("NV-4F7K-92") && escreveu(FT_REGISTRO_ENVIADO_L2) && escreveu(FT_SOME_30));
  varredura_envio(1, FEUI_TELA5_ENVIADO);
  tecla(SDLK_RETURN); /* Pronto fecha limpo e libera textura */
  CONFERE(resolver_fase() == RESOLVER_FASE_SINTOMA && !farolenvioui_ativo());
  CONFERE(farolenvioui_qr_ativo() == 0 && c_texLive == 0);

  /* 4. Reabrir Tela 4 -> toggle = 1 -> enviar /okcon (8HJ3MX) -> opt-in log dispara */
  usar(srv, "/okcon");
  ir_chip_enviar();
  CONFERE(farolenvioui_incluir_log() == 0);
  tecla(SDLK_UP);
  CONFERE(resolver_foco() == FEUI_B_TOGGLE_LOG);
  tecla(SDLK_RETURN);
  CONFERE(farolenvioui_incluir_log() == 1);
  tecla(SDLK_DOWN);
  if (resolver_foco() != FEUI_B_MANDAR) tecla(SDLK_LEFT);
  CONFERE(resolver_foco() == FEUI_B_MANDAR);
  tecla(SDLK_RETURN);
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_TELA5_ENVIADO);
  CONFERE(c_logPedidos == 1);
  CONFERE(resolver_enviados_n() == 2);
  CONFERE(strcmp(farolenvioui_codigo_exibir(), "NV-8HJ3-MX") == 0);
  CONFERE(strstr(farolenvioui_qr_url(), "/v1/farol/r/8HJ3MX") != NULL);
  CONFERE(c_texLive == 1);
  tecla(SDLK_ESCAPE); /* Voltar fecha limpo */
  CONFERE(resolver_fase() == RESOLVER_FASE_SINTOMA && c_texLive == 0);

  /* 5. Tela 6: Ja corrigido (GET /conhecido -> corrigido; 0 POST; Atualizar agora) */
  usar(srv, "/corrigido");
  ir_chip_enviar();
  CONFERE(farolenvioui_incluir_log() == 0);
  tecla(SDLK_RETURN);
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_TELA6_CORRIGIDO);
  CONFERE(resolver_foco() == FEUI_B_ATUALIZAR);
  desenhar();
  CONFERE(escreveu("2.1.3") && escreveu(FT_JA_CORRIGIDO_L2));
  varredura_envio(2, FEUI_TELA6_CORRIGIDO);
  if (resolver_foco() != FEUI_B_ATUALIZAR) tecla(SDLK_LEFT);
  CONFERE(resolver_foco() == FEUI_B_ATUALIZAR);
  c_atNova[0] = '\0';
  tecla(SDLK_RETURN); /* Atualizar agora sem release conhecida -> atualizacao_procurar_agora */
  CONFERE(c_atProcurarN == 1 && c_atAbrirN == 0 && !resolver_aberto());

  /* Segunda vez com atualizacao_nova() preenchida -> atualizacao_abrir() */
  resolver_abrir();
  ir_chip_enviar();
  tecla(SDLK_RETURN);
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_TELA6_CORRIGIDO);
  snprintf(c_atNova, sizeof c_atNova, "2.1.3");
  tecla(SDLK_RETURN);
  CONFERE(c_atAbrirN == 1 && !resolver_aberto());

  /* 6. Estado §3.3 Enviando -> cancelavel com Voltar */
  limpar_estado();
  usar(srv, "/lento");
  resolver_abrir();
  ir_chip_enviar();
  tecla(SDLK_RETURN);
  CONFERE(farolenvioui_tela() == FEUI_ENVIANDO);
  desenhar();
  CONFERE(escreveu(FT_ENVIANDO));
  tecla(SDLK_ESCAPE);
  CONFERE(resolver_fase() == RESOLVER_FASE_SINTOMA && !farolenvioui_ativo());
  farolenvio_encerrar();

  /* 7. Estado §3.3 Sem internet / Pendente -> farol-pendente.json + auto-envio */
  limpar_estado();
  usar(srv, "/ok");
  c_offline = 1;
  resolver_abrir();
  ir_chip_enviar();
  tecla(SDLK_RETURN);
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_PENDENTE);
  CONFERE(resolver_foco() == FEUI_B_PRONTO);
  desenhar();
  CONFERE(escreveu(FT_SEM_NET_ENVIO_L2));
  arq_caminho(pbuf, sizeof pbuf, FAROLENVIO_PENDENTE_ARQ);
  CONFERE(stat(pbuf, &st) == 0);
  c_offline = 0;
  for (int i = 0; i < 400 && farolenvioui_tela() == FEUI_PENDENTE; i++) {
    resolver_atualizar(0.016f, 0);
    dormir_ms(10);
  }
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_TELA5_ENVIADO);
  CONFERE(resolver_enviados_n() == 1);
  CONFERE(stat(pbuf, &st) != 0);
  tecla(SDLK_RETURN);
  CONFERE(c_texLive == 0);

  /* 8. Estado §3.3 Servidor recusou (503) -> Tentar de novo / Fechar */
  usar(srv, "/r500");
  ir_chip_enviar();
  tecla(SDLK_RETURN);
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_FALHOU_RECUSOU);
  CONFERE(resolver_foco() == FEUI_B_TENTAR);
  desenhar();
  CONFERE(escreveu(FT_RECUSOU_L1) && escreveu(FT_RECUSOU_L2));
  varredura_envio(2, FEUI_FALHOU_RECUSOU);
  if (resolver_foco() != FEUI_B_TENTAR) tecla(SDLK_LEFT);
  CONFERE(resolver_foco() == FEUI_B_TENTAR);
  usar(srv, "/ok");
  tecla(SDLK_RETURN); /* Tentar de novo */
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_TELA5_ENVIADO);
  tecla(SDLK_RETURN);

  /* 9. Estado §3.3 Limite atingido (429) -> mostra ultimo codigo valido */
  usar(srv, "/r429");
  ir_chip_enviar();
  tecla(SDLK_RETURN);
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_FALHOU_429);
  CONFERE(resolver_foco() == FEUI_B_FECHAR);
  desenhar();
  CONFERE(escreveu(FT_LIMITE_L1) && escreveu(FT_LIMITE_L2) && escreveu("NV-4F7K-92"));
  tecla(SDLK_RETURN);

  /* 10. Estado §3.3 Sem conta -> RESOLVER_PEDIU_CONTAS + app.c */
  usar(srv, "/ok");
  c_temConta = 0;
  ir_chip_enviar();
  tecla(SDLK_RETURN);
  esperar_tela_nao(FEUI_ENVIANDO, 5000);
  CONFERE(farolenvioui_tela() == FEUI_FALHOU_SEM_CONTA);
  CONFERE(resolver_foco() == FEUI_B_CONTAS);
  desenhar();
  CONFERE(escreveu(FT_SEM_CONTA_L1) && escreveu(FT_SEM_CONTA_L2));
  varredura_envio(2, FEUI_FALHOU_SEM_CONTA);
  if (resolver_foco() != FEUI_B_CONTAS) tecla(SDLK_LEFT);
  CONFERE(resolver_foco() == FEUI_B_CONTAS);
  {
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_RETURN;
    app_resolver_despachar(&e);
  }
  CONFERE(!resolver_aberto());
  CONFERE(c_traktAjustesN == 1);
  CONFERE(c_tela == TELA_AJUSTES);
  CONFERE(c_texLive == 0 && c_texGenN == c_texDelN);

  limpar_estado();
  farolperfil_encerrar();
  if (falhas) { fprintf(stderr, "farol_vai: %d falha(s)\n", falhas); return 1; }
  puts("farol_vai: telas 4/5/6, espelho, QR, opt-in e estados 3.3 ok");
  return 0;
}

#else /* FAROL_VAI_SHOT */

#define RESOLVER_TESTE 1
#include "farolenvioui.h"
#include "farolenvio.h"
#include "resolver.h"
#include "resolverregra.h"
#include "farol.h"
#include "farolregra.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "gl_compat.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLuint fbo, fboTex;
static char shotSrv[256];

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

static void quadro(float dt) {
  SDL_PumpEvents();
  resolver_atualizar(dt, SDL_GetTicks());
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  resolver_desenhar(SDL_GetTicks());
}

static void captura(const char *nome, int nQuadros) {
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y, variou = 0;
  assert(pix);
  for (int i = 0; i < nQuadros; i++) { quadro(1.0f / 60.0f); SDL_Delay(2); }
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 1; y < 1920 * 1080 && !variou; y += 997)
    if (memcmp(pix + (size_t)y * 4, pix, 3)) variou = 1;
  assert(variou);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
}

static void esperar_envio(void) {
  for (int i = 0; i < 400 && farolenvioui_tela() == FEUI_ENVIANDO; i++) {
    quadro(1.0f / 60.0f);
    SDL_Delay(5);
  }
}

/* Override forte de cabecalhos para o servidor local de captura */
int avisos_registro_cabecalhos(const char **cab, char *aut, size_t nAut,
                               char *via, size_t nVia, char *chave, size_t nChave) {
  (void)chave; (void)nChave;
  snprintf(aut, nAut, "Authorization: Bearer tok-farol-18");
  snprintf(via, nVia, "X-Nuvio-Auth: nuvio");
  cab[0] = aut; cab[1] = via; cab[2] = "Content-Type: application/json"; cab[3] = NULL;
  return 1;
}

int main(int argc, char **argv) {
  const char *outDir = argc > 1 ? argv[1] : "/tmp";
  const char *srv = argc > 2 ? argv[2] : "";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700], u[320];
  SDL_Window *w;
  SDL_GLContext gl;
  FarolFatos fx;

  assert(dir && dir[0] && srv[0]);
  snprintf(shotSrv, sizeof shotSrv, "%s", srv);
  dados_iniciar(dir);
  ajustes_dir(dir);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  w = SDL_CreateWindow("farol-vai-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }

  farolenvio_teste_prazos(3000, 1200);
  farolregra_fatos_iniciar(&fx);
  fx.addonsQtd = 3;
  resolver_teste_fatos_farol(&fx);

  /* Tela 4: Vai junto / Nunca vai */
  snprintf(u, sizeof u, "%s/ok", shotSrv);
  farolenvio_definir_url(u);
  farolenvioui_definir_url(u);
  resolver_abrir();
  for (int i = 0; i < 8 && resolver_foco() != RESOLVER_B_ENVIAR &&
                  resolver_foco() != RESOLVER_B_LISTA; i++) tecla(SDLK_DOWN);
  if (resolver_foco() == RESOLVER_B_LISTA) tecla(SDLK_LEFT);
  assert(resolver_foco() == RESOLVER_B_ENVIAR);
  tecla(SDLK_RETURN);
  assert(farolenvioui_tela() == FEUI_TELA4_CONFIRMA);
  snprintf(nome, sizeof nome, "%s/vai-4-confirma.png", outDir);
  captura(nome, 24);

  /* Tela 5: Enviado (NV-4F7K-92 + QR) */
  tecla(SDLK_RETURN);
  esperar_envio();
  if (farolenvioui_tela() != FEUI_TELA5_ENVIADO) {
    FarolEnvioInfo inf;
    farolenvio_info(&inf);
    fprintf(stderr, "shot tela5=%d est=%d mot=%d http=%d cod=%s qr=%d\n",
            (int)farolenvioui_tela(), (int)inf.estado, (int)inf.motivo, inf.http, inf.codigo,
            farolenvioui_qr_ativo());
  }
  assert(farolenvioui_tela() == FEUI_TELA5_ENVIADO);
  assert(farolenvioui_qr_ativo());
  snprintf(nome, sizeof nome, "%s/vai-5-enviado.png", outDir);
  captura(nome, 16);
  tecla(SDLK_RETURN);
  assert(!farolenvioui_qr_ativo());

  /* Tela 6: Ja corrigido */
  snprintf(u, sizeof u, "%s/corrigido", shotSrv);
  farolenvio_definir_url(u);
  farolenvioui_definir_url(u);
  for (int i = 0; i < 8 && resolver_foco() != RESOLVER_B_ENVIAR &&
                  resolver_foco() != RESOLVER_B_LISTA; i++) tecla(SDLK_DOWN);
  if (resolver_foco() == RESOLVER_B_LISTA) tecla(SDLK_LEFT);
  assert(resolver_foco() == RESOLVER_B_ENVIAR);
  tecla(SDLK_RETURN);
  tecla(SDLK_RETURN);
  esperar_envio();
  assert(farolenvioui_tela() == FEUI_TELA6_CORRIGIDO);
  snprintf(nome, sizeof nome, "%s/vai-6-corrigido.png", outDir);
  captura(nome, 16);

  farolenvioui_esquecer();
  farolenvio_encerrar();
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("farol_vai_shot: 3 PNGs ok");
  return 0;
}

#endif
