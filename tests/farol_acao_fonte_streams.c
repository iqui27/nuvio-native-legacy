// Integracao real: streams.c + farolacao_fonte.c + fontefalha.c.
// Reproduz P1-2 (PROXIMA_FONTE e FONTE_MENOR saindo da fonte 0 para 1 -> nova
// FonteFalhaSessao semeada so com 1 -> failover NUNCA volta para 0), P2-3
// (stream_filtro_bloqueou com filtro temporario) e P2-4 (stream_filtro_atualizar).
#define _POSIX_C_SOURCE 200809L
#define SDL_MAIN_HANDLED
#include "streams.h"
#include "farolacao_fonte.h"
#include "farol.h"
#include "farolanel.h"
#include "faroltexto.h"
#include "fontefalha.h"
#include "ilhaacao.h"
#include "player.h"
#include "debrid.h"
#include "addons.h"
#include "fonteregra.h"
#include "selospacote.h"
#include "audioext.h"
#include "streamfit.h"
#include "badges.h"
#include "fontepref.h"
#include "perfisessao.h"
#include "addonmodo.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- fronteiras do app/rede/ui ---------------------------------------------
static int trocasFarol = 0;
static int ilhas = 0;
static int manualCfg = 0;
static int resolverOk = 1;
static unsigned simTicks = 1000;

uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t g) { return g == 1; }
int perfis_sessao_travar(uint64_t g) { return g == 1; }
void perfis_sessao_destravar(void) {}
Uint32 SDL_GetTicks(void) { return simTicks; }

void farol_ev(FarolMod mod, int ev, int sev, int32_t a, int32_t b, const char *tok) {
  (void)mod; (void)ev; (void)sev; (void)a; (void)b; (void)tok;
}
void farol_registrar_acao(int acao) { (void)acao; }
void farol_registrar_resultado(int acao, int ok) { (void)acao; (void)ok; trocasFarol += ok ? 1 : 0; }
void ilhaacao_feita(const IlhaAcao *a) { (void)a; ilhas++; }
const char *i18n(const char *s) { return s; }

unsigned player_recarregar_contexto(void) { return 0; }
int player_recarregar_fonte(unsigned c) { (void)c; return 0; }
float gfx_escala_ui(void) { return 1.0f; }
int partes_disponivel(void) { return 0; }

int debrid_ativo(void) { return 1; }
int p2p_ativo(void) { return 0; }
const char *debrid_servico_nome(int s) { return s == 0 ? "Real-Debrid" : ""; }
int debrid_servico_por_nome(const char *n) { (void)n; return 0; }
int debrid_servico_disponivel(int s) { return s == 0; }
int debrid_resolver_escolhido(const char *h, int a, char *u, unsigned t, char *sv, unsigned ns, int *p) {
  (void)h; (void)a; (void)u; (void)t; (void)sv; (void)ns; (void)p; return 0;
}
int debrid_resolver_servico_cache(int s, const char *h, int a, char *u, unsigned t, int *fora) {
  (void)fora;
  if (!resolverOk) return 0;
  snprintf(u, t, "https://debrid.invalid/%d/%s/%d.mp4", s, h, a);
  return 1;
}
int debrid_resolver_cache(const char *h, int a, char *u, unsigned t, int *fora) {
  return debrid_resolver_servico_cache(0, h, a, u, t, fora);
}
void debrid_contexto_capturar(DebridContexto *c) { memset(c, 0, sizeof *c); }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
int p2p_resolver(const char *h, int a, const char *f, char *u, unsigned t) {
  (void)h; (void)a; (void)f; (void)u; (void)t; return 0;
}
int p2p_ultimo_erro(void) { return 0; }
int p2pmotor_e_url(const char *u) { (void)u; return 0; }

AddEstado addons_estado(void) { return ADD_PRONTO; }
int addons_n(void) { return 0; }
int addons_parcial_abas(AddonAba *out, int max) { (void)out; (void)max; return 0; }

