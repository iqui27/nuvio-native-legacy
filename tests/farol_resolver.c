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
//   - cenario 21 (Farol #17): ADDON_LENTO com duas fontes -> o OK isola o lento
//     por 24 h com farolacao_addon.c e addonstats.c de verdade (anel, ilha com
//     Desfazer), sem enviar registro e sem pedido ao app.
//   - cenario 22: ADDON_LENTO com uma fonte so -> nao oferece isolar; cai no registro.
//   - cenarios FAROL34B (issue #34): as 4 acoes de rede saem como PEDIU_FAROL
//     para farolacao_rede_executar (toque vs segurar no Desligar o HDR),
//     e o cartao IMAGEM_DV que antes caia no envio do registro agora executa.

#include "resolver.h"
#include "resolverregra.h"
#include "farolregra.h"
#include "farolerro.h"
#include "farolanel.h"
#include "faroltexto.h"
#include "farolsonda.h"
#include "farol.h"
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
#include "ilhaacao.h"
#include "addonstats.h"
#include "farolacao_addon.h"
#include "p2pmotor.h"
#include "perfis.h"
#include "plrilha.h"
#include "gfx.h"
#include "text.h"
#include "ponteiro.h"
#include "layout.h"
#include "segurar.h"
#include "farolacao_rede.h"
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
static uint64_t ultimoFarolSeq;   // espelho do static do player (#14)
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
static int c_quarentenaGravada;
int dados_gravar_leve(const char *n, const char *v) {
  (void)v;
  if (n && !strcmp(n, "fonte-quarentena.txt")) c_quarentenaGravada++;
  return 1;
}
void aio_resultados(AioSnapshot *s, const char *json) { (void)s; (void)json; }   // addonstats.c sem AIOStreams
// Farol #17: o mundo de add-ons que farolacao_addon.c (o de verdade) consulta;
// a quarentena e a de addonstats.c (tambem o de verdade).
static int c_lento = -1;
int addons_chave_estatistica(int i, char chave[32]) {
  if (i < 0 || i >= c_addons) return 0;
  snprintf(chave, 32, "fx-%d", i);
  return 1;
}
static int c_isolado(int i) {
  char c[32];
  return addons_chave_estatistica(i, c) && addonstats_isolado_s(c) > 0;
}
int addons_fontes_chaves(char (*chaves)[32], int max) {
  int k = 0;
  for (int i = 0; i < c_addons && k < max; i++)
    if (c_addon[i].ativo && c_addon[i].forneceStream) addons_chave_estatistica(i, chaves[k++]);
  return k;
}
int addons_fontes_vivas_n(void) {
  int k = 0;
  for (int i = 0; i < c_addons; i++)
    if (c_addon[i].ativo && c_addon[i].forneceStream && !c_isolado(i)) k++;
  return k;
}
int addons_lento_isolavel(void) {
  return c_lento >= 0 && c_lento < c_addons && c_addon[c_lento].forneceStream && !c_isolado(c_lento) ? c_lento : -1;
}
// weak: quando o teste linkar a fachada src/farol.c, valem as dela (mesmos eventos).
__attribute__((weak)) void farol_registrar_acao(int a) { farol_ev(FM_FAROL, FE_FAROL_ACAO, 0, a, 0, farolregra_acao_token(a)); }
__attribute__((weak)) void farol_registrar_resultado(int a, int ok) {
  farol_ev(FM_FAROL, FE_FAROL_RESULTADO, ok ? 0 : 1, a, ok ? 1 : 0, farolregra_acao_token(a));
}
static IlhaAcao c_ilhaAcao;
static char c_ilhaAcaoCtx[64];
static int c_ilhaAcaoN;
void ilhaacao_feita(const IlhaAcao *a) {
  assert(a->ctxN <= sizeof c_ilhaAcaoCtx);
  c_ilhaAcao = *a;
  memcpy(c_ilhaAcaoCtx, a->ctx, a->ctxN);
  c_ilhaAcao.ctx = c_ilhaAcaoCtx;
  c_ilhaAcaoN++;
}
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
// tests/farol_tela.c mexe nestes dois: Interface de vidro e animacoes.
static int c_vidro, c_reduzidas = 1, c_vidroPaineis;
int ajustes_vidro(void) { return c_vidro; }
int ajustes_animacoes_reduzidas(void) { return c_reduzidas; }
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
// FAROL34B: a dica do OK segurado ("sempre nesta TV") e o vencer do segurar.
static int c_ilhaAtvN;
static char c_ilhaAtvTxt[256];
void ilha_atividade(const char *texto, float progresso) {
  (void)progresso;
  c_ilhaAtvN++;
  snprintf(c_ilhaAtvTxt, sizeof c_ilhaAtvTxt, "%s", texto ? texto : "");
}
int segurar_venceu(Uint32 agora, Uint32 desde, const char *tela) {
  (void)tela;
  return desde != 0 && (Sint32)(agora - desde) >= (Sint32)NV_HOLD_MS;
}
// FAROL34C P1-3: -Wl,--wrap faz o resolver_evento() de src/resolver.c ler o
// clock DAQUI. comTicksZero arma o KEYDOWN com SDL_GetTicks()==0 (1o quadro
// de um processo real) — sem o `if (!t) t = 1` a pressao longa nunca vencia.
static int comTicksZero;
Uint32 __real_SDL_GetTicks(void);
Uint32 __wrap_SDL_GetTicks(void) {
  return comTicksZero ? 0u : __real_SDL_GetTicks();
}
// FAROL34B: o despacho real de app.c (farol_prod_bridge.inc) chega aqui —
// a prova de que RESOLVER_PEDIU_FAROL executa a acao escolhida.
static int c_farolExecN, c_farolExecAcao, c_farolExecPersistir;
int farolacao_rede_executar(int acao, int persistir) {
  c_farolExecN++; c_farolExecAcao = acao; c_farolExecPersistir = persistir;
  return 1;
}

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
static void txt_anotar(const char *s);
void botao_pilula(GfxRect r, const char *rot, const char *ic, float f, int p, int al, float a) { (void)r; (void)ic; (void)f; (void)p; (void)al; (void)a; txt_anotar(rot); }
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
// O que o desenho escreveu no quadro (tests/farol_tela.c confere textos).
static char c_txt[160][200];
static int c_txtN;
static void txt_anotar(const char *s) {
  if (s && c_txtN < (int)(sizeof c_txt / sizeof c_txt[0]))
    snprintf(c_txt[c_txtN++], sizeof c_txt[0], "%s", s);
}
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) { TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)r; (void)g; (void)b; (void)a; txt_anotar(s); return l; }
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float mw) { TxtLinha l; memset(&l, 0, sizeof l); (void)e; (void)r; (void)g; (void)b; (void)a; (void)mw; txt_anotar(s); return l; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) { (void)e; if (a > 0.0f) txt_anotar(s); (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml; return 0.0f; }
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float ld, float a, int ml) { (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml; return 0.0f; }
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float a, float t) { (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)t; return 0.0f; }

