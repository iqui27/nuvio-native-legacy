// CORRECAO-FAROL6B/6C: o caminho de PRODUCAO do Resolver com o motor de
// playbooks e os executores reais (player fechado = como abre por Ajustes).
// Compila src/resolver.c, src/extplayer.c (NV_ANDROID), src/addonguia.c,
// src/addonmodo.c e os trechos reais de src/player.c e src/app.c.
//
// Cobre:
//   - cenarios 1..8 (paridade Add-ons, Fonte FC_NENHUMA, mascara IMAGEM_DV,
//     DEBRID_DESCONECTOU, PLAYER_EXTERNO, sem botao morto, FE_FAROL_VEREDITO)
//   - cenario 9: Android com player FECHADO — player_externo_disponivel() decide
//     sem exigir player aberto, o cartao oferece "Player externo", e o OK passa
//     por app.c -> player_abrir_externo() -> extplayer_forcar() ->
//     android_abrir_externo() sem abrir o pipeline interno.
//   - cenario 10: Android com player FECHADO e cabecalhos privados / sem app
//     instalado / falha na Intent — recusa com seguranca sem tocar interno.
//   - cenario 11: Recusa sincrona (FE_RECUSA_SINC + FE_ERRO_FONTE) conta UMA
//     falha (TV_FORMATO), e duas recusas contam 2 (TV_FORMATO_REPETE).
//   - cenario 12: FE_FAROL_ACAO nunca sai com token vazio em "Mandar registro".
//   - cenario 13: QR de debrid com player FECHADO e titulo VOD (serie T2E5):
//     app.c -> addonguia_reconectar_debrid() -> celb_pegar -> AGP_REPETIR ->
//     app.c reabre o titulo no ponto (sem player_do_inicio) e renova fontes,
//     gravando AM_DEBRID so no add-on de fontes que falhou (nunca no 0 de
//     catalogo) e sem laco.
//   - cenario 14: QR de debrid com player FECHADO e lista trocada (sem add-on
//     de stream correspondente): grava credencial, nao suja add-on 0 de catalogo,
//     e reabre o filme no ponto.
//   - cenario 15: QR de debrid com player FECHADO e SEM titulo anterior:
//     nao abre o player e avisa pela ilha que a conta reconectou, sem laco.
//   - cenario 16: AG_MODO fora do Resolver com player fechado nao reabre filme.
//   - cenario 17: A->B (FE_PRONTO) -> fecha -> Resolver diagnostica A e usa
//     alternativa, sem jamais entregar URL/titulo de B ao player externo.
//   - cenario 18: A->B (sem FE_PRONTO) apos TV_FORMATO_REPETE -> alternativa.
//   - cenario 19: Caso positivo de TV_FORMATO unico (1 falha) com player fechado.
//   - cenario 20: Troca para B entre oferta e OK -> despacho recusa B e usa alternativa.

#include "resolver.h"
#include "resolverregra.h"
#include "farolregra.h"
#include "farolerro.h"
#include "farolanel.h"
#include "faroltexto.h"
#include "fontesonda.h"
#include "extplayer.h"
#include "addonguia.h"
#include "addonmodo.h"
#include "addons.h"
#include "catalogo.h"
#include "mediaurl.h"
#include "streams.h"
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---------------------------------------------------------------- cenario
static int c_offline, c_addons = 3, c_ocupado, c_semPlano, c_recusa, c_debrid = 1,
           c_tvNegou, c_kb, c_streamAtual = -1;
static long c_mem = -1;
static char c_motivo[200], c_recusaTxt[64];
static Stream c_stream;
static AvisosEnvio c_envio;
static int c_envioChamadas;

// Estado espelhado de src/player.c (para os trechos extraidos)
static int aberto, saindo, pediuSair;
static int temFixo, canalSessao, epT, epE;
static CatItem itemFixo, itemCanal;
static uint64_t itemSessao = 1, c_perfilSessao = 1;
static FarolErro ultimoFarol;
static int temUltimoFarol, ultimoFarolDebrid;
static uint64_t ultimoFarolAddonId;
static uint64_t farolTentativaSeq, ultimaFonteTentativa, ultimoFarolTentativa;
static char ultimaFonteUrl[NV_MEDIA_URL_MAX];
static int ultimaFonteTemCabs;
static int ultimaFonteTemItem, ultimaFonteIdx, ultimaFonteEpT, ultimaFonteEpE;
static CatItem ultimaFonteItem;
static uint64_t ultimaFonteSessao;
static char c_videoCabs[256];
static int c_tocarInternoN;
static int erroFonte;
static char erroTitulo[160], erroDica[160], linhaEp[220];
static int esperandoFonte, visivel, tocando, soBarra, erroBotao;
static int c_travaBloq, c_travaManualAutorizada;
static char c_travaImdb[24];
static int c_travaT, c_travaE;
static int c_catIdxAtual = 3;

// Estado espelhado de Android/extplayer
static char c_androidApps[256];
static int c_androidAbrirOk = 1, c_androidAbrirN;
static char c_androidAbrirUrl[NV_MEDIA_URL_MAX], c_androidAbrirTit[160], c_androidAbrirPkg[64];

// Estado espelhado de addons/addonguia/app.c
static struct {
  char base[128];
  char nome[64];
  int ativo, sondado, forneceStream;
} c_addon[4];
static int c_streamOrdemAddon = -1;
static int c_celbDisp = 1, c_celbAbertoN, c_celbFechadoN;
static char c_celbChave[128];
static int c_debridCelN;
static char c_debridCelServ[32], c_debridCelChave[128];
static int c_ilhaAvisosN;
static char c_ilhaChave[64], c_ilhaTexto[256];

// Espias de app.c
enum { TELA_HOME = 0, TELA_AJUSTES = 1 };
enum { MENU_AJUSTES = 5 };
static int tela = TELA_AJUSTES;
static uint64_t guiaTorrentRepetir;
static int torrentSessao, torrentPendente = -1, aguardandoFonte;
static int c_playerAbrirN, c_playerAbrirIdx = -1, c_playerDoInicioN;
static int c_playerDefEpN, c_playerDefT, c_playerDefE;
static int c_buscarModoN, c_buscarModoRenovar;
static int c_menuFechouN, c_ajustesFontesN, c_travauiBloq;
static CatItem c_catUltimo;
static int c_catReabrirIdx = 7;