int rede_url_final_tipo(const char *u, int s, const char *const *c,
                        char *fim, unsigned t, int *st, char *m, unsigned mt, long *cp) {
  (void)s; (void)c;
  if (!resolverOk) return 0;
  snprintf(fim, t, "%s", u);
  snprintf(m, mt, "video/mp4");
  *st = 200; *cp = 4096;
  return 1;
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; (void)st; return NULL;
}
int naovideo_mime(const char *m, const char *u, long b) { (void)m; (void)u; (void)b; return 0; }
int naovideo_nome(const char *a, const char *b, int h, long t) { (void)a; (void)b; (void)h; (void)t; return 0; }
long fontesmortas_idade(const Stream *s) { (void)s; return -1; }

int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_regex_modo(void) { return 0; }
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_fonte_prioridade(void) { return 0; }
int ajustes_fonte_hdr(void) { return 1; }
int ajustes_fonte_manual(void) { return manualCfg; }
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
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
int ajustes_selos_coloridos(void) { return 0; }
int ajustes_idioma(void) { return 0; }
const char *ajustes_qualidade(void) { return "Automatica"; }
const char *ajustes_p2p_url(void) { return ""; }

int fontepref_escolher(const char *id) { (void)id; return -1; }
int fontepref_escolher_manual(const char *id) { (void)id; return -1; }
int fontepref_escolher_lista_next(const char *id, const Stream *s, int n,
                                  int (*pode)(int, int, void *), void *ctx) {
  (void)id; (void)s; (void)n; (void)pode; (void)ctx; return -1;
}
unsigned fontepref_revisao(void) { return 0; }

unsigned fonteregra_versao(void) { return 1; }
int fonteregra_grupo(const FonteRegraCfg *c, const char *n, int p, const char *t) {
  (void)c; (void)n; (void)p; (void)t; return 0;
}
int fonteregra_no_escopo(const FonteRegraCfg *c, int p) { (void)c; (void)p; return 1; }
int fonteregra_ordem_rank(const char *p) { (void)p; return 0; }

uint64_t badges_bit(const char *s) { (void)s; return 0; }
uint64_t badges_detectar(const char *s) { (void)s; return 0; }
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
void audioext_alvo(const char *a) { (void)a; }
int audioext_serie_procura(const char *a, AudioExtIdent *id) { (void)a; (void)id; return 0; }
void audioext_serie_achou(const char *u, const char *c, const char *n) { (void)u; (void)c; (void)n; }
AddonModo addonmodo_get(uint64_t id) { (void)id; return AM_AUTO; }
int addonmodo_permite(uint64_t id, int d, int db, int p2p) {
  (void)id; (void)d; (void)db; (void)p2p; return 1;
}

// ---- helpers ---------------------------------------------------------------
static Stream mkDireta(const char *rot, const char *url, int alt, long mb, int dv) {
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
  return s;
}

static Stream mkHash(const char *rot, const char *hash, int alt, long mb) {
  Stream s = {0};
  snprintf(s.provedor, sizeof s.provedor, "Addon A");
  fontefalha_addon_chave("https://addon-a.invalid/manifest.json", s.addonChave, sizeof s.addonChave);
  snprintf(s.rotulo, sizeof s.rotulo, "%s", rot);
  snprintf(s.descricao, sizeof s.descricao, "%s", rot);
  snprintf(s.infoHash, sizeof s.infoHash, "%s", hash);
  s.altura = alt;
  s.tamanhoMB = mb;
  s.tamanhoBytes = (uint64_t)mb * 1000000ULL;
  return s;
}

static FonteFalhaConfig cfgPadrao(int provedores) {
  FonteFalhaConfig c = {0};
  c.provedores = provedores;
  c.addons = 1;
  for (int p = 0; p < 4; p++) { c.ordem[p] = p; c.limites[p] = 4; }
  c.nAddons = 1;
  fontefalha_addon_chave("https://addon-a.invalid/manifest.json", c.addon[0].chave, sizeof c.addon[0].chave);
  c.addon[0].limite = 10;
  return c;
}

