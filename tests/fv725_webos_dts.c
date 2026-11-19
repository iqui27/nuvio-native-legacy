// #725: webOS DTS capability × hide Ligado/Desligado/Automático × auto-pick (simulated adapters; no GL/TV).
#define dts_playback_enabled fv725_dts_playback_enabled
#define nv_webos_major fv725_webos_major
#include "streams.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stub_perfisessao.h"

static int simDtsConvert;
static int simWebOsMajor;
int fv725_dts_playback_enabled(void) { return simDtsConvert; }
int fv725_webos_major(void) { return simWebOsMajor; }

static uint64_t fakeNow = UINT64_C(1700000000000);
static uint64_t fixtureNow(void) { return fakeNow; }
#define streamfit_agora_ms fixtureNow
#include "../src/streams.c"
#undef streamfit_agora_ms

static int hideAudio; /* 0=Ligado 1=Desligado 2=Automático */

int ajustes_fonte_ocultar(int regra) {
  if (regra < 0 || regra > 2) return 0;
  if (hideAudio == 1) return 0;
  if (hideAudio == 0) return 1;
  return stream_incompat_padrao(regra);
}

const char *ajustes_qualidade(void) { return "Automatica"; }
int ajustes_fonte_prioridade(void) { return 0; }
int ajustes_fonte_primeira(void) { return 0; }
int ajustes_fonte_ordem_uso(void) { return 0; }
int ajustes_fonte_ordem_addon(void) { return 0; }
int ajustes_fonte_manual(void) { return 0; }
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_regex_modo(void) { return 0; }
int ajustes_audio_externo(void) { return 0; }
void audioext_alvo(const char *alvo) { (void)alvo; }
int audioext_serie_procura(const char *alvo, AudioExtIdent *id) {
  (void)alvo; (void)id; return 0;
}
void audioext_serie_achou(const char *url, const char *cabecalhos, const char *nome) {
  (void)url; (void)cabecalhos; (void)nome;
}
int ajustes_fonte_hdr(void) { return 0; }
int ajustes_fonte_idioma_audio(void) { return 0; }
int ajustes_dolby_atmos(void) { return 0; }
int ajustes_dolby_vision(void) { return 0; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_dts_desligado(void) { return 0; }
int ajustes_selos_coloridos(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
int addons_n(void) { return 0; }
int debrid_ativo(void) { return 0; }
int p2p_ativo(void) { return 0; }
int ajustes_fonte_texto_addon(void) { return 0; }
int fonteregra_ordem_rank(const char *nome) { (void)nome; return 0; }
int fonteregra_no_escopo(const FonteRegraCfg *c, int plugin) { (void)c; (void)plugin; return 1; }
unsigned fonteregra_versao(void) { return 0; }
int fonteregra_grupo(const FonteRegraCfg *c, const char *nome, int plugin, const char *texto) {
  (void)c; (void)nome; (void)plugin; (void)texto; return 0;
}
int fontepref_escolher(const char *id) { (void)id; return -1; }
int fontepref_escolher_manual(const char *id) { (void)id; return -1; }
unsigned fontepref_revisao(void) { return 0; }
long fontesmortas_idade(const Stream *s) { (void)s; return -1; }
int naovideo_nome(const char *rotulo, const char *descricao, int altura, long tamanhoMB) {
  (void)rotulo; (void)descricao; (void)altura; (void)tamanhoMB; return 0;
}
const char *i18n(const char *s) { return s; }
float gfx_escala_ui(void) { return 1; }
Uint32 SDL_GetTicks(void) { return (Uint32)fakeNow; }
const char *ajustes_p2p_url(void) { return ""; }
int partes_disponivel(void) { return 0; }
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int n, unsigned short *o, int m) {
  (void)p; (void)n; (void)o; (void)m; return 0;
}
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }
void selospacote_colorido(int ligado) { (void)ligado; }
int selospacote_embutidos_ok(void) { return 0; }

static Stream mk(const char *desc, const char *file) {
  Stream s = {0};
  snprintf(s.url, sizeof s.url, "https://example.invalid/%s", file);
  snprintf(s.rotulo, sizeof s.rotulo, "%s", desc);
  snprintf(s.descricao, sizeof s.descricao, "%s", desc);
  snprintf(s.arquivo, sizeof s.arquivo, "%s", file);
  s.altura = 1080;
  s.filtroLido = 0;
  return s;
}

static void webos_no_convert_major5(void) {
  simDtsConvert = 0;
  simWebOsMajor = 5;
  stream_definir_tela(1, 0);
  stream_definir_decoder4k(1, 1, 1, -1);
  stream_definir_audio_capacidades(-1, -1, -1, -1, -1);
  (void)stream_incompat_padrao(1);
}

int main(void) {
  Stream pair[2];
  int pick;

  webos_no_convert_major5();
  hideAudio = 2;
  pair[0] = mk("1080p HEVC DTS", "Movie.DTS.mkv");
  pair[1] = mk("1080p HEVC AC3", "Movie.AC3.mkv");
  stream_definir_alvo("tt-fv725");
  stream_definir_lista(pair, 2);

  assert(incompatRegra(&pair[0]) == 2);
  assert(incompatRegra(&pair[1]) == 0);
  assert(incompatOculta(&pair[0]));
  assert(!incompatOculta(&pair[1]));
  assert(!stream_automatico_disponivel(0));
  assert(stream_automatico_disponivel(1));
  pick = stream_automatico();
  assert(pick == 1);
  puts("PASS: simulated webOS5 no DTS convert, Automático hides DTS-only when AC3 exists; auto avoids index 0");

  hideAudio = 1;
  assert(!ajustes_fonte_ocultar(1));
  assert(incompatRegra(&pair[0]) == 0);
  assert(!incompatOculta(&pair[0]));
  assert(stream_automatico_disponivel(0));
  pick = stream_automatico();
  assert(pick == 0 || pick == 1);
  puts("PASS: Desligado keeps DTS eligible for auto (no MOT_DECODER hide); pick not asserted to reject DTS");

  hideAudio = 0;
  assert(ajustes_fonte_ocultar(1));
  assert(incompatRegra(&pair[0]) == 2 && incompatOculta(&pair[0]));
  assert(stream_automatico() == 1);
  puts("PASS: Ligado hides unsupported DTS when AC3 fallback exists");

  Stream onlyDts[1] = { mk("1080p HEVC DTS", "Only.DTS.mkv") };
  hideAudio = 2;
  stream_definir_lista(onlyDts, 1);
  assert(incompatRegra(&onlyDts[0]) == 2);
  assert(!incompatOculta(&onlyDts[0]));
  assert(stream_automatico_disponivel(0));
  puts("PASS: all-incompatible list keeps DTS visible (incompatOculta src/streams.c:1336-1339)");

  puts("fv725_webos_dts: characterization complete (simulated webOS/DTS; not TV evidence)");
  return 0;
}
