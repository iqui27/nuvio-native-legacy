// Production source worker + policy, with only network/settings boundaries mocked.
#define _POSIX_C_SOURCE 200809L
#define SDL_MAIN_HANDLED
#include "../src/streams.c"
#include "stub_player_source_actions.h"
#include <assert.h>

typedef struct { int servico, arquivo; char hash[48]; } Pedido;
static Pedido pedidos[128];
static int nPedidos, nHttp, cancelar, trocar, errar, duplicar, recusar, bloquearManual;
static int falharServico = -1;
static int prioridadeTeste = 1, primeiraTeste;
static const char *qualidadeTeste = "Automatica";
static int nManual, manualResultado = 1, nP2P, p2pHabilitado, motor;
static AddEstado estadoAddons = ADD_PRONTO;
static unsigned disponiveis = 15;
static Stream substituta;
static char ultimoCab[512];

uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t geracao) { return geracao == 1; }
int perfis_sessao_travar(uint64_t geracao) { return geracao == 1; }
void perfis_sessao_destravar(void) {}
Uint32 SDL_GetTicks(void) { return 1000; }
int debrid_ativo(void) { return 1; }
int p2p_ativo(void) { return p2pHabilitado; }
const char *debrid_servico_nome(int s) {
  static const char *const nomes[] = {"Real-Debrid", "TorBox", "Premiumize", "AllDebrid"};
  return s >= 0 && s < 4 ? nomes[s] : "";
}
int debrid_servico_por_nome(const char *nome) {
  for (int s = 0; s < 4; s++) if (nome && !strcasecmp(nome, debrid_servico_nome(s))) return s;
  return -1;
}
int debrid_resolver_escolhido(const char *hash, int arq, char *url, unsigned tam,
                              char *servico, unsigned ns, int *pct) {
  nManual++;
  assert(!strcmp(hash, "MANUAL-HASH") && arq == 6);
  if (servico && ns) snprintf(servico, ns, "%s", debrid_servico_nome(DEBRID_TB));
  if (pct) *pct = 100;
  if (!manualResultado) { url[0] = 0; return 0; }
  snprintf(url, tam, "https://manual.native.invalid/%s/%d.mp4", hash, arq);
  return manualResultado;
}
int p2p_resolver(const char *hash, int arq, const char *fontes, char *url, unsigned tam) {
  (void)fontes; nP2P++;
  assert(!strcmp(hash, "MANUAL-HASH") && arq == 6);
  snprintf(url, tam, "%s", motor ? "http://127.0.0.1/embedded/file.mp4" : "https://p2p.invalid/torrent/file.mp4");
  return P2P_OK;
}
int p2p_ultimo_erro(void) { return 0; }
int p2pmotor_e_url(const char *url) { return url && strstr(url, "/embedded/") != NULL; }
int debrid_servico_disponivel(int s) { return !!(disponiveis & (1u << s)); }
AddEstado addons_estado(void) { return estadoAddons; }
int debrid_resolver_servico_cache(int s, const char *hash, int arq,
                                 char *url, unsigned tam, int *fora) {
  assert(nPedidos < (int)(sizeof pedidos / sizeof pedidos[0]));
  Pedido *p = &pedidos[nPedidos++];
  p->servico = s; p->arquivo = arq;
  snprintf(p->hash, sizeof p->hash, "%s", hash);
  if (trocar) stream_definir_lista(&substituta, 1);
  if (cancelar == 1) cancelar = 2;
  if (recusar == s + 1) disponiveis &= ~(1u << s);
  if (errar || s == falharServico || (recusar == s + 1)) { if (fora) *fora = 1; return 0; }
  if (duplicar && s == 0 && strcmp(hash, "hash-a") == 0)
    snprintf(url, tam, "https://native.invalid/shared.mp4");
  else snprintf(url, tam, "https://native.invalid/%d/%s/%d.mp4", s, hash, arq);
  return 1;
}
int debrid_resolver_cache(const char *hash, int arq, char *url, unsigned tam, int *fora) {
  return debrid_resolver_servico_cache(0, hash, arq, url, tam, fora);
}
int rede_url_final_tipo(const char *url, int seg, const char *const *cab,
                        char *fim, unsigned tam, int *status,
                        char *mime, unsigned mimeTam, long *corpo) {
  (void)seg; nHttp++;
  ultimoCab[0] = 0;
  if (cab && cab[0]) snprintf(ultimoCab, sizeof ultimoCab, "%s", cab[0]);
  snprintf(fim, tam, "%s", url);
  snprintf(mime, mimeTam, "video/mp4"); *status = 200; *corpo = 1024;
  return !errar;
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; (void)st; assert(!"no HLS in fixture"); return NULL;
}
int naovideo_mime(const char *m, const char *u, long b) { (void)m; (void)u; (void)b; return 0; }
int naovideo_nome(const char *a, const char *b, int h, long t) {
  (void)a; (void)b; (void)h; (void)t; return 0;
}
long fontesmortas_idade(const Stream *s) {
  return bloquearManual && strstr(s->url, "/manual.mp4") ? 0 : -1;
}
int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_regex_modo(void) { return 0; }
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_fonte_prioridade(void) { return prioridadeTeste; }
int ajustes_fonte_hdr(void) { return 1; }
int ajustes_fonte_manual(void) { return 0; }
int ajustes_fonte_idioma_audio(void) { return 0; }
int ajustes_fonte_preparar(void) { return 1; }
int ajustes_fonte_precarregar(void) { return 1; }
int ajustes_buffer_preset(void) { return 0; }
int ajustes_cache_seek_mb(void) { return 0; }
int ajustes_fonte_conferir_varias(void) { return 0; }
int ajustes_fonte_tocar_conferindo(void) { return 0; }
void debrid_contexto_capturar(DebridContexto *c) { memset(c, 0, sizeof *c); }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
static int memoriaNextConsultas;
int fontepref_escolher_lista_next(const char *id, const Stream *s, int n,
                                  int (*pode)(int,int,void *), void *ctx) {
  (void)id;(void)s;(void)n;(void)pode;(void)ctx;memoriaNextConsultas++;return -1;
}
int ajustes_fonte_primeira(void) { return prioridadeTeste == 3 || primeiraTeste; }
int ajustes_fonte_ordem_uso(void) { return 0; }
int ajustes_dolby_vision(void) { return 0; }
int ajustes_dolby_atmos(void) { return 0; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
int ajustes_selos_coloridos(void) { return 0; }
const char *ajustes_qualidade(void) { return qualidadeTeste; }
unsigned fonteregra_versao(void) { return 1; }
int fonteregra_grupo(const FonteRegraCfg *c, const char *nome, int plugin, const char *texto) {
  (void)c; (void)plugin; (void)texto;
  return bloquearManual && strstr(nome, "Blocked") ? -1 : 0;
}
int fonteregra_no_escopo(const FonteRegraCfg *c, int p) { (void)c; (void)p; return 1; }
int fonteregra_ordem_rank(const char *p) { (void)p; return 0; }
uint64_t badges_bit(const char *s) { return !strcmp(s, "co-x265") ? UINT64_C(1) : 0; }
uint64_t badges_detectar(const char *s) {
  return s && (strstr(s, "HEVC") || strstr(s, "hevc")) ? UINT64_C(1) : 0;
}
int badges_fonte_hdr_marca(uint64_t b) { (void)b; return -1; }
uint64_t streamfit_agora_ms(void) { return 0; }
void streamfit_foto(StreamfitFoto *s, uint64_t ms) { (void)ms; memset(s, 0, sizeof *s); }
StreamfitClasse streamfit_classificar(const StreamfitFoto *f, const char *u,
                                      uint64_t b, double d, StreamfitResultado *s) {
  (void)f; (void)u; (void)b; (void)d; memset(s, 0, sizeof *s); return 0;
}
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int n, unsigned short *o, int m) {
  (void)p; (void)n; (void)o; (void)m; return 0;
}
void selospacote_colorido(int c) { (void)c; }
int selospacote_embutidos_ok(void) { return 0; }
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }

