// #400: duas respostas fora de ordem; exercita a montagem real da folha.
#include <assert.h>
#include "../src/streams.c"
#include "stub_player_source_actions.h"
#include "addonmodo.h"
#include "fontenota.h"
#include "fontefalha.h"
#include "vazao.h"
#include "fontepref.h"
#include "addons.h"

AddonModo addonmodo_get(uint64_t id) { (void)id; return AM_AUTO; }
int addonmodo_permite(uint64_t id, int direto, int debrid, int p2p) {
  (void)id; (void)direto; (void)debrid; (void)p2p; return 1;
}
int vazao_resumir(const int *kbps, int n, VazaoResumo *r) { (void)kbps; (void)n; if (r) memset(r, 0, sizeof *r); return 0; }
void plrui_decimal(char *s) { if (s) *s = 0; }
unsigned addons_parcial_rev(void) { return 0; }
int addons_parcial_abas(AddonAba *out, int max) { (void)out; (void)max; return 0; }
unsigned fontepref_revisao(void) { return 0; }

int ajustes_fonte_prioridade(void) { return 0; }
const char *ajustes_qualidade(void) { return "Automatica"; }
int ajustes_dolby_atmos(void) { return 0; }
int ajustes_dolby_vision(void) { return 0; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_hdr(void) { return 0; }
int ajustes_fonte_manual(void) { return 0; }
int ajustes_fonte_ordem_uso(void) { return 0; }
int ajustes_fonte_primeira(void) { return 0; }
int ajustes_fonte_regex_modo(void) { return 0; }
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_fonte_idioma_audio(void) { return 0; }
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
int fonteregra_no_escopo(const FonteRegraCfg *c, int plugin)
  { (void)c; (void)plugin; return 1; }
int fonteregra_ordem_rank(const char *nome) { (void)nome; return 0; }
unsigned fonteregra_versao(void) { return 0; }
int fonteregra_grupo(const FonteRegraCfg *c, const char *nome, int plugin, const char *texto)
  { (void)c; (void)nome; (void)plugin; (void)texto; return 0; }
int fontepref_escolher(const char *id) { (void)id; return -1; }
int fontepref_escolher_manual(const char *id) { (void)id; return -1; }
/* TV-incompatible hide and audioext: not asserted in #400 ordering. */
int ajustes_fonte_ocultar(int regra) { (void)regra; return 0; }
int ajustes_audio_externo(void) { return 0; }
int ajustes_audio_externo_ocultar_fora_cache(void) { return 0; }
int ajustes_dts_desligado(void) { return 0; }
void audioext_alvo(const char *alvo) { (void)alvo; }
int audioext_alternar(const char *url, const char *cabecalhos, const char *nome) {
  (void)url; (void)cabecalhos; (void)nome; return -1;
}
void audioext_lembrar_serie(const AudioExtIdent *id) { (void)id; }
int audioext_serie_procura(const char *alvo, AudioExtIdent *id) { (void)alvo; (void)id; return 0; }
void audioext_serie_achou(const char *url, const char *cabecalhos, const char *nome) {
  (void)url; (void)cabecalhos; (void)nome;
}
int audioext_marcada(const char *url) { (void)url; return 0; }
TxtFamilia txt_fonte_interface(void) { return TXT_FAMILIA_INTER; }
void txt_estilo_medidas(TxtEstilo estilo, int *corpo, int *peso) {
  (void)estilo; if (corpo) *corpo = 20; if (peso) *peso = 400;
}
int txt_largura(TxtEstilo estilo, const char *s) {
  (void)estilo; return s ? (int)(8 * strlen(s)) : 0;
}
TxtLinha txt_linha(TxtEstilo estilo, const char *s, int r, int g, int b, int a) {
  (void)estilo; (void)r; (void)g; (void)b; (void)a;
  TxtLinha l = {0}; l.w = s ? (int)(8 * strlen(s)) : 0; return l;
}
void vazao_fmt_mbps(char *dst, size_t n, int kbps, char sep) {
  (void)sep; if (dst && n) snprintf(dst, n, "%.1f", kbps / 1000.0);
}
void vazao_fmt_gb(char *dst, size_t n, double gb, char sep) {
  (void)sep; if (dst && n) snprintf(dst, n, "%.2f", gb);
}
int xtream_url_limitada(const char *url) { (void)url; return 0; }
static int modo;
static int negativa, consultas;
int debrid_resolver_cache(const char *h, int f, char *u, unsigned t, int *fora) {
  (void)h; (void)f; (void)u; (void)t; consultas++; *fora = negativa; return 0;
}
int rede_url_final_tipo(const char *u, int s, const char *const *c, char *d,
                        unsigned t, int *st, char *m, unsigned mt, long *b) {
  (void)u; (void)s; (void)c; (void)d; (void)t; (void)st; (void)m; (void)mt; (void)b;
  assert(!"esta fixture so consulta torrent"); return 0;
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; (void)st; assert(!"sem rede nesta fixture"); return NULL;
}
int ajustes_selos_coloridos(void) { return 0; }
int ajustes_idioma(void) { return IDIOMA_EN; }
int perfis_sessao_atual(uint64_t geracao) { (void)geracao; return 1; }
void selospacote_colorido(int ligado) { (void)ligado; }
int selospacote_embutidos_ok(void) { return 0; }
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }
const char *i18n(const char *s) { return s; }
int vazao_url_aviso(const char *url) { (void)url; return 0; }
int ajustes_fonte_ordem_addon(void) { return modo; }
int ajustes_fonte_texto_addon(void) { return 0; }
int debrid_ativo(void) { return 1; }
int p2p_ativo(void) { return 0; }
const char *ajustes_p2p_url(void) { return ""; }
float gfx_escala_ui(void) { return 1; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b,
                float x, float y, float w, float leading, float alpha, int max) {
  (void)e; (void)s; (void)r; (void)g; (void)b;
  (void)x; (void)y; (void)w; (void)leading; (void)alpha; (void)max;
  abort(); // This ordering fixture has no discarded-torrent footer to measure.
}
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int q, unsigned short *o, int m) {
  (void)p; (void)q; (void)o; (void)m; return 0;
}
int ondever_n(const char *id) { (void)id; return 0; }
Uint32 SDL_GetTicks(void) { return 1000; }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int dados_gravar(const char *nome, const char *s) { (void)nome; (void)s; return 1; }