static void reproduzir_p1_2_acao(int acao) {
  farolacao_fonte_encerrar();
  manualCfg = 1;
  resolverOk = 1;
  FonteFalhaConfig cfg = cfgPadrao(0);
  Stream l[3] = {
    mkDireta("2160p BluRay Remux", "https://cdn.invalid/f0.mkv", 2160, 4000, 0),
    mkDireta("1080p WEB-DL",       "https://cdn.invalid/f1.mkv", 1080, 1500, 0),
    mkDireta("1080p BluRay",       "https://cdn.invalid/f2.mkv", 1080, 2500, 0),
  };
  stream_definir_lista(l, 3);
  unsigned ger = stream_lista_geracao();
  stream_definir_atual(0);

  // Reproducao inicial na fonte 0.
  FonteFalhaSessao *s0 = fontefalha_sessao_nova(&cfg, 0, -1);
  assert(s0);
  assert(stream_falha_semear_manual(s0, 0, ger, l[0].url) == 1);

  // Farol troca da fonte 0 para 1.
  assert(farolacao_fonte_executar(acao, 0) == 1);
  int escolhida = -1;
  assert(stream_farol_troca_escolheu(&escolhida) == 1 && escolhida == 1);

  // tocarFonteEscolhida(1, 2) limpa a sessao antiga, atualiza atual=1 e semeia
  // a nova FonteFalhaSessao SOMENTE com a fonte 1.
  fontefalha_sessao_liberar(s0);
  stream_definir_atual(1);
  FonteFalhaSessao *s1 = fontefalha_sessao_nova(&cfg, 0, -1);
  assert(s1);
  assert(stream_falha_semear_manual(s1, 1, ger, l[1].url) == 1);

  // Fonte 1 falha: o failover NUNCA pode devolver a fonte 0 abandonada.
  assert(stream_falha_tem_proxima(s1, ger) == 1);
  int idx = -1, serv = -1;
  char url[NV_MEDIA_URL_MAX] = "";
  int res = stream_falha_verificar(s1, ger, 1, NULL, NULL, &idx, &serv, url, sizeof url);
  assert(res == FONTEFALHA_PROXIMA);
  assert(idx != 0);
  assert(idx == 2);

  // Esgotada a 2, deve encerrar sem voltar para 0.
  assert(stream_falha_tem_proxima(s1, ger) == 0);
  res = stream_falha_verificar(s1, ger, 1, NULL, NULL, &idx, &serv, url, sizeof url);
  assert(res == FONTEFALHA_FIM);
  fontefalha_sessao_liberar(s1);

  // Tambem com apenas 2 fontes (0 e 1): sem 2, nao ha proxima nem volta para 0.
  farolacao_fonte_encerrar();
  stream_definir_lista(l, 2);
  ger = stream_lista_geracao();
  stream_definir_atual(0);
  assert(farolacao_fonte_executar(acao, 0) == 1);
  assert(stream_farol_troca_escolheu(&escolhida) == 1 && escolhida == 1);
  stream_definir_atual(1);
  FonteFalhaSessao *s2 = fontefalha_sessao_nova(&cfg, 0, -1);
  assert(s2);
  assert(stream_falha_semear_manual(s2, 1, ger, l[1].url) == 1);
  assert(stream_falha_tem_proxima(s2, ger) == 0);
  res = stream_falha_verificar(s2, ger, 1, NULL, NULL, &idx, &serv, url, sizeof url);
  assert(res == FONTEFALHA_FIM);
  fontefalha_sessao_liberar(s2);
}