// ------------------------------------------- fachada do Farol (issue 19)
// src/farol.c entra de verdade: o Resolver julga por farol_julgar. O lado da
// Home (cena, catalogo, gosto) fica mudo como em tests/farol_fachada.c; os
// produtores que a cadeia le saem dos mesmos espelhos do cenario.
int home_fileiras_na_tela(void) { return 0; }
const char *app_tela_nome(void) { return "resolver"; }
int recomenda_pessoa_canonica(const char *id, char *dst, size_t tam) {
  (void)id; if (dst && tam) dst[0] = 0; return 0;
}
int cat_indice_por_imdb(const char *imdb) { (void)imdb; return -1; }
int cat_copiar_item(int idx, CatItem *dst) { (void)idx; (void)dst; return 0; }
int cat_copiar_por_id(const char *id, const char *tipo, CatItem *dst) { (void)id; (void)tipo; (void)dst; return 0; }
int cat_fileira_imdb(const char *chave, char (*out)[64], int max, int stride) {
  (void)chave; (void)out; (void)max; (void)stride; return 0;
}
const CatItem *cat_item(int i) { (void)i; return NULL; }
int cat_n(void) { return 0; }
unsigned cat_revisao(void) { return 0; }
int cat_visto(const CatItem *ci) { (void)ci; return 0; }
int cat_remover_fileiras_prefixo(const char *d, const char *pf) { (void)d; (void)pf; return 0; }
int farolcena_home(char *dst, size_t cap, const void *ctx, void *a, size_t b) {
  (void)dst; (void)cap; (void)ctx; (void)a; (void)b; return 0;
}
int farolpeca_ler(void *pc, const char *j, int nc) { (void)pc; (void)j; (void)nc; return 0; }
int farolpeca_texto_tem(const void *t) { (void)t; return 0; }
const char *farolpeca_x(const void *c, const void *t) { (void)c; (void)t; return ""; }
int farolrec_nivel(const void *g) { (void)g; return 0; }
int farolficha_ler_item(const CatItem *ci, void *dst) { (void)ci; (void)dst; return 0; }
uint32_t farolficha_chave(const void *f) { (void)f; return 0; }
int farolsinal_modelo(void *dst) { (void)dst; return 0; }
int farolsinal_ficha_do_item(const CatItem *ci, void *dst) { (void)ci; (void)dst; return 0; }
void farolsinal_mostrado(const CatItem *ci) { (void)ci; }
int farolsinal_recomendar(void) { return 0; }
void farolsinal_zerar_gosto(void) {}
void farolsinal_definir_recomendar(int ligado) { (void)ligado; }
int seguro_perfil_ativo(void) { return 0; }
int perfil_infantil_ativo(void) { return 0; }
int perfis_ativo(void) { return 1; }
int fil_oculta(const char *chave) { (void)chave; return 0; }
int diario_n(int perfil) { (void)perfil; return 0; }
int diario_entrada(int perfil, int i, void *dst) { (void)perfil; (void)i; (void)dst; return 0; }
int diario_titulo(int perfil, const char *id, void *dst) { (void)perfil; (void)id; (void)dst; return 0; }
int diario_nome(const char *id, char *t, size_t nt, int *a, char *pn, size_t np) {
  (void)id; if (t && nt) t[0] = 0; (void)a; if (pn && np) pn[0] = 0; return 0;
}
int salvos_n(void) { return 0; }
const void *salvos_item(int i) { (void)i; return NULL; }
int32_t mapa_hash_nome(const char *x) { (void)x; return -1; }
uint32_t mapa_hash_titulo(const char *x) { (void)x; return 1; }
const char *atualizacao_nova(void) { return ""; }
int nv_webos_major(void) { return 0; }
int dts_playback_enabled(void) { return 0; }
int dts_tpk_disponivel(void) { return 0; }
int video_audio_nao_suportado(void) { return 0; }
int video_n_audio(void) { return 0; }
typedef struct VideoFaixa VideoFaixa;
const VideoFaixa *video_audio(int i) { (void)i; return NULL; }
int video_audio_atual(void) { return -1; }
int video_faixa_dts(const VideoFaixa *f) { (void)f; return 0; }
int video_dts_estado(void) { return 0; }
int video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void) { return ""; }
#ifndef FAROL_SONDA_REAL
// A sonda ativa (farolsonda.h): pronta = nada no ar.
static int c_sondaPronta = 1, c_sondaCanceladaN, c_sondaIniciadaN, c_sondaIniciarAuto;
int farolsonda_iniciar(const char *url, const char *const *cab) {
  (void)cab;
  c_sondaIniciadaN++;
  if (!url || !*url) return 0;
  if (c_sondaIniciarAuto && c_sondaPronta) {
    c_sondaPronta = 0;
    return 1;
  }
  return 0;
}
int farolsonda_estado(FarolSonda *o) {
  if (o) { memset(o, 0, sizeof *o); o->http = 0; }
  return c_sondaPronta;
}
void farolsonda_cancelar(void) { c_sondaCanceladaN++; c_sondaPronta = 1; }
#endif
// A fonte atual como o Resolver do pai lia (stream_item(stream_atual())).
void stream_farol_fatos(int *fonteDv, int *fonteHdr, int *fonteGBx10,
                        int *fonteMbps, int *fonteDebrid, int *temOutraFonte,
                        int *temFonteMenor, int *temFonteSemDv,
                        int *temFonteSemDts, int *tvHdr, int *tvDv,
                        int *vazaoMbps) {
  const Stream *ft = stream_atual() >= 0 ? stream_item(stream_atual()) : NULL;
  *fonteDv = *fonteHdr = *fonteGBx10 = *fonteMbps = *fonteDebrid = -1;
  *temOutraFonte = *temFonteMenor = *temFonteSemDv = *temFonteSemDts = -1;
  *tvHdr = *tvDv = *vazaoMbps = -1;
  if (ft) {
    uint64_t g = ft->tamanhoBytes / 100000000ULL;
    int kbps = stream_kbps_atual();
    *fonteDv = ft->dolbyVision > 0 ? 1 : 0;
    *fonteGBx10 = !ft->tamanhoBytes ? -1 : g > INT32_MAX ? INT32_MAX : (int)g;
    *fonteMbps = kbps > 0 ? kbps / 1000 : -1;
    *fonteDebrid = ft->debridServico > 0 ? 1 : 0;
  }
}
void gfx_vidro_painel(GfxRect r, float raio, float fundo, float a) { (void)r; (void)raio; (void)fundo; (void)a; c_vidroPaineis++; }

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
  ultimoFarolSeq = 0;
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
  c_lento = -1; c_ilhaAcaoN = 0; c_quarentenaGravada = 0;
  addonstats_isolar_zerar();
  c_celbDisp = 1; c_celbAbertoN = c_celbFechadoN = 0; c_celbChave[0] = 0;
  c_debridCelN = 0; c_debridCelServ[0] = c_debridCelChave[0] = 0;
  c_ilhaAvisosN = 0; c_ilhaChave[0] = c_ilhaTexto[0] = 0;
  c_ilhaAtvN = 0; c_ilhaAtvTxt[0] = 0;
  c_farolExecN = 0; c_farolExecAcao = 0; c_farolExecPersistir = 0;
  tela = TELA_AJUSTES;
  guiaTorrentRepetir = 0; torrentSessao = 0; torrentPendente = -1; aguardandoFonte = 0;
  c_playerAbrirN = 0; c_playerAbrirIdx = -1; c_playerDoInicioN = 0;
  c_playerDefEpN = 0; c_playerDefT = c_playerDefE = 0;
  c_buscarModoN = 0; c_buscarModoRenovar = 0;
  c_menuFechouN = 0; c_ajustesFontesN = 0; c_travauiBloq = 0;
  c_catReabrirIdx = 7;