// ------------------------------------------------------- simbolos de player
static const CatItem *item(void) {
  if (canalSessao) return &itemCanal;
  if (temFixo) return &itemFixo;
  return NULL;
}
int player_aberto(void) { return aberto; }
static void tocarFonteInterno(const char *url) { (void)url; c_tocarInternoN++; }
static int mini;
static int idxAtual(void) { return c_catIdxAtual; }
static struct { char imdb[24]; int nEpsT1; } c_catSlots[4];
static int c_catSlotCount;
int cat_indice_titulo(const char *imdb, int preferido) {
  if (!c_catSlotCount) {
    (void)imdb;
    return preferido;
  }
  if (preferido >= 0 && preferido < c_catSlotCount &&
      !strcmp(c_catSlots[preferido].imdb, imdb))
    return preferido;
  for (int i = 0; i < c_catSlotCount; i++)
    if (!strcmp(c_catSlots[i].imdb, imdb)) return i;
  return -1;
}
static int travaui_segura(const char *imdb, int idxCat, int t, int e, int *perfis, int *n) {
  (void)perfis;
  if (n) *n = 0;
  if (!c_travaBloq || c_travaManualAutorizada) return 0;
  if (c_catSlotCount > 0) {
    int nEps = (idxCat >= 0 && idxCat < c_catSlotCount) ? c_catSlots[idxCat].nEpsT1 : 0;
    return imdb && !strcmp(imdb, c_travaImdb) && t == 2 && e == 1 && nEps >= 3;
  }
  if (!c_travaImdb[0]) return 1;
  return imdb && !strcmp(imdb, c_travaImdb) && t == c_travaT && e == c_travaE;
}
static void farolErroRegistrar(const FarolErro *fe, int foiDebrid, uint64_t addonId);

void player_erro_fonte(void) {
  FarolErro fe;
  erroFonte = 1;
  memset(&fe, 0, sizeof fe);
  fe.classe = FC_FORMATO;
  snprintf(fe.cod, sizeof fe.cod, "m3_4003");
  farolErroRegistrar(&fe, ultimoFarolDebrid, ultimoFarolAddonId);
}
void player_erro_fonte_motivo(const char *titulo, const char *dica) {
  player_erro_fonte();
  snprintf(erroTitulo, sizeof erroTitulo, "%s", titulo ? titulo : "");
  snprintf(erroDica, sizeof erroDica, "%s", dica ? dica : "");
  resolverregra_falha_registrar(RES_T_FONTE, erroTitulo, erroDica, linhaEp, (long)time(NULL));
}
static int nv_perfil_sessao_atual(uint64_t s) { return s == c_perfilSessao; }

// ------------------------------------------------------------------- stubs
const char *i18n(const char *s) { return s; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *v) { (void)n; (void)v; return 1; }
int rede_saude_offline(void) { return c_offline; }
int addons_n(void) { return c_addons; }
int addons_ocupado(void) { return c_ocupado; }
int addons_ativo(int i) { return i >= 0 && i < c_addons ? c_addon[i].ativo : 0; }
int addons_sondado(int i) { return i >= 0 && i < c_addons ? c_addon[i].sondado : 0; }
const char *addons_base(int i) { return i >= 0 && i < c_addons ? c_addon[i].base : ""; }
const char *addons_nome(int i) { return i >= 0 && i < c_addons ? c_addon[i].nome : ""; }
int addons_fornece(int i, int rec) {
  if (i < 0 || i >= c_addons) return 0;
  if (rec == ADD_STREAM) return c_addon[i].forneceStream;
  return !c_addon[i].forneceStream;
}
int addons_motivo_vazio(char *d, unsigned n) {
  if (!c_motivo[0]) return 0;
  snprintf(d, n, "%s", c_motivo);
  return 1;
}
int debrid_sem_plano(void) { return c_semPlano; }
const char *debrid_sem_plano_frase(int m) { (void)m; return c_semPlano ? "Sua conta de debrid está sem plano." : NULL; }
int debrid_recusa(char *d, unsigned n) {
  if (!c_recusa) return 0;
  snprintf(d, n, "%s", c_recusaTxt[0] ? c_recusaTxt : "TorBox 403");
  return 1;
}
int debrid_ativo(void) { return c_debrid; }
int video_registro_negado(void) { return c_tvNegou; }
const char *video_cabecalhos(void) { return c_videoCabs; }
void player_toast(const char *t, unsigned ms) { (void)t; (void)ms; }
void player_abrir(int idx, const char *url) {
  (void)url;
  c_playerAbrirN++;
  c_playerAbrirIdx = idx;
  aberto = 1;
}
void player_definir_episodio(int t, int e) { c_playerDefEpN++; c_playerDefT = t; c_playerDefE = e; }
void player_voltar_a_esperar(void) {}