static void reproduzir_p1_2_torrent_escolhida_e_failover(void) {
  farolacao_fonte_encerrar();
  manualCfg = 1;
  resolverOk = 1;
  FonteFalhaConfig cfg = cfgPadrao(1);
  Stream l[3] = {
    mkHash("2160p BluRay Remux", "hash000000000000000000000000000000000000", 2160, 4000),
    mkHash("1080p WEB-DL",       "hash111111111111111111111111111111111111", 1080, 1500),
    mkHash("1080p BluRay",       "hash222222222222222222222222222222222222", 1080, 2500),
  };
  stream_definir_lista(l, 3);
  unsigned ger = stream_lista_geracao();
  stream_definir_atual(0);

  assert(farolacao_fonte_executar(FAROL_A_PROXIMA_FONTE, 0) == 1);
  int escolhida = -1;
  assert(stream_farol_troca_escolheu(&escolhida) == 1 && escolhida == 1);
  stream_definir_atual(1);

  // Primeira abertura da escolhida (tentativas == 0): continua podendo abrir 1.
  FonteFalhaSessao *primeira = fontefalha_sessao_nova(&cfg, 1, -1);
  assert(primeira);
  int idx = -1, serv = -1;
  char url[NV_MEDIA_URL_MAX] = "";
  assert(stream_falha_verificar(primeira, ger, 1, NULL, NULL, &idx, &serv, url, sizeof url) == FONTEFALHA_PROXIMA);
  assert(idx == 1);
  assert(stream_falha_aplicar(1, ger, serv, url));
  fontefalha_sessao_liberar(primeira);

  // Apos semear 1 e ela falhar, failover vai para 2, NUNCA 0.
  FonteFalhaSessao *semeada = fontefalha_sessao_nova(&cfg, 1, -1);
  assert(semeada);
  assert(stream_falha_semear_manual(semeada, 1, ger, url) == 1);
  assert(stream_falha_verificar(semeada, ger, 1, NULL, NULL, &idx, &serv, url, sizeof url) == FONTEFALHA_PROXIMA);
  assert(idx == 2);
  assert(stream_falha_verificar(semeada, ger, 1, NULL, NULL, &idx, &serv, url, sizeof url) == FONTEFALHA_FIM);
  fontefalha_sessao_liberar(semeada);
}

static void verificar_p2_3_e_p2_4(void) {
  farolacao_fonte_encerrar();
  FonteFiltroConfig vazio = {0};
  vazio.cache = FONTEFILTRO_CACHE_TODAS;
  assert(fontefalha_filtro_definir(&vazio));
  assert(!fontefalha_filtro_ativo());

  // P2-3: filtro temporario sozinho tambem aciona stream_filtro_bloqueou().
  Stream soDv[2] = {
    mkDireta("2160p Dolby Vision", "https://cdn.invalid/dv0.mkv", 2160, 4000, 1),
    mkDireta("1080p Dolby Vision", "https://cdn.invalid/dv1.mkv", 1080, 2000, 1),
  };
  stream_definir_lista(soDv, 2);
  assert(stream_filtro_bloqueou() == 0);
  FonteFiltroConfig semDv = {0};
  semDv.cache = FONTEFILTRO_CACHE_TODAS;
  semDv.excluir[FONTEFILTRO_VISUAL] = (UINT64_C(1) << 0) | (UINT64_C(1) << 1) | (UINT64_C(1) << 5);
  assert(farolacao_fonte_filtro_temporario_definir(&semDv));
  assert(stream_filtro_bloqueou() == 1);
  farolacao_fonte_encerrar();
  assert(stream_filtro_bloqueou() == 0);

  // P2-4: esconder DV nesta TV e Desfazer chamam stream_filtro_atualizar().
  Stream mista[2] = {
    mkDireta("2160p Dolby Vision", "https://cdn.invalid/dv.mkv", 2160, 4000, 1),
    mkDireta("1080p SDR",          "https://cdn.invalid/sdr.mkv", 1080, 2000, 0),
  };
  stream_definir_alvo("tt0015");
  stream_definir_lista(mista, 2);
  stream_definir_atual(0);
  assert(stream_item(0)->filtro.naoVod == 0);
  assert(fontefalha_addon_definir_ao_vivo(mista[0].addonChave, 1));
  assert(stream_item(0)->filtro.naoVod == 0);
  assert(farolacao_fonte_executar(FAROL_A_FONTE_SEM_DV, 1) == 1);
  assert(stream_item(0)->filtro.naoVod == 1);
  assert(fontefalha_addon_definir_ao_vivo(mista[0].addonChave, 0));
  assert(stream_item(0)->filtro.naoVod == 1);
  assert(farolacao_fonte_desfazer_dv() == 1);
  assert(stream_item(0)->filtro.naoVod == 0);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  fontefalha_dir(argv[1]);
  reproduzir_p1_2_acao(FAROL_A_PROXIMA_FONTE);
  reproduzir_p1_2_acao(FAROL_A_FONTE_MENOR);
  reproduzir_p1_2_torrent_escolhida_e_failover();
  verificar_p2_3_e_p2_4();
  stream_definir_lista(NULL, 0);
  puts("farol_acao_fonte_streams: PASS (P1-2 PROXIMA_FONTE/FONTE_MENOR + P2-3 + P2-4)");
  return 0;
}