#ifndef FAROL_SONDA_REAL
  c_sondaPronta = 1; c_sondaCanceladaN = 0; c_sondaIniciadaN = 0; c_sondaIniciarAuto = 0;
#endif
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
  // Como no player, o erro e gravado DEPOIS de um evento da tentativa no anel
  // (a fonte abriu): a fachada (issue 19) descarta erro com seq na fronteira
  // da sessao, e sem evento nenhum a seq do erro seria a propria fronteira.
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
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

// O OK longo executa no KEYUP: depois de apertar_ok(), soltar conclui o
// toque (ou a versao duravel, se o segurar ja venceu no atualizar).
static void soltar_ok(void) {
  SDL_Event evento = { .type = SDL_KEYUP };
  evento.key.keysym.sym = SDLK_RETURN;
  resolver_evento(&evento);
}

static void soltar_ok_app(void) {
  SDL_Event evento = { .type = SDL_KEYUP };
  evento.key.keysym.sym = SDLK_RETURN;
  app_resolver_despachar(&evento);
}

static void avancar_addonguia(void) {
  for (int i = 0; i < 40; i++) {
    addonguia_shot_passo(0.05f);
    addonguia_atualizar(0);
  }
}

#ifndef FAROL_RESOLVER_SEM_MAIN
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
  // FAROL34B (issue #34): "Desligar o HDR" executa de verdade. O toque (sem
  // segurar vencer) sai como PEDIU_FAROL com persistir=0 — e NAO mais cai no
  // envio do registro, que era o botao morto apontado na revisao.
  assert(resolver_teste_prim() == FAROL_A_DESLIGAR_HDR);
  apertar_ok();
  assert(resolver_aberto());           // toque ainda nao acabou: falta o KEYUP
  soltar_ok();
  assert(resolver_pedido() == RESOLVER_PEDIU_FAROL);
  assert(resolver_pedido_acao() == FAROL_A_DESLIGAR_HDR);
  assert(resolver_pedido_persistir() == 0);
  assert(!resolver_aberto());
  assert(c_envioChamadas == 0);

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
  // FAROL34B: TV_FORMATO_REPETE sem player externo cai na alternativa
  // LIMPAR_CACHE — que agora executa (antes cairia no registro).
  assert(resolver_teste_prim() == FAROL_A_LIMPAR_CACHE);

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
  // FAROL34B: a alternativa LIMPAR_CACHE executa — nao cai mais no registro.
  assert(resolver_teste_prim() == FAROL_A_LIMPAR_CACHE);
  apertar_ok_app();
  assert(c_androidAbrirN == 0);
  assert(c_farolExecN == 1 && c_farolExecAcao == FAROL_A_LIMPAR_CACHE);
  assert(c_envioChamadas == 0);

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
  assert(resolver_teste_prim() == FAROL_A_LIMPAR_CACHE);
  assert(player_abrir_externo_tentativa(tentSinc) == 0);
  assert(c_androidAbrirN == 0);
  apertar_ok_app();
  // FAROL34B: a alternativa LIMPAR_CACHE executa (era Mandar registro).
  assert(c_farolExecN == 1 && c_farolExecAcao == FAROL_A_LIMPAR_CACHE);
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

  // ------------------------------------------------------------ cenario 21
  // Farol #17: o OK no veredito ADDON_LENTO isola o add-on (nao envia registro).
  {
    FarolEvento ev[FAROL_ANEL_N];
    char ch[32];
    int n, acoes = 0, s;
    zerar();
    c_lento = 1;                                       // Torrentio lento; MediaFusion segue
    memset(&fx, 0, sizeof fx);
    farolregra_fatos_iniciar(&fx);
    fx.plat = FP_WEBOS;
    fx.addonsQtd = 2;
    fx.addonsLentos = 1;
    fx.addonsMudos = 0;
    resolver_teste_fatos_farol(&fx);
    resolver_abrir();
    resolver_teste_testar(RES_S_TRAVA);
    assert(resolver_teste_veredito() == FAROL_C_ADDON_LENTO);
    assert(resolver_teste_prim() == FAROL_A_ISOLAR_ADDON);
    farolanel_zerar();
    apertar_ok();
    assert(c_envioChamadas == 0);                      // nada de registro
    assert(resolver_pedido() == RESOLVER_PEDIU_NADA);
    assert(!resolver_aberto());                        // fecha: a ilha mostra o Desfazer
    assert(addons_chave_estatistica(1, ch));
    assert(addonstats_isolado_s(ch) == FAAD_ISOLA_S && !c_isolado(2));
    assert(addons_fontes_vivas_n() == 1 && c_quarentenaGravada == 1);
    n = farolanel_ler(ev, FAROL_ANEL_N, 0);
    for (int i = 0; i < n; i++) {
      if (ev[i].ev == FE_FAROL_ACAO) { assert(ev[i].a == FAROL_A_ISOLAR_ADDON); acoes++; }
      if (ev[i].ev == FE_FAROL_RESULTADO) assert(ev[i].a == FAROL_A_ISOLAR_ADDON && ev[i].b == 1);
    }
    assert(acoes == 1);                                // uma acao so no anel
    assert(c_ilhaAcaoN == 1 && c_ilhaAcao.desfazer && !strcmp(c_ilhaAcao.titulo, "Torrentio"));
    assert(!strcmp(c_ilhaAcao.onde, FT_ADDON_DE_LADO));
    c_ilhaAcao.desfazer(c_ilhaAcao.ctx);               // o "Desfazer" da ilha
    assert(!c_isolado(1) && addonstats_isolado_s(ch) == 0 && c_quarentenaGravada == 2);
    // Ja ha 3 de lado (teto): o cartao nao promete isolar.
    zerar();
    c_lento = 1;
    resolver_teste_fatos_farol(&fx);
    assert(addonstats_isolar("x-1", 600) && addonstats_isolar("x-2", 600) && addonstats_isolar("x-3", 600));
    resolver_abrir();
    resolver_teste_testar(RES_S_TRAVA);
    assert(resolver_teste_veredito() == FAROL_C_ADDON_LENTO);
    assert(resolver_teste_prim() != FAROL_A_ISOLAR_ADDON);
    // Oferecido, mas outro fio isolou antes do OK: recusa, testa de novo, sem registro.
    zerar();
    c_lento = 1;
    resolver_teste_fatos_farol(&fx);
    resolver_abrir();
    resolver_teste_testar(RES_S_TRAVA);
    assert(resolver_teste_prim() == FAROL_A_ISOLAR_ADDON);
    assert(addons_chave_estatistica(2, ch) && addonstats_isolar(ch, 600));   // MediaFusion saiu
    apertar_ok();
    assert(c_envioChamadas == 0 && c_ilhaAcaoN == 0 && resolver_aberto());
    assert(!c_isolado(1));                             // o ultimo de fontes ficou
    assert(resolver_teste_prim() != FAROL_A_ISOLAR_ADDON);
    (void)s;
  }

  // ------------------------------------------------------------ cenario 22
  // Uma fonte so: isolar deixaria zero; o cartao cai no registro.
  zerar();
  c_lento = 1;
  c_addon[2].forneceStream = 0;
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_WEBOS;
  fx.addonsQtd = 2;
  fx.addonsLentos = 1;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_TRAVA);
  assert(resolver_teste_veredito() == FAROL_C_ADDON_LENTO);
  assert(resolver_teste_prim() == FAROL_A_ENVIAR_REGISTRO);
  apertar_ok();
  assert(c_envioChamadas >= 1 && !c_isolado(1) && c_ilhaAcaoN == 0);

  // ----------------------------------------------------------- cenario 25
  // FAROL34B (issue #34): REDE_NAO_CHEGA (erro FC_REDE recente, sem offline)
  // -> prim "Reconectar a rede" -> PEDIU_FAROL -> app.c chama
  // farolacao_rede_executar(RECONECTAR_REDE, 0). Prova o despacho de producao
  // inteiro (o bloco extraido em farol_prod_bridge.inc).
  zerar();
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_WEBOS;
  fx.offline = 0;
  fx.erro.classe = FC_REDE;
  fx.erroIdadeS = 30;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_REDE_NAO_CHEGA);
  assert(resolver_teste_prim() == FAROL_A_RECONECTAR_REDE);
  apertar_ok_app();                     // nao e celula longa: executa no KEYDOWN
  assert(!resolver_aberto());
  assert(c_farolExecN == 1);
  assert(c_farolExecAcao == FAROL_A_RECONECTAR_REDE);
  assert(c_farolExecPersistir == 0);

  // ----------------------------------------------------------- cenario 26
  // FAROL34B: PLAYER_PAROU com vazao suficiente -> prim "Buffer maior" ->
  // executar(BUFFER_MAIOR, 0); secundario escala JUNTO = Mandar registro.
  zerar();
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_WEBOS;
  fx.erro.classe = FC_PARADO;
  fx.erroIdadeS = 30;
  fx.fonteMbps = 5;
  fx.fonteGBx10 = 0;
  fx.vazaoMbps = 5;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_PLAYER_PAROU);
  assert(resolver_teste_prim() == FAROL_A_BUFFER_MAIOR);
  assert(resolver_teste_sec() == FAROL_A_ENVIAR_REGISTRO);
  apertar_ok_app();
  assert(!resolver_aberto());
  assert(c_farolExecN == 1);
  assert(c_farolExecAcao == FAROL_A_BUFFER_MAIOR);
  assert(c_farolExecPersistir == 0);

  // ----------------------------------------------------------- cenario 27
  // FAROL34B: TV_FORMATO_REPETE sem player externo (webOS): a regra oferece
  // FONTE_OUTRO_FORMATO (sem executor) e o cartao cai na ALTERNATIVA
  // LIMPAR_CACHE — que executa, nao vai mais para o envio de registro.
  zerar();
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_WEBOS;
  fx.erro.classe = FC_FORMATO;
  fx.erroIdadeS = 30;
  fx.falhasSeguidas = 2;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(RES_S_NAO_COMECA);
  assert(resolver_teste_veredito() == FAROL_C_TV_FORMATO_REPETE);
  assert(resolver_teste_prim() == FAROL_A_LIMPAR_CACHE);
  apertar_ok_app();
  assert(!resolver_aberto());
  assert(c_farolExecN == 1);
  assert(c_farolExecAcao == FAROL_A_LIMPAR_CACHE);
  assert(c_farolExecPersistir == 0);

  // ----------------------------------------------------------- cenario 28
  // FAROL34B: segurar OK em "Desligar o HDR" sobe a dica da versao duravel
  // e o KEYUP depois do vencido executa persistir=1 ("sempre nesta TV").
  // O toque do cenario 3 provou persistir=0.
  zerar();
  memset(&fx, 0, sizeof fx);
  farolregra_fatos_iniciar(&fx);
  fx.plat = FP_WEBOS;
  fx.fonteDv = 1;
  fx.tvDv = 0;
  resolver_teste_fatos_farol(&fx);
  resolver_abrir();
  resolver_teste_testar(FAROL_S_CORES);
  assert(resolver_teste_veredito() == FAROL_C_IMAGEM_DV);
  assert(resolver_teste_prim() == FAROL_A_DESLIGAR_HDR);
  comTicksZero = 1;
  apertar_ok();                          // KEYDOWN: arma com SDL_GetTicks()==0
  comTicksZero = 0;
  assert(resolver_aberto());
  resolver_atualizar(0.016f, SDL_GetTicks() + NV_HOLD_MS + 16);
  assert(c_ilhaAtvN >= 1);
  assert(strstr(c_ilhaAtvTxt, "sempre") != NULL);
  soltar_ok();                           // KEYUP depois do vencido: duravel
  assert(resolver_pedido() == RESOLVER_PEDIU_FAROL);
  assert(resolver_pedido_acao() == FAROL_A_DESLIGAR_HDR);
  assert(resolver_pedido_persistir() == 1);
  assert(!resolver_aberto());

  puts("farol_resolver: producao ok");
  return 0;
}
#endif