static void conferir(const int *esperada, int qtd) {
  // montar() passou a recalcular melhorFolha pelo cache (d28ff9e77); esta fixture injeta
  // melhorFolha a mao, entao confere o montador sem cache.
  montarBase(-1);
  assert(nOrdem == qtd);
  for (int i = 0; i < qtd; i++) {
    if (ordem[i] != esperada[i]) {
      fprintf(stderr, "FAIL modo=%d linha=%d: indice=%d esperado=%d\n", modo, i, ordem[i], esperada[i]);
      exit(1);
    }
  }
  if (modo) for (int g = 0; g < FOLHA_GRUPOS; g++) assert(secN[g] == 0);
}
int main(void) {
  assert(stream_texto_fora_de_cache("Not cached"));
  assert(!stream_texto_fora_de_cache("cached WEB-DL Download"));
  const char *json = "{\"streams\":["
    "{\"url\":\"https://video.invalid/720.mp4\",\"name\":\"720p\",\"videoSize\":104857600},"
    "{\"url\":\"https://video.invalid/4k-small.mp4\",\"name\":\"2160p\",\"videoSize\":209715200},"
    "{\"url\":\"https://video.invalid/4k-large.mkv\",\"name\":\"2160p\",\"videoSize\":419430400}]}";
  Stream *a = NULL, *b = NULL;
  assert(stream_extrair(json, "Addon A", &a) == 3);
  assert(stream_extrair(json, "Addon B", &b) == 3);
  // B responde primeiro, embora A tenha sido instalado antes.
  stream_lista_acrescentar(b, 3, 1);
  stream_lista_acrescentar(a, 3, 0);
  free(a); free(b);
  filtro = 0; atualizarProvedores(); melhorFolha = -1;
  const int qualidade[] = {5,2,4,1,3,0}, addon[] = {3,4,5,0,1,2};
  modo = 0; conferir(qualidade, 6);
  melhorFolha = 0; const int promovida[] = {0,5,2,4,1,3};
  conferir(promovida, 6);
  modo = 1; conferir(addon, 6);
  // Nem melhor fonte nem StreamFit podem retirar uma fonte da ordem do addon.
  melhorFolha = 2; fitAtualizar(); fitClasses[4] = SF_PESADA;
  conferir(addon, 6);
  filtro = 1; const int apenasA[] = {3,4,5}; conferir(apenasA, 3);
  soMp4 = 1; const int mp4A[] = {3,4}; conferir(mp4A, 2);
  soMp4 = 0; filtro = 0; melhorFolha = -1; fitClasses[4] = SF_DESCONHECIDA;
  modo = 0; conferir(qualidade, 6);
  stream_definir_lista(NULL, 0); modo = 1; conferir(NULL, 0);
  // #402: cache negativo conhecido vai ao fim, desconhecido conserva a ordem.
  const char *mista = "{\"streams\":["
    "{\"infoHash\":\"a\",\"name\":\"⏳ 2160p\"},"
    "{\"url\":\"https://video.invalid/720.mp4\",\"name\":\"720p\"},"
    "{\"infoHash\":\"b\",\"name\":\"1080p\"},"
    "{\"infoHash\":\"c\",\"name\":\"⚡ 1080p cached\"}]}";
  assert(stream_extrair(mista, "NMR | ALL", &a) == 4);
  assert(a[0].foraCache && !a[2].foraCache && !a[3].foraCache);
  stream_definir_lista(a, 4); free(a);
  atualizarProvedores(); melhorFolha = -1;
  // Modo Nuvio: fora do cache no fim do PROPRIO grupo de resolucao (38f3dd35);
  // modo Do addon: no fim da lista inteira.
  const int mistaQ[] = {0,2,3,1}, mistaA[] = {1,2,3,0};
  modo = 0; conferir(mistaQ, 4);
  melhorFolha = 0; conferir(mistaQ, 4); // nem a promocao pode ressuscita-la
  modo = 1; conferir(mistaA, 4);
  assert(!stream_automatico_disponivel(0));
  assert(stream_automatico_disponivel(2) && stream_automatico_disponivel(3));
  assert(temMarca(0, -1)); // selo tambem no modo de texto do addon
  // Campo estruturado sem marca textual: false e diferente de ausente/true.
  assert(stream_extrair("{\"streams\":["
    "{\"name\":\"2160p\",\"clientResolve\":{\"infoHash\":\"x\",\"isCached\":false}},"
    "{\"name\":\"720p\",\"clientResolve\":{\"infoHash\":\"y\",\"isCached\":true}},"
    "{\"name\":\"1080p\",\"infoHash\":\"z\",\"behaviorHints\":{\"bingeGroup\":\"aio|1080p\"}}]}",
    "AIOStreams", &a) == 3);
  assert(a[0].foraCache && !a[1].foraCache && !a[2].foraCache);
  stream_definir_lista(a, 3); free(a); melhorFolha = -1;
  atualizarProvedores();
  const int hintQ[] = {0,2,1}, hintA[] = {1,2,0};
  modo = 0; conferir(hintQ, 3); modo = 1; conferir(hintA, 3);
  Conferencia c = {0}; c.geracao = listaGeracao;
  negativa = 0; assert(!verificarUma(2, &c)); // erro/sem resposta nao e negativa
  assert(!lista[2].foraCache && consultas == 1);
  negativa = 1; assert(!verificarUma(2, &c));
  assert(lista[2].foraCache && consultas == 2 && !stream_automatico_disponivel(2));
  assert(!verificarUma(2, &c) && consultas == 2); // nao consulta de novo
  const int consultadaQ[] = {0,2,1}, consultadaA[] = {1,0,2};
  modo = 0; conferir(consultadaQ, 3); modo = 1; conferir(consultadaA, 3);
  puts("fonte_ordem_addon: PASS (qualidade, addon, chegada inversa, filtros, melhor fonte, StreamFit, troca, vazio)");
}