char *android_listar_players(void) {
  char *s = (char *)malloc(strlen(c_androidApps) + 1);
  if (s) strcpy(s, c_androidApps);
  return s;
}
int android_abrir_externo(const char *url, const char *titulo, const char *pkg) {
  c_androidAbrirN++;
  snprintf(c_androidAbrirUrl, sizeof c_androidAbrirUrl, "%s", url ? url : "");
  snprintf(c_androidAbrirTit, sizeof c_androidAbrirTit, "%s", titulo ? titulo : "");
  snprintf(c_androidAbrirPkg, sizeof c_androidAbrirPkg, "%s", pkg ? pkg : "");
  return c_androidAbrirOk;
}
int ajustes_player_externo(int anime) { (void)anime; return EXTPLAYER_INTERNO; }
const char *ajustes_player_pacote(int anime) { (void)anime; return ""; }
int ajustes_debrid_celular(const char *serv, const char *chave) {
  c_debridCelN++;
  snprintf(c_debridCelServ, sizeof c_debridCelServ, "%s", serv ? serv : "");
  snprintf(c_debridCelChave, sizeof c_debridCelChave, "%s", chave ? chave : "");
  c_debrid = 1;
  return 1;
}
void ajustes_definir_p2p_ligado(int on) { (void)on; }
int ajustes_p2p_ligado(void) { return 0; }
const char *ajustes_p2p_url(void) { return ""; }
const char *ajustes_idioma(void) { return "pt"; }
int ajustes_vidro(void) { return 0; }
int ajustes_animacoes_reduzidas(void) { return 1; }
int ajustes_animacoes_reduzidas_usuario(void) { return 1; }
void ajustes_acento(float *r, float *g, float *b) { *r = 0.4f; *g = 0.7f; *b = 1.0f; }
float ajustes_acento_tinta(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
int ajustes_tinta_foco(void) { return 255; }
int ajustes_tinta_foco2(void) { return 200; }
void ajustes_abrir_na_espera_fonte(void) { c_ajustesFontesN++; }
void ajustes_abrir_fileiras(void) {}
void ajustes_iniciar(void) {}

int celb_disponivel(void) { return c_celbDisp; }
int celb_embutido(int dono) { (void)dono; return 0; }
int celb_abrir_embutido(int dono, const char *tit) { (void)dono; (void)tit; c_celbAbertoN++; return 1; }
void celb_fechar_dono(int dono) { (void)dono; c_celbFechadoN++; }
int celb_pegar(int dono, char *dst, size_t n) {
  (void)dono;
  if (!c_celbChave[0]) return 0;
  snprintf(dst, n, "%s", c_celbChave);
  c_celbChave[0] = 0;
  return 1;
}
void celb_desenhar_em(int dono, GfxRect r, float a) { (void)dono; (void)r; (void)a; }
float celb_qr_lado(void) { return 100.0f; }
void celb_qr_desenhar(const char *u, float x, float y, float a) { (void)u; (void)x; (void)y; (void)a; }

void ilha_avisar(const char *chave, int tipo, const char *ic, const char *texto, unsigned ms, int tecla) {
  (void)tipo; (void)ic; (void)ms; (void)tecla;
  c_ilhaAvisosN++;
  snprintf(c_ilhaChave, sizeof c_ilhaChave, "%s", chave ? chave : "");
  snprintf(c_ilhaTexto, sizeof c_ilhaTexto, "%s", texto ? texto : "");
}
void ilha_avisar_ex(const IlhaAvisoEx *a) { (void)a; }
int ilha_tem(const char *chave) { (void)chave; return 0; }
int ilha_modal_aberto(void) { return 0; }
int ilha_modal_visivel(void) { return 0; }

int stream_atual(void) { return c_streamAtual; }
const Stream *stream_item(int i) { return i >= 0 ? &c_stream : NULL; }
int stream_kbps_atual(void) { return c_kb; }
int stream_ordem_addon(int i) { (void)i; return c_streamOrdemAddon; }
int stream_proxima_vod(int i) { (void)i; return -1; }
int stream_incompativel(int i) { (void)i; return 0; }
int stream_automatico_excluir(int i) { (void)i; return 0; }
void stream_folha_abrir(void) {}

long memlog_livre_mb(void) { return c_mem; }
int avisos_envio_info(AvisosEnvio *o) { *o = c_envio; c_envioChamadas++; return 1; }
int p2p_ativo(void) { return 0; }
void p2p_cancelar(void) {}
int p2pmotor_disponivel(void) { return 1; }
void p2pmotor_estado(P2pmEstado *e) { memset(e, 0, sizeof *e); }
void p2pmotor_cancelar(void) {}
const char *nuvem_base_login(void) { return ""; }
const char *perfis_dono(void) { return "u"; }
static int c_perfilSlot;
int perfis_ativo_addons(void) { return c_perfilSlot; }
const ContaPerfil *perfis_item_ativo(void) { return NULL; }
const char *sessao_usuario(void) { return "u"; }
int sync_manual_addons(void) { return 0; }
int sync_manual_addons_rodando(void) { return 0; }
int plrilha_centro_visivel(void) { return 0; }
void plrilha_pedir(const PlrIlhaPedido *p) { (void)p; }
void plrilha_desenhar(Uint32 agora) { (void)agora; }
void plrui_botao_repouso(GfxRect r, float a) { (void)r; (void)a; }
void plrui_linha_foco(GfxRect r, float raio, float f) { (void)r; (void)raio; (void)f; }
void plrui_material(GfxRect r, float raio, int modo, float a) { (void)r; (void)raio; (void)modo; (void)a; }
void addonsui_ponteiro(int a, int b) { (void)a; (void)b; }
void rec_avatar_estilo(GfxRect r, const char *u, const char *n, const char *f, float a, TxtEstilo e) {
  (void)r; (void)u; (void)n; (void)f; (void)a; (void)e;
}

float gfx_opacidade_grupo = 1.0f;
int gfx_vidro_sem_aro = 0;
float botao_largura(const char *rot, const char *ic, int pri) { (void)rot; (void)ic; (void)pri; return 100.0f; }
void botao_pilula(GfxRect r, const char *rot, const char *ic, float f, int p, int al, float a) { (void)r; (void)rot; (void)ic; (void)f; (void)p; (void)al; (void)a; }
float botao_cor_foco(float *r, float *g, float *b) { *r = *g = *b = 1.0f; return 1.0f; }
void botao_luz(GfxRect r, float f, float a) { (void)r; (void)f; (void)a; }
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) { (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) { (void)r; (void)raio; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) { (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_luz_canto(GfxRect r, float raio, float lx, float ly, float lr, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)lx; (void)ly; (void)lr; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro(GfxRect r, float raio, int tipo, float cinza, float dens, float a) {
  (void)r; (void)raio; (void)tipo; (void)cinza; (void)dens; (void)a;
}
float gfx_vidro_opacidade(void) { return 1.0f; }
void gfx_sombra_sob(GfxRect s, float foco, float parx, float raio, float cr, float cg, float cb,
                    float a, GfxRect recorte, float recRaio, float recA) {
  (void)s; (void)foco; (void)parx; (void)raio; (void)cr; (void)cg; (void)cb;
  (void)a; (void)recorte; (void)recRaio; (void)recA;
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
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn fo, PonteiroFn at, int a, int b) { (void)x; (void)y; (void)w; (void)h; (void)fo; (void)at; (void)a; (void)b; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) { TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; return l; }
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float mw) { TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; (void)mw; return l; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) { (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml; return 0.0f; }
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) { (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml; return 0.0f; }
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float a, float t) { (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)t; return 0.0f; }

// Helpers referenciados pelos blocos extraidos de src/app.c
static void menu_fechar(void) { c_menuFechouN++; }
static void menu_definir_destino(int d) { (void)d; }
static void trocarTela(int nova) { tela = nova; }
static void nomeParaFolha(void) {}
static int pedirProximaFonteVOD(int atual) { (void)atual; return 0; }
static void pedirTorrentEscolhido(int idx) { (void)idx; }
static void buscarParaPlayerModo(int renovar) { c_buscarModoN++; c_buscarModoRenovar = renovar; }
static void diagnostico_cancelar_vazao(void) {}
static void limparFonteVOD(void) {}
static int travaui_abrir_espera(int idx, int t, int e) { (void)idx; (void)t; (void)e; return c_travauiBloq; }
int cat_indice_reabrir(int idx, const CatItem *copia) {
  (void)idx;
  if (!copia || !copia->imdb[0]) return -1;
  c_catUltimo = *copia;
  return c_catReabrirIdx;
}

// Inclui os trechos REAIS de producao extraidos de src/player.c e src/app.c.
#include "farol_prod_bridge.inc"

#ifndef FAROL_BRIDGE_TEM_TENTATIVA
int player_ultimo_erro_farol(FarolErro *o, int *foiDebrid) {
  if (!temUltimoFarol) return 0;
  if (o) *o = ultimoFarol;
  if (foiDebrid) *foiDebrid = ultimoFarolDebrid;
  return 1;
}
#endif

static void sim_abrir_tentativa(const char *imdb, const char *tipo,
                                const char *titulo, int t, int e,
                                const char *url, int temCabs) {
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
  farolTentativaIniciar();
#else
  ultimaFonteUrl[0] = 0;
  ultimaFonteTemCabs = 0;
#endif
  temFixo = 1;
  canalSessao = 0;
  memset(&itemFixo, 0, sizeof itemFixo);
  snprintf(itemFixo.imdb, sizeof itemFixo.imdb, "%s", imdb ? imdb : "");
  snprintf(itemFixo.tipo, sizeof itemFixo.tipo, "%s", tipo ? tipo : "");
  snprintf(itemFixo.titulo, sizeof itemFixo.titulo, "%s", titulo ? titulo : "");
  epT = t;
  epE = e;
  if (url && url[0]) {
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
    farolFonteRegistrar(url, temCabs);
#else
    snprintf(ultimaFonteUrl, sizeof ultimaFonteUrl, "%s", url);
    ultimaFonteTemCabs = temCabs ? 1 : 0;
#endif
  }
}

void addonguia_shot_passo(float dt);

// ------------------------------------------------------------- cenario-base
static int c_slotSeq;
static void zerar(void) {
  memset(&c_stream, 0, sizeof c_stream);
  memset(&ultimoFarol, 0, sizeof ultimoFarol);
  memset(&c_envio, 0, sizeof c_envio);
  memset(&itemFixo, 0, sizeof itemFixo);
  memset(&itemCanal, 0, sizeof itemCanal);
  memset(&c_catUltimo, 0, sizeof c_catUltimo);
  c_offline = 0; c_addons = 3; c_ocupado = 0; c_semPlano = 0; c_recusa = 0;
  c_debrid = 1; c_tvNegou = 0; c_kb = 0;
  c_streamAtual = -1; c_mem = -1; c_motivo[0] = 0; c_recusaTxt[0] = 0;
  c_envioChamadas = 0;
  c_envio.disponivel = 0;
  aberto = saindo = pediuSair = 0;
  temFixo = canalSessao = epT = epE = 0;
  itemSessao = c_perfilSessao = 1;
  temUltimoFarol = 0; ultimoFarolDebrid = 0; ultimoFarolAddonId = 0;
  farolTentativaSeq = ultimaFonteTentativa = ultimoFarolTentativa = 0;
  ultimaFonteUrl[0] = 0; ultimaFonteTemCabs = 0; c_videoCabs[0] = 0;
  ultimaFonteTemItem = 0; memset(&ultimaFonteItem, 0, sizeof ultimaFonteItem);
  ultimaFonteIdx = -1; ultimaFonteEpT = 0; ultimaFonteEpE = 0; ultimaFonteSessao = 0;
  erroFonte = 0; erroTitulo[0] = 0; erroDica[0] = 0; linhaEp[0] = 0;
  esperandoFonte = visivel = tocando = soBarra = erroBotao = mini = 0;
  c_travaBloq = 0; c_travaManualAutorizada = 0;
  c_travaImdb[0] = 0; c_travaT = 0; c_travaE = 0; c_catIdxAtual = 3;
  c_tocarInternoN = 0;
  c_androidApps[0] = 0; c_androidAbrirOk = 1; c_androidAbrirN = 0;
  c_androidAbrirUrl[0] = c_androidAbrirTit[0] = c_androidAbrirPkg[0] = 0;
  memset(c_addon, 0, sizeof c_addon);
  snprintf(c_addon[0].base, sizeof c_addon[0].base, "https://v3-cinemeta.strem.io");
  snprintf(c_addon[0].nome, sizeof c_addon[0].nome, "Cinemeta");
  c_addon[0].ativo = 1; c_addon[0].sondado = 1; c_addon[0].forneceStream = 0;
  snprintf(c_addon[1].base, sizeof c_addon[1].base, "https://torrentio.example");
  snprintf(c_addon[1].nome, sizeof c_addon[1].nome, "Torrentio");
  c_addon[1].ativo = 1; c_addon[1].sondado = 1; c_addon[1].forneceStream = 1;
  snprintf(c_addon[2].base, sizeof c_addon[2].base, "https://mediafusion.example");
  snprintf(c_addon[2].nome, sizeof c_addon[2].nome, "MediaFusion");
  c_addon[2].ativo = 1; c_addon[2].sondado = 1; c_addon[2].forneceStream = 1;
  c_streamOrdemAddon = -1;
  c_celbDisp = 1; c_celbAbertoN = c_celbFechadoN = 0; c_celbChave[0] = 0;
  c_debridCelN = 0; c_debridCelServ[0] = c_debridCelChave[0] = 0;
  c_ilhaAvisosN = 0; c_ilhaChave[0] = c_ilhaTexto[0] = 0;
  tela = TELA_AJUSTES;
  guiaTorrentRepetir = 0; torrentSessao = 0; torrentPendente = -1; aguardandoFonte = 0;
  c_playerAbrirN = 0; c_playerAbrirIdx = -1; c_playerDoInicioN = 0;
  c_playerDefEpN = 0; c_playerDefT = c_playerDefE = 0;
  c_buscarModoN = 0; c_buscarModoRenovar = 0;
  c_menuFechouN = 0; c_ajustesFontesN = 0; c_travauiBloq = 0;
  c_catReabrirIdx = 7;
  c_perfilSlot = ++c_slotSeq;
  addonmodo_preparar("u", c_perfilSlot);
  addonguia_fechar();
  (void)addonguia_pedido();
  farolanel_zerar();
  resolverregra_falha_limpar();
  resolver_teste_esquecer();
  resolver_teste_fatos(NULL);
  resolver_teste_fatos_farol(NULL);
}

// Uma falha recente de fonte como player_erro_fonte registra: a memoria do
// Resolver mais o erro nativo normalizado que so o Farol le.
static void falhar_fonte(const char *titulo, const char *dica,
                         FarolClasse classe, int http, const char *cod) {
  FarolErro fe;
  resolverregra_falha_registrar(RES_T_FONTE, titulo, dica, "T2:E5", (long)time(NULL));
  memset(&fe, 0, sizeof fe);
  fe.classe = classe;
  fe.http = http;
  snprintf(fe.cod, sizeof fe.cod, "%s", cod ? cod : "");
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
  farolErroRegistrar(&fe, ultimoFarolDebrid, ultimoFarolAddonId);
#else
  ultimoFarol = fe;
  temUltimoFarol = 1;
#endif
}

static void apertar_ok(void) {
  SDL_Event evento = { .type = SDL_KEYDOWN };
  evento.key.keysym.sym = SDLK_RETURN;
  resolver_evento(&evento);
}

static void apertar_ok_app(void) {
  SDL_Event evento = { .type = SDL_KEYDOWN };
  evento.key.keysym.sym = SDLK_RETURN;
  app_resolver_despachar(&evento);
}

static void avancar_addonguia(void) {
  for (int i = 0; i < 40; i++) {
    addonguia_shot_passo(0.05f);
    addonguia_atualizar(0);
  }
}

int main(void) {
  const int *est;
  FarolFatos fx;

  // ------------------------------------------------------------ cenario 1
  zerar();
  snprintf(c_motivo, sizeof c_motivo, "%s",
           "4 add-ons responderam: nenhum tem este título");
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  est = resolver_teste_estados();
  assert(est[RES_T_ADDONS] == RES_FALHOU);
  assert(resolver_teste_veredito() == FAROL_C_NADA_ACHADO);
  assert(strstr(resolver_teste_titulo(), "add-ons") != NULL);
  assert(strstr(resolver_teste_corpo(), "nenhum tem este título") != NULL);
  apertar_ok();
  assert(resolver_pedido() == RESOLVER_PEDIU_FONTES);
  assert(!resolver_aberto());

  // ------------------------------------------------------------ cenario 2
  zerar();
  falhar_fonte("A última fonte falhou.", "Escolha outra fonte.",
               FC_NENHUMA, 0, "ums_outro");
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  est = resolver_teste_estados();
  assert(est[RES_T_FONTE] == RES_FALHOU);
  assert(resolver_teste_veredito() == FAROL_C_NADA_ACHADO);
  assert(strstr(resolver_teste_titulo(), "fonte falhou") != NULL);
  apertar_ok();
  assert(c_envioChamadas >= 1);

  // ------------------------------------------------------------ cenario 3
  zerar();
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_WEBOS;
  fx.addonsQtd = 3;
  fx.fonteDv = 1;
  fx.tvDv = 0;
  fx.erro.classe = FC_HTTP;
  fx.erro.http = 403;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_GERAL);
  assert(resolver_teste_veredito() != FAROL_C_IMAGEM_DV);
  fx.erro.classe = FC_NENHUMA;
  fx.erro.http = 0;
  resolver_teste_fatos_farol(&fx);
  resolver_teste_testar(FAROL_S_CORES);
  assert(resolver_teste_veredito() == FAROL_C_IMAGEM_DV);
  assert(strstr(resolver_teste_titulo(), FT_IMAGEM_DV_L1) != NULL);
  apertar_ok();
  assert(c_envioChamadas >= 1);

  // ------------------------------------------------------------ cenario 4
  zerar();
  assert(!fontesonda_host_debrid("https://real-debrid.com.outro.example/video"));
  assert(!fontesonda_host_debrid("https://torbox.app@outro.example/video"));
  for (int http = 401; http <= 403; http += 2) {
    ultimoFarolDebrid = fontesonda_host_debrid("https://CDN.real-debrid.com:443/video");
    falhar_fonte("A última fonte falhou.", "", FC_HTTP, http, "http");
    resolver_abrir();
    resolver_teste_testar(RES_S_NAO_COMECA);
    assert(resolver_teste_veredito() == FAROL_C_DEBRID_DESCONECTOU);
    assert(strstr(resolver_teste_titulo(), FT_DEBRID_DESCONECTOU_L1) != NULL);
    assert(strstr(resolver_teste_corpo(), FT_DEBRID_DESCONECTOU_L2) != NULL);
    assert(resolver_teste_prim() == FAROL_A_RECONECTAR_DEBRID);
    apertar_ok();
    assert(resolver_pedido() == RESOLVER_PEDIU_DEBRID);
    assert(!resolver_aberto());
  }

  // ------------------------------------------------------------ cenario 5
  zerar();
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_ANDROID;
  fx.addonsQtd = 3;
  fx.erro.classe = FC_FORMATO;
  fx.falhasSeguidas = 1;
  fx.externoDisponivel = 1;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  assert(strstr(resolver_teste_titulo(), FT_TV_FORMATO_L1) != NULL);
  assert(strstr(resolver_teste_corpo(), FT_L2_PLAYER_EXTERNO) != NULL);
  apertar_ok();
  assert(resolver_pedido() == RESOLVER_PEDIU_PLAYER_EXT);
  assert(!resolver_aberto());

  // ------------------------------------------------------------ cenario 6
  zerar();
  fx.falhasSeguidas = 2;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  assert(resolver_teste_sec() == FAROL_A_ENVIAR_REGISTRO);
  assert(strstr(resolver_teste_corpo(), FT_L2_PLAYER_EXTERNO) != NULL);
  apertar_ok();
  assert(resolver_pedido() == RESOLVER_PEDIU_PLAYER_EXT);

  // ------------------------------------------------------------ cenario 7
  zerar();
  fx.externoDisponivel = 0;
  fx.falhasSeguidas = 1;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  apertar_ok();
  assert(c_envioChamadas >= 1);

  // ------------------------------------------------------------ cenario 8
  {
    FarolEvento ev[FAROL_ANEL_N];
    int n = farolanel_ler(ev, FAROL_ANEL_N, 0), i, achou = 0;
    for (i = 0; i < n; i++)
      if (ev[i].ev == FE_FAROL_VEREDITO) achou = 1;
    assert(achou);
  }

  // ------------------------------------------------------------ cenario 9
  // CAMINHO REAL (FAROL6C, item 1): Android, player FECHADO (como Ajustes >
  // Resolver sempre abre), externo instalado, 2 falhas de formato no anel.
  // Sem resolver_teste_fatos_farol(): farol_fatos() chama o
  // player_externo_disponivel() real, o cartao oferece "Player externo", e OK
  // passa pelo app.c real -> player_abrir_externo() -> extplayer_forcar() ->
  // android_abrir_externo().
  zerar();
  aberto = 0;
  sim_abrir_tentativa("tt7654321", "movie", "Filme Externo", 0, 0,
                      "https://cdn.example/video.mkv", 0);
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  assert(player_externo_disponivel() == 1);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  assert(strstr(resolver_teste_corpo(), FT_L2_PLAYER_EXTERNO) != NULL);
  apertar_ok_app();
  assert(!resolver_aberto());
  assert(c_androidAbrirN == 1);
  assert(!strcmp(c_androidAbrirUrl, "https://cdn.example/video.mkv"));
  assert(!strcmp(c_androidAbrirTit, "Filme Externo"));
  assert(c_tocarInternoN == 0);
  assert(!saindo && !pediuSair);
  {
    FarolEvento ev[FAROL_ANEL_N];
    int n = farolanel_ler(ev, FAROL_ANEL_N, 0), achou = 0;
    for (int i = 0; i < n; i++)
      if (ev[i].ev == FE_FAROL_ACAO && !strcmp(ev[i].tok, "player_externo"))
        achou = 1;
    assert(achou);
  }

  // ----------------------------------------------------------- cenario 10
  // Guardas do player externo com player FECHADO:
  // (a) fonte que exigiu cabecalhos privados -> indisponivel -> Mandar registro;
  // (b) nenhum app externo instalado -> indisponivel;
  // (c) falha da Intent com player fechado -> devolve 0 e NAO chama interno.
  zerar();
  aberto = 0;
  sim_abrir_tentativa("tt7654321", "movie", "Filme Privado", 0, 0,
                      "https://cdn.example/priv.mkv", 1);
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  assert(player_externo_disponivel() == 0);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);

  ultimaFonteTemCabs = 0;
  c_androidApps[0] = 0;
  assert(player_externo_disponivel() == 0);

  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  // Cabecalhos de outra tela (ex. musica) nao podem derrubar fonte sem cabecalho
  // com o player fechado, e falha da Intent nao dispara tocarFonteInterno.
  snprintf(c_videoCabs, sizeof c_videoCabs, "Referer: https://www.deezer.com/\r\n");
  c_androidAbrirOk = 0;
  assert(player_externo_disponivel() == 1);
  assert(player_abrir_externo() == 0);
  assert(c_androidAbrirN == 1);
  assert(c_tocarInternoN == 0);

  // ----------------------------------------------------------- cenario 11
  // Ressalva: uma recusa sincrona emite FE_RECUSA_SINC + FE_ERRO_FONTE para o
  // mesmo erro. Deve contar 1 falha (TV_FORMATO), e so com duas falhas virar
  // TV_FORMATO_REPETE.
  zerar();
  aberto = 0;
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_FONTE, FE_RECUSA_SINC, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);

  farol_ev(FM_FONTE, FE_RECUSA_SINC, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);

  // ----------------------------------------------------------- cenario 12
  // Ressalva: "Mandar registro" nao pode emitir FE_FAROL_ACAO com token vazio.
  zerar();
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);
  apertar_ok();
  assert(c_envioChamadas >= 1);
  {
    FarolEvento ev[FAROL_ANEL_N];
    int n = farolanel_ler(ev, FAROL_ANEL_N, 0);
    for (int i = 0; i < n; i++)
      assert(!(ev[i].ev == FE_FAROL_ACAO && ev[i].tok[0] == '\0'));
  }

  // ----------------------------------------------------------- cenario 13
  // CAMINHO REAL (FAROL6C, item 2): Debrid desconectou, player FECHADO,
  // serie T2E5 falhou no add-on 1 (stream), enquanto o add-on 0 e so catalogo.
  // Resolver OK -> app.c abre addonguia_reconectar_debrid(1) -> celular envia
  // chave -> addonguia emite AGP_REPETIR -> app.c reabre T2E5 no ponto (sem
  // player_do_inicio) e renova fontes; add-on 0 nao vira AM_DEBRID; sem laco.
  zerar();
  aberto = 0;
  c_debrid = 0;
  temFixo = 1;
  snprintf(itemFixo.imdb, sizeof itemFixo.imdb, "tt1234567");
  snprintf(itemFixo.tipo, sizeof itemFixo.tipo, "series");
  snprintf(itemFixo.titulo, sizeof itemFixo.titulo, "Serie Debrid");
  epT = 2; epE = 5;
  ultimoFarolAddonId = addonmodo_id(c_addon[1].base);
  ultimoFarolDebrid = 1;
  falhar_fonte("O servidor recusou o link (HTTP 403).", "", FC_HTTP, 403, "http");
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_DEBRID_DESCONECTOU);
  assert(resolver_teste_prim() == FAROL_A_RECONECTAR_DEBRID);
  apertar_ok_app();
  assert(!resolver_aberto());
  assert(addonguia_aberto());
  assert(addonguia_identidade() == addonmodo_id(c_addon[1].base));
  {
    SDL_Event okEv = { .type = SDL_KEYDOWN };
    okEv.key.keysym.sym = SDLK_RETURN;
    addonguia_evento(&okEv);
  }
  assert(c_celbAbertoN == 1);
  snprintf(c_celbChave, sizeof c_celbChave, "CHAVE_TESTE_RD");
  avancar_addonguia();
  assert(!addonguia_aberto());
  assert(c_debridCelN == 1);
  assert(!strcmp(c_debridCelServ, "realdebrid"));
  assert(addonmodo_get(addonmodo_id(c_addon[1].base)) == AM_DEBRID);
  assert(addonmodo_get(addonmodo_id(c_addon[0].base)) == AM_AUTO);
  app_addonguia_consumir();
  assert(c_playerAbrirN == 1);
  assert(c_playerAbrirIdx == c_catReabrirIdx);
  assert(!strcmp(c_catUltimo.imdb, "tt1234567"));
  assert(c_playerDefEpN == 1 && c_playerDefT == 2 && c_playerDefE == 5);
  assert(c_playerDoInicioN == 0);
  assert(c_buscarModoN == 1 && c_buscarModoRenovar == 1);
  assert(aguardandoFonte == 1);
  assert(c_ilhaAvisosN == 0);
  // Sem laco: novos quadros nao disparam segunda reabertura.
  avancar_addonguia();
  app_addonguia_consumir();
  assert(c_playerAbrirN == 1);

  // ----------------------------------------------------------- cenario 14
  // Ressalva: lista trocada (sem add-on de stream conhecido) e add-on 0 so de
  // catalogo. O QR salva a credencial sem gravar AM_DEBRID no add-on 0 e
  // reabre o filme no ponto.
  zerar();
  aberto = 0;
  c_debrid = 0;
  temFixo = 1;
  snprintf(itemFixo.imdb, sizeof itemFixo.imdb, "tt9999999");
  snprintf(itemFixo.tipo, sizeof itemFixo.tipo, "movie");
  snprintf(itemFixo.titulo, sizeof itemFixo.titulo, "Filme Sem Lista");
  epT = 0; epE = 0;
  ultimoFarolAddonId = 0;
  c_streamAtual = -1;
  ultimoFarolDebrid = 1;
  falhar_fonte("O servidor recusou o link (HTTP 401).", "", FC_HTTP, 401, "http");
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_DEBRID_DESCONECTOU);
  apertar_ok_app();
  assert(addonguia_aberto());
  assert(addonguia_identidade() == 0);
  snprintf(c_celbChave, sizeof c_celbChave, "CHAVE_SEM_ADDON");
  avancar_addonguia();
  assert(c_debridCelN == 1);
  assert(addonmodo_get(addonmodo_id(c_addon[0].base)) == AM_AUTO);
  app_addonguia_consumir();
  assert(c_playerAbrirN == 1);
  assert(!strcmp(c_catUltimo.imdb, "tt9999999"));
  assert(c_playerDefEpN == 0);
  assert(c_buscarModoN == 1 && c_buscarModoRenovar == 1);

  // ----------------------------------------------------------- cenario 15
  // CAMINHO REAL (FAROL6C, item 2 — sem titulo): player FECHADO e nenhum
  // titulo aberto na sessao. Apos o QR, nao tenta abrir o player e avisa pela
  // ilha que a conta reconectou, sem laco.
  zerar();
  aberto = 0;
  c_debrid = 0;
  c_recusa = 1;
  temFixo = 0;
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_DEBRID_DESCONECTOU);
  apertar_ok_app();
  assert(addonguia_aberto());
  snprintf(c_celbChave, sizeof c_celbChave, "CHAVE_SEM_TITULO");
  avancar_addonguia();
  app_addonguia_consumir();
  assert(c_playerAbrirN == 0);
  assert(c_ilhaAvisosN == 1);
  assert(c_ilhaTexto[0] != '\0');
  avancar_addonguia();
  app_addonguia_consumir();
  assert(c_playerAbrirN == 0 && c_ilhaAvisosN == 1);

  // ----------------------------------------------------------- cenario 16
  // Protecao contra disparo acidental: escolher modo em Ajustes > Meus addons
  // (AG_MODO) com o player fechado nao pode emitir AGP_REPETIR nem abrir filme.
  zerar();
  aberto = 0;
  temFixo = 1;
  snprintf(itemFixo.imdb, sizeof itemFixo.imdb, "tt1111111");
  snprintf(itemFixo.tipo, sizeof itemFixo.tipo, "movie");
  addonguia_abrir(AG_MODO, 1, NULL);
  {
    SDL_Event okEv = { .type = SDL_KEYDOWN };
    okEv.key.keysym.sym = SDLK_RETURN;
    addonguia_evento(&okEv);
  }
  avancar_addonguia();
  app_addonguia_consumir();
  assert(c_playerAbrirN == 0 && c_ilhaAvisosN == 0);

  // ----------------------------------------------------------- cenario 17
  // BLOQUEIO r3 (FAROL6D): episodio A falha por formato e fecha; filme B abre,
  // chega a FE_PRONTO e fecha. Resolver que diagnostica A JAMAIS pode oferecer
  // ou entregar URL/titulo de B a Intent de Player externo — usa a alternativa.
  zerar();
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  sim_abrir_tentativa("tt1111111", "series", "Episodio A", 1, 3,
                      "https://cdn.example/epA.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  aberto = 0;

  // Agora o filme B abre, chega a FE_PRONTO e fecha.
  sim_abrir_tentativa("tt2222222", "movie", "Filme B", 0, 0,
                      "https://cdn.example/filmeB.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  farol_ev(FM_PLAYER, FE_PRONTO, 0, 250, 0, NULL);
  aberto = 0;

  assert(player_externo_disponivel() == 0);
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

  // ----------------------------------------------------------- cenario 18
  // A->B mesmo SEM FE_PRONTO (B abriu apos 2 falhas em A e fechou antes do
  // primeiro quadro): TV_FORMATO_REPETE tambem cai na alternativa e nao entrega B.
  zerar();
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  sim_abrir_tentativa("tt1111111", "series", "Episodio A", 1, 3,
                      "https://cdn.example/epA.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  aberto = 0;

  sim_abrir_tentativa("tt2222222", "movie", "Filme B", 0, 0,
                      "https://cdn.example/filmeB.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  aberto = 0;

  assert(player_externo_disponivel() == 0);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);
  assert(c_envioChamadas >= 1);

  // ----------------------------------------------------------- cenario 19
  // Cenario positivo (1 falha de formato, TV_FORMATO -> acaoAlt PLAYER_EXTERNO)
  // com player fechado: tentativa correta ainda e a guardada -> oferece e abre A.
  zerar();
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  sim_abrir_tentativa("tt1111111", "series", "Episodio A", 1, 3,
                      "https://cdn.example/epA.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  aberto = 0;
  assert(player_externo_disponivel() == 1);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  assert(strstr(resolver_teste_corpo(), FT_L2_PLAYER_EXTERNO) != NULL);
  apertar_ok_app();
  assert(!resolver_aberto());
  assert(c_androidAbrirN == 1);
  assert(!strcmp(c_androidAbrirUrl, "https://cdn.example/epA.mkv"));
  assert(!strcmp(c_androidAbrirTit, "Episodio A"));
  assert(c_tocarInternoN == 0);

  // ----------------------------------------------------------- cenario 20
  // Troca entre oferta e OK: Resolver diagnostica A e oferece "Player externo";
  // antes do OK, B abre (e/ou chega a FE_PRONTO). O despacho recusa B, nao
  // grava FE_FAROL_ACAO player_externo, troca o cartao para a alternativa e
  // o novo OK envia o registro.
  zerar();
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  sim_abrir_tentativa("tt1111111", "series", "Episodio A", 1, 3,
                      "https://cdn.example/epA.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  aberto = 0;
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
  uint64_t tentA = player_ultimo_erro_tentativa();
  assert(tentA != 0);
#endif
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);

  // Troca de tentativa entre a oferta na tela e o aperto de OK:
  sim_abrir_tentativa("tt2222222", "movie", "Filme B", 0, 0,
                      "https://cdn.example/filmeB.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  farol_ev(FM_PLAYER, FE_PRONTO, 0, 200, 0, NULL);
  aberto = 0;

  apertar_ok_app();
  assert(c_androidAbrirN == 0);
  assert(resolver_aberto());
  assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);
  assert(player_abrir_externo() == 0);
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
  assert(player_abrir_externo_tentativa(tentA) == 0);
#endif
  assert(c_androidAbrirN == 0);
  {
    FarolEvento ev[FAROL_ANEL_N];
    int n = farolanel_ler(ev, FAROL_ANEL_N, 0);
    for (int i = 0; i < n; i++)
      assert(!(ev[i].ev == FE_FAROL_ACAO && !strcmp(ev[i].tok, "player_externo")));
  }
  apertar_ok_app();
  assert(c_envioChamadas >= 1);
  assert(c_androidAbrirN == 0);

  // ----------------------------------------------------------- cenario 21
  // BLOQUEIO r4 (item 2): recusa de player_definir_fonte por travaFonte NAO
  // pode revalidar erro/URL da fonte anterior do episodio.
  zerar();
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  sim_abrir_tentativa("tt3333333", "series", "Serie Travada", 1, 3,
                      "https://cdn.example/s1e3_antiga.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  uint64_t tentAnterior = player_ultimo_erro_tentativa();
  assert(tentAnterior != 0);
  aberto = 1;
  c_travaBloq = 1;
  snprintf(c_travaImdb, sizeof c_travaImdb, "tt3333333");
  c_travaT = 1; c_travaE = 3;
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
  farol_test_definir_fonte_prefixo("https://cdn.example/s1e3_nova.mkv");
#endif
  aberto = 0;
  assert(player_externo_disponivel() == 0);
  assert(player_externo_disponivel_tentativa(tentAnterior) == 0);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_prim() != FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);

  // ----------------------------------------------------------- cenario 22
  // BLOQUEIO r4 (item 2): trava passa a bloquear ENTRE oferta e OK (com player
  // fechado e catalogo/itemFixo deslocados). Candidato conserva identidade e
  // episodio; disponibilidade e despacho recusam, sem chamar JNI.
  zerar();
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  sim_abrir_tentativa("tt4444444", "series", "Serie Sinc", 2, 5,
                      "https://cdn.example/s2e5.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  aberto = 0;
  // Mesmo com itemFixo/epT/epE limpos ou deslocados fora do player, o candidato
  // conserva os dados do episodio.
  temFixo = 0; memset(&itemFixo, 0, sizeof itemFixo); epT = 0; epE = 0;
  uint64_t tentSinc = player_ultimo_erro_tentativa();
  assert(tentSinc != 0);
  assert(player_externo_disponivel() == 1);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);

  // Sincronizacao em segundo plano ativa a trava de serie apos a oferta.
  c_travaBloq = 1;
  snprintf(c_travaImdb, sizeof c_travaImdb, "tt4444444");
  c_travaT = 2; c_travaE = 5;
  assert(player_externo_disponivel_tentativa(tentSinc) == 0);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);
  assert(resolver_aberto());
  assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);
  assert(player_abrir_externo_tentativa(tentSinc) == 0);
  assert(c_androidAbrirN == 0);
  apertar_ok_app();
  assert(c_envioChamadas >= 1);
  assert(c_androidAbrirN == 0);

  // ----------------------------------------------------------- cenario 23
  // Autorizacao manual legitima da trava ("Assistir mesmo assim"): episodio
  // liberado segue elegivel com player fechado e entrega titulo/URL corretos.
  zerar();
  snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
  c_travaBloq = 1;
  snprintf(c_travaImdb, sizeof c_travaImdb, "tt5555555");
  c_travaT = 1; c_travaE = 2;
  c_travaManualAutorizada = 1;
  sim_abrir_tentativa("tt5555555", "series", "Serie Autorizada", 1, 2,
                      "https://cdn.example/s1e2_ok.mkv", 0);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
               FC_FORMATO, 0, "m3_4003");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
  aberto = 0;
  assert(player_externo_disponivel() == 1);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
  apertar_ok_app();
  assert(!resolver_aberto());
  assert(c_androidAbrirN == 1);
  assert(!strcmp(c_androidAbrirUrl, "https://cdn.example/s1e2_ok.mkv"));
  assert(!strcmp(c_androidAbrirTit, "Serie Autorizada"));
  assert(c_tocarInternoN == 0);

  // ----------------------------------------------------------- cenario 24
  // FAROL6-r5 achado 4: indice da serie reavaliado pela identidade antes da
  // trava na oferta e na confirmacao, inclusive com player aberto ou mini e
  // catalogo reordenado (slot antigo agora tem 2 eps em T1; serie real com 3
  // eps moveu para slot 1 e deve bloquear T2E1).
  for (int modo = 0; modo < 2; modo++) {
    zerar();
    snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
    c_catSlotCount = 2;
    snprintf(c_catSlots[0].imdb, sizeof c_catSlots[0].imdb, "tt6666666");
    c_catSlots[0].nEpsT1 = 3;
    snprintf(c_catSlots[1].imdb, sizeof c_catSlots[1].imdb, "tt9999998");
    c_catSlots[1].nEpsT1 = 2;
    c_catIdxAtual = 0;
    aberto = modo == 0 ? 1 : 0;
    mini = modo == 1 ? 1 : 0;
    c_travaBloq = 1;
    snprintf(c_travaImdb, sizeof c_travaImdb, "tt6666666");
    sim_abrir_tentativa("tt6666666", "series", "Serie Reordenada", 2, 1,
                        "https://cdn.example/s2e1.mkv", 0);
    farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
    falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
                 FC_FORMATO, 0, "m3_4003");
    farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
    uint64_t tentReord = player_ultimo_erro_tentativa();
    assert(tentReord != 0);
#endif
    // Catalogo reordena mantendo o player aberto/mini: slot 0 fica com serie
    // de 2 episodios em T1, e tt6666666 (3 eps) move para slot 1.
    snprintf(c_catSlots[0].imdb, sizeof c_catSlots[0].imdb, "tt9999998");
    c_catSlots[0].nEpsT1 = 2;
    snprintf(c_catSlots[1].imdb, sizeof c_catSlots[1].imdb, "tt6666666");
    c_catSlots[1].nEpsT1 = 3;
    c_catIdxAtual = 1;
    assert(player_externo_disponivel() == 0);
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
    assert(player_externo_disponivel_tentativa(tentReord) == 0);
#endif
    resolver_abrir();
    resolver_teste_testar(RES_S_NAO_COMECA);
    assert(resolver_teste_prim() != FAROL_A_PLAYER_EXTERNO);
    apertar_ok_app();
    assert(c_androidAbrirN == 0);
#ifdef FAROL_BRIDGE_TEM_TENTATIVA
    assert(player_abrir_externo_tentativa(tentReord) == 0);
#endif
    assert(c_androidAbrirN == 0);

    // Tambem na confirmacao: oferta inicial sem trava (ou antes da realocacao),
    // catalogo reordena e trava passa a bloquear entre oferta e OK.
    zerar();
    snprintf(c_androidApps, sizeof c_androidApps, "org.videolan.vlc\tVLC\n");
    c_catSlotCount = 2;
    snprintf(c_catSlots[0].imdb, sizeof c_catSlots[0].imdb, "tt6666666");
    c_catSlots[0].nEpsT1 = 3;
    snprintf(c_catSlots[1].imdb, sizeof c_catSlots[1].imdb, "tt9999998");
    c_catSlots[1].nEpsT1 = 2;
    c_catIdxAtual = 0;
    aberto = modo == 0 ? 1 : 0;
    mini = modo == 1 ? 1 : 0;
    c_travaBloq = 0;
    snprintf(c_travaImdb, sizeof c_travaImdb, "tt6666666");
    sim_abrir_tentativa("tt6666666", "series", "Serie Reordenada Conf", 2, 1,
                        "https://cdn.example/s2e1.mkv", 0);
    farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
    falhar_fonte("A TV não conseguiu tocar o formato desta fonte.", "",
                 FC_FORMATO, 0, "m3_4003");
    farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, FS_CAUSA_FORMATO, 0, "m3_4003");
    resolver_abrir();
    resolver_teste_testar(RES_S_NAO_COMECA);
    assert(resolver_teste_prim() == FAROL_A_PLAYER_EXTERNO);
    snprintf(c_catSlots[0].imdb, sizeof c_catSlots[0].imdb, "tt9999998");
    c_catSlots[0].nEpsT1 = 2;
    snprintf(c_catSlots[1].imdb, sizeof c_catSlots[1].imdb, "tt6666666");
    c_catSlots[1].nEpsT1 = 3;
    c_catIdxAtual = 1;
    c_travaBloq = 1;
    apertar_ok_app();
    assert(c_androidAbrirN == 0);
    assert(resolver_aberto());
    assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);
    assert(player_abrir_externo() == 0);
    assert(c_androidAbrirN == 0);
  }
  c_catSlotCount = 0;
  mini = 0;

  puts("farol_resolver: producao ok");
  return 0;
}