static int foiCancelado(void *ctx) { (void)ctx; return cancelar == 2; }
static FonteFalhaConfig cfg(int native, int addon, int limite) {
  FonteFalhaConfig c = {0}; c.provedores = native; c.addons = addon;
  for (int p = 0; p < 4; p++) { c.ordem[p] = p; c.limites[p] = limite; }
  c.nAddons = 2;
  fontefalha_addon_chave("https://addon-a.invalid/manifest.json", c.addon[0].chave, sizeof c.addon[0].chave);
  fontefalha_addon_chave("https://addon-b.invalid/manifest.json", c.addon[1].chave, sizeof c.addon[1].chave);
  c.addon[0].limite = c.addon[1].limite = 10;
  return c;
}
static Stream fonte(const FonteFalhaConfig *c, int addon, const char *hash, int arq) {
  Stream s = {0};
  snprintf(s.provedor, sizeof s.provedor, "Addon %c", 'A' + addon);
  snprintf(s.addonChave, sizeof s.addonChave, "%s", c->addon[addon].chave);
  snprintf(s.rotulo, sizeof s.rotulo, "1080p");
  snprintf(s.infoHash, sizeof s.infoHash, "%s", hash);
  s.fileIdx = arq; s.addonOrdem = addon; s.altura = 1080;
  return s;
}
static void resetar(void) {
  prioridadeTeste = 1; primeiraTeste = 0; qualidadeTeste = "Automatica";
  nPedidos = nHttp = cancelar = trocar = errar = duplicar = recusar = bloquearManual = 0;
  disponiveis = 15; estadoAddons = ADD_PRONTO;
  falharServico = -1;
  nManual = nP2P = p2pHabilitado = motor = 0; manualResultado = 1;
  memset(&substituta, 0, sizeof substituta);
  strcpy(substituta.url, "https://replacement.invalid/next.mp4");
}
static int verificar(FonteFalhaSessao *s, unsigned ger, int *idx, int *serv, char *url) {
  return stream_falha_verificar(s, ger, -1, foiCancelado, NULL, idx, serv, url, 4096);
}
static void ordemETotal(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 1, 2);
  // Arrival order is B, A, A. Provider-first must still try A then B before TB.
  c.addon[0].limite = 1;
  Stream l[] = {fonte(&c, 1, "hash-b", 0), fonte(&c, 0, "hash-a", 0), fonte(&c, 0, "hash-c", 0)};
  stream_definir_lista(l, 3);
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 3, -1); assert(s);
  unsigned ger = stream_lista_geracao();
  int idx, serv; char url[4096];
  const int esperado[] = {1, 0, 1, 0};
  for (int i = 0; i < 4; i++) {
    assert(stream_falha_tem_proxima(s, ger));
    assert(stream_falha_tem_proxima(s, ger));
    assert(fontefalha_tentativas(s) == i && nPedidos == i);
    assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA);
    assert(idx == esperado[i] && serv == i / 2);
    fontefalha_marcar_aberta(s);
  }
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM);
  assert(!stream_falha_tem_proxima(s, ger));
  assert(!stream_falha_tem_proxima(s, ger + 1));
  assert(nPedidos == 4 && fontefalha_tentativas(s) == 4);
  assert(pedidos[0].servico == 0 && pedidos[1].servico == 0 && pedidos[2].servico == 1);
  assert(!strcmp(pedidos[0].hash, "hash-a") && !strcmp(pedidos[1].hash, "hash-b"));
  fontefalha_sessao_liberar(s);
}
static void identidadeEDedup(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 0, 2);
  Stream l[] = {fonte(&c, 0, "hash-a", 0), fonte(&c, 1, "hash-a", 0),
                fonte(&c, 0, "hash-a", 1), fonte(&c, 0, "hash-b", 0)};
  stream_definir_lista(l, 4); duplicar = 1;
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 1, -1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && idx == 0);
  assert(stream_falha_aplicar(idx, ger, serv, url));
  assert(!strcmp(stream_item(0)->infoHash, "hash-a") && stream_item(0)->fileIdx == 0);
  assert(stream_item(0)->debridServico == 1 && !strcmp(stream_item(0)->addonChave, c.addon[0].chave));
  fontefalha_marcar_aberta(s);
  // Same hash/file from another addon is skipped before resolution. Different
  // file resolves once; duplicate final URL refunds its slot, allowing hash-b.
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && idx == 3 && serv == 0);
  assert(nPedidos == 3 && pedidos[1].arquivo == 1 && fontefalha_tentativas(s) == 2);
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM && nPedidos == 3);
  fontefalha_sessao_liberar(s);
}
static void missNaoExcluiOutroServico(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 0, 1);
  Stream l = fonte(&c, 0, "hash-a", 3); stream_definir_lista(&l, 1);
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 3, -1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  falharServico = 0;
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && serv == 1);
  assert(nPedidos == 2 && pedidos[0].servico == 0 && pedidos[1].servico == 1);
  assert(debrid_servico_disponivel(0) && "uncached RD does not revoke credentials or other native routes");
  assert(!stream_item(0)->foraCache && !stream_item(0)->url[0]);
  assert(stream_falha_aplicar(idx, ger, serv, url));
  assert(stream_item(0)->debridServico == 2 && stream_item(0)->fileIdx == 3);
  fontefalha_sessao_liberar(s);
}
static void cachePertenceAoServico(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 1, 1);
  c.ordem[0] = 1; c.ordem[1] = 0;
  Stream l[] = {fonte(&c, 0, "hash-a", 7), fonte(&c, 1, "", 0)};
  l[0].foraCache = l[1].foraCache = 1;
  strcpy(l[1].url, "https://addon.invalid/uncached.mp4");
  stream_definir_lista(l, 2); falharServico = 1;
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 3, -1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  // Addon "uncached" may only describe TB. Ask the selected native service
  // cache-only, retain the same hash for RD, and exclude uncached HTTP URLs.
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && idx == 0 && serv == 0);
  assert(nPedidos == 2 && pedidos[0].servico == 1 && pedidos[1].servico == 0);
  assert(pedidos[0].arquivo == 7 && !strcmp(pedidos[0].hash, pedidos[1].hash));
  assert(nHttp == 0 && stream_item(0)->foraCache && stream_item(1)->foraCache);
  fontefalha_marcar_aberta(s);
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM && nHttp == 0);
  fontefalha_sessao_liberar(s);
}
static void hashCanonicoEAplicacaoSobreposta(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 0, 2);
  Stream l[] = {fonte(&c, 0, "AABBCC", 4), fonte(&c, 1, "aabbcc", 4), fonte(&c, 0, "AABBCC", 5)};
  stream_definir_lista(l, 3);
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 1, -1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && idx == 0);
  assert(!strcmp(pedidos[0].hash, "AABBCC") && "resolver keeps original hash bytes");
  assert(stream_falha_aplicar(0, ger, serv, url));
  assert(stream_falha_aplicar(0, ger, serv, stream_item(0)->url));
  assert(!strcmp(stream_item(0)->url, url) && stream_item(0)->debridServico == serv + 1);
  assert(!strcmp(stream_item(0)->infoHash, "AABBCC") && stream_item(0)->fileIdx == 4);
  fontefalha_marcar_aberta(s);
  // Case changes cannot consume another distinct link, but another file can.
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && idx == 2);
  assert(nPedidos == 2 && pedidos[1].arquivo == 5);
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM && nPedidos == 2);
  fontefalha_sessao_liberar(s);
}
static void semTetosAntigos(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 0, 10);
  Stream l[12];
  for (int i = 0; i < 12; i++) {
    char hash[48]; snprintf(hash, sizeof hash, "distinct-hash-%d", i);
    l[i] = fonte(&c, i % 2, hash, 0);
  }
  stream_definir_lista(l, 12);
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 3, -1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  for (int i = 0; i < 20; i++) {
    assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && serv == i / 10);
    fontefalha_marcar_aberta(s);
  }
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM);
  assert(nPedidos == 20 && fontefalha_tentativas(s) == 20);
  fontefalha_sessao_liberar(s);
}
static void limiteNativoIndependente(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 0, 10);
  c.addon[0].limite = 3;
  Stream l[12];
  for (int i = 0; i < 12; i++) {
    char hash[48]; snprintf(hash, sizeof hash, "native-only-%d", i);
    l[i] = fonte(&c, 0, hash, 0);
  }
  stream_definir_lista(l, 12);
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 1, -1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  for (int i = 0; i < 10; i++) {
    assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && serv == 0);
    fontefalha_marcar_aberta(s);
  }
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM);
  assert(nPedidos == 10 && fontefalha_tentativas(s) == 10);
  fontefalha_sessao_liberar(s);
}
static void decoderAprendido(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 0, 1);
  Stream l[] = {fonte(&c, 0, "same-movie", 0), fonte(&c, 0, "same-movie", 1), fonte(&c, 1, "other-4k", 0)};
  for (int i = 0; i < 3; i++) {
    l[i].altura = i == 1 ? 1080 : 2160;
    snprintf(l[i].rotulo, sizeof l[i].rotulo, "%dp HEVC", l[i].altura);
  }
  stream_definir_lista(l, 3);
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 3, 0); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  assert(stream_falha_verificar(s, ger, 0, foiCancelado, NULL, &idx, &serv, url, sizeof url) == FONTEFALHA_PROXIMA);
  assert(idx == 0 && serv == DEBRID_RD && nPedidos == 1);
  fontefalha_marcar_aberta(s);
  stream_automatico_erro_decoder(idx, 4005, 0);
  // Initial explicit choice is allowed; after a real video decoder failure,
  // do not resolve the same unsupported 4K family on TB. Lower HEVC can play.
  assert(stream_falha_verificar(s, ger, 0, foiCancelado, NULL, &idx, &serv, url, sizeof url) == FONTEFALHA_PROXIMA);
  assert(idx == 1 && serv == DEBRID_TB && nPedidos == 2 && pedidos[1].arquivo == 1);
  assert(!strcmp(pedidos[1].hash, "same-movie"));
  assert(stream_falha_verificar(s, ger, 0, foiCancelado, NULL, &idx, &serv, url, sizeof url) == FONTEFALHA_FIM);
  assert(nPedidos == 2 && fontefalha_tentativas(s) == 2);
  fontefalha_sessao_liberar(s);
}
static void geracaoECancelamento(void) {
  for (int modo = 0; modo < 2; modo++) {
    resetar(); FonteFalhaConfig c = cfg(1, 1, 3);
    Stream l[] = {fonte(&c, 0, "hash-a", 0), fonte(&c, 1, "hash-b", 0)};
    stream_definir_lista(l, 2);
    FonteFalhaSessao *viva = fontefalha_sessao_nova(&c, 3, -1); assert(viva);
    FonteFalhaSessao *worker = fontefalha_sessao_clonar(viva); assert(worker);
    unsigned ger = stream_lista_geracao(); int idx = 99, serv = 99; char url[4096] = "old";
    if (modo == 0) trocar = 1; else cancelar = 1;
    assert(verificar(worker, ger, &idx, &serv, url) == FONTEFALHA_FIM);
    assert(nPedidos == 1 && idx == -1 && serv == -1 && !url[0]);
    assert(fontefalha_tentativas(viva) == 0 && "cancelled clone must not consume live playback budget");
    if (modo == 0) {
      assert(!stream_falha_aplicar(0, ger, 0, "https://stale.invalid/old.mp4"));
      assert(!strcmp(stream_item(0)->url, substituta.url) && stream_item(0)->debridServico == 0);
    } else assert(!stream_item(0)->url[0] && !stream_item(1)->url[0]);
    fontefalha_sessao_liberar(worker); fontefalha_sessao_liberar(viva);
  }
}
static void addonIndependente(void) {
  resetar(); FonteFalhaConfig c = cfg(0, 1, 3);
  c.addon[0].limite = 1; c.addon[1].limite = 2;
  Stream l[] = {fonte(&c, 1, "", 0), fonte(&c, 0, "", 0), fonte(&c, 0, "", 0), fonte(&c, 1, "", 0)};
  for (int i = 0; i < 4; i++) snprintf(l[i].url, sizeof l[i].url, "https://addon.invalid/%d.mp4", i);
  strcpy(l[1].cabecalhos, "Referer: https://addon-a.invalid/");
  stream_definir_lista(l, 4);
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 15, -1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  const int esperado[] = {1, 0, 3};
  for (int i = 0; i < 3; i++) {
    assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && idx == esperado[i] && serv == -1);
    if (i == 0) assert(!strcmp(ultimoCab, l[1].cabecalhos));
    fontefalha_marcar_aberta(s);
  }
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM);
  assert(nHttp == 3 && nPedidos == 0 && fontefalha_tentativas(s) == 3);
  fontefalha_sessao_liberar(s);
}
static void buscaParcial(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 0, 2);
  Stream l = fonte(&c, 0, "hash-a", 0); stream_definir_lista(&l, 1);
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 3, -1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  estadoAddons = ADD_BUSCANDO; errar = 1;
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_ESPERAR && nPedidos == 1);
  // Final completion permits provider advance, retaining the already tried RD pair.
  estadoAddons = ADD_PRONTO; errar = 0;
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA && serv == 1 && nPedidos == 2);
  fontefalha_sessao_liberar(s);
}
static void escolhaManual(void) {
  resetar(); FonteFalhaConfig c = cfg(1, 1, 1);
  c.addon[0].limite = c.addon[1].limite = 1;
  Stream l[] = {fonte(&c, 1, "hash-b", 2), fonte(&c, 0, "", 0), fonte(&c, 0, "", 0)};
  strcpy(l[1].url, "https://addon.invalid/manual.mp4");
  strcpy(l[2].url, "https://addon.invalid/excluded.mp4");
  strcpy(l[1].provedor, "Blocked manual"); strcpy(l[2].provedor, "Blocked other");
  strcpy(l[1].cabecalhos, "Referer: https://chosen.invalid/");
  stream_definir_lista(l, 3); bloquearManual = 1;
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 3, 1); assert(s);
  int idx, serv; char url[4096]; unsigned ger = stream_lista_geracao();
  // Explicit selection bypasses automatic filters only for this row, and is
  // the first link even though native provider order normally precedes URLs.
  assert(stream_falha_verificar(s, ger, 1, foiCancelado, NULL, &idx, &serv, url, sizeof url) == FONTEFALHA_PROXIMA);
  assert(idx == 1 && serv == -1 && nHttp == 1 && nPedidos == 0);
  assert(!strcmp(ultimoCab, l[1].cabecalhos) && fontefalha_tentativas(s) == 1);
  fontefalha_marcar_aberta(s);
  for (int p = 0; p < 2; p++) {
    assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_PROXIMA);
    assert(idx == 0 && serv == p);
  }
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM);
  assert(nHttp == 1 && nPedidos == 2 && fontefalha_tentativas(s) == 3);
  fontefalha_sessao_liberar(s);
}
static void resolvedorManual(const char *dir) {
  resetar(); fontefalha_dir(dir);
  assert(fontefalha_definir_provedores(1));
  FonteFalhaConfig c = cfg(1, 1, 3);
  Stream l = fonte(&c, 0, "MANUAL-HASH", 6);
  strcpy(l.url, "https://cached.native.invalid/old.mp4");
  l.debridServico = DEBRID_TB + 1;
  stream_definir_lista(&l, 1);
  unsigned ger = stream_lista_geracao(); int pct = -1;
  char url[4096], nome[32];
  // Known cached winner is canonical and requires no resolver/network call.
  assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1);
  assert(!strcmp(url, l.url) && !strcmp(nome, "TorBox") && !nManual && !nPedidos && !nHttp);
  assert(stream_item(0)->debridServico == DEBRID_TB + 1 && !strcmp(stream_item(0)->infoHash, l.infoHash));
  // An old cached native URL has no trustworthy winning service. With the
  // feature enabled, resolve explicitly once and persist the exact winner.
  l.debridServico = 0; stream_definir_lista(&l, 1); ger = stream_lista_geracao();
  assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1);
  assert(nManual == 1 && !strcmp(nome, "TorBox") && strcmp(url, l.url));
  assert(stream_item(0)->debridServico == DEBRID_TB + 1 && !strcmp(stream_item(0)->url, url));
  assert(!strcmp(stream_item(0)->infoHash, "MANUAL-HASH") && stream_item(0)->fileIdx == 6);
  assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1 && nManual == 1);
  // A downloading response overrides both addon and previous native cache hits.
  for (int native = 0; native < 2; native++) {
    Stream downloading = l;
    downloading.emCache = 1; downloading.cacheNativo = native;
    stream_definir_lista(&downloading, 1); ger = stream_lista_geracao();
    assert(cacheConfirmado(stream_item(0)));
    manualResultado = DEBRID_BAIXANDO;
    assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == DEBRID_BAIXANDO);
    assert(!cacheConfirmado(stream_item(0)));
    assert(!stream_item(0)->emCache && !stream_item(0)->cacheNativo);
    assert(stream_item(0)->cacheNegado && stream_item(0)->foraCache);
    // A later ready result restores the author's native confirmation behavior.
    manualResultado = 1;
    assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1);
    assert(cacheConfirmado(stream_item(0)));
  }
  nManual = 1;
  // Integration #499 removes the addon lightning when the native icon is shown.
  strcpy(l.rotulo, "⚡️  Addon 1080p"); l.emCache = 1;
  tituloAddon(&l, url, sizeof url);
  assert(!strcmp(url, "Addon 1080p"));
  // Feature off preserves the old cached-URL fast path.
  assert(fontefalha_definir_provedores(0));
  stream_definir_lista(&l, 1); ger = stream_lista_geracao();
  assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1);
  assert(nManual == 1 && !strcmp(url, l.url) && !nome[0]);
  // P2P fallback owns its URL and must clear any prior native service name.
  assert(fontefalha_definir_provedores(1));
  l.url[0] = 0; l.debridServico = DEBRID_TB + 1;
  stream_definir_lista(&l, 1); ger = stream_lista_geracao();
  manualResultado = 0; p2pHabilitado = 1; nManual = 0;
  assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1);
  assert(nManual == 1 && nP2P == 1 && !nome[0]);
  assert(stream_item(0)->urlP2P == 1 && stream_item(0)->debridServico == 0 && !strcmp(stream_item(0)->url, url));
  FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 3, -1); assert(s);
  int idx, serv;
  assert(verificar(s, ger, &idx, &serv, url) == FONTEFALHA_FIM && !nPedidos && !nHttp);
  fontefalha_sessao_liberar(s);
  assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1 && nManual == 1 && nP2P == 1);
  // Embedded motor URLs die with the motor and are never stored in the list.
  stream_definir_lista(&l, 1); ger = stream_lista_geracao(); motor = 1;
  assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1);
  assert(p2pmotor_e_url(url) && !stream_item(0)->url[0]);
  assert(stream_item(0)->urlP2P && !stream_item(0)->debridServico && !nome[0]);
  assert(stream_resolver_escolhida(0, ger, url, sizeof url, nome, sizeof nome, &pct) == 1);
  assert(nManual == 3 && nP2P == 3 && !stream_item(0)->url[0]);
  assert(fontefalha_definir_provedores(0));
}
static void canalIgnoraFiltros(const char *dir) {
  resetar(); fontefalha_dir(dir);
  FonteFiltroConfig previous, blocked = {0}; fontefalha_filtro(&previous);
  for (int c = 0; c < FONTEFILTRO_N; c++)
    blocked.excluir[c] = (UINT64_C(1) << fontefiltro_opcoes_n(c)) - 1;
  assert(fontefalha_filtro_definir(&blocked));
  Stream live = {0}; live.fileIdx = -1;
  snprintf(live.url, sizeof live.url, "%s", "https://live.invalid/channel.m3u8");
  stream_definir_lista(&live, 1);
  unsigned generation = stream_lista_geracao(); char url[4096], service[32]; int pct;
  stream_folha_canal(0);
  assert(stream_resolver_escolhida(0, generation, url, sizeof url, service, sizeof service, &pct) == 0);
  stream_folha_canal(1);
  stream_filtro_tipo("custom-live-resource");
  stream_definir_lista(&live, 1); generation = stream_lista_geracao();
  assert(stream_resolver_escolhida(0, generation, url, sizeof url, service, sizeof service, &pct) == 1);
  assert(!strcmp(url, live.url) && !nManual && !nPedidos && !nHttp);
  FonteFiltroDados retained;
  stream_filtro_dados(stream_item(0), &retained);
  assert(retained.naoVod);
  stream_folha_canal(0);
  stream_filtro_tipo("movie");
  assert(fontefiltro_permite(&blocked, &retained));
  stream_definir_lista(&live, 1); generation = stream_lista_geracao();
  assert(stream_resolver_escolhida(0, generation, url, sizeof url, service, sizeof service, &pct) == 0);
  assert(fontefalha_filtro_definir(&previous));
}
static void ordemSemPrioridade(void) {
  for (int priority = 0; priority < 4; priority++) {
    resetar(); primeiraTeste = 1; prioridadeTeste = priority; qualidadeTeste = "1080p";
    FonteFalhaConfig c = cfg(1, 0, 3);
    Stream list[2] = {fonte(&c, 0, "hash-first", 0), fonte(&c, 0, "hash-second", 1)};
    list[0].altura = 2160;
    strcpy(list[0].rotulo, "First from AIOStreams");
    strcpy(list[1].rotulo, "1080p REMUX higher local preference");
    stream_definir_lista(list, 2);
    FonteFalhaSessao *s = fontefalha_sessao_nova(&c, 15, -1); assert(s);
    int index = -1, service = -1; char url[4096];
    assert(verificar(s, stream_lista_geracao(), &index, &service, url) == FONTEFALHA_PROXIMA);
    assert(index == (priority == 3 ? 0 : 1));
    assert(nPedidos == 1);
    fontefalha_sessao_liberar(s);
    if (priority == 3) {
      memoriaNextConsultas = 0;
      StreamPreparo *prepared = stream_preparo_criar_alvo(list, 2, 1, "tt123:1:2");
      assert(prepared && prepared->nc == 2 && prepared->candidatas[0].indice == 0);
      assert(!memoriaNextConsultas); // No remembered source overrides the current order.
      Stream result;
      assert(stream_preparo_resolver(prepared, foiCancelado, NULL, &result, &index, NULL) == 1);
      assert(index == 0);
      prioridadeTeste = 0;
      assert(!stream_preparo_contexto_atual(prepared)); // Old preparation cannot survive a mode change.
      prioridadeTeste = 3;
      stream_preparo_liberar(prepared);
      // Explicit selection in Sources must still play the source the user tapped.
      s = fontefalha_sessao_nova(&c, 15, -1); assert(s);
      assert(stream_falha_verificar(s, stream_lista_geracao(), 1, foiCancelado, NULL,
                                   &index, &service, url, sizeof url) == FONTEFALHA_PROXIMA);
      assert(index == 1); fontefalha_sessao_liberar(s);
      // A profile's disallowed add-on cannot win just because it was first.
      bloquearManual = 1; strcpy(list[0].provedor, "Blocked addon");
      stream_definir_lista(list, 2);
      s = fontefalha_sessao_nova(&c, 15, -1); assert(s);
      assert(verificar(s, stream_lista_geracao(), &index, &service, url) == FONTEFALHA_PROXIMA);
      assert(index == 1); fontefalha_sessao_liberar(s);
    }
  }
  resetar();
  puts("add-on order worker: PASS (soft cap bypass, existing priorities, manual pick, add-on eligibility)");
}
int main(int argc, char **argv) {
  assert(argc == 2 && "runner supplies an isolated preference directory");
  ordemETotal(); identidadeEDedup(); missNaoExcluiOutroServico(); semTetosAntigos();
  limiteNativoIndependente(); decoderAprendido();
  cachePertenceAoServico(); hashCanonicoEAplicacaoSobreposta();
  geracaoECancelamento(); addonIndependente(); buscaParcial(); escolhaManual();
  resolvedorManual(argv[1]);
  canalIgnoraFiltros(argv[1]);
  ordemSemPrioridade();
  stream_definir_lista(NULL, 0);
  puts("fontefalha worker: PASS (provider order, total limits, dedup, identity, generation, cancellation, addon order, partial search, manual selection, cached winner, P2P isolation)");
  return 0;
}
