// Stubs da conversao DTS/TrueHD do .tpk (src/dts/dts_hls_tpk.c, ajustes.c,
// proxyts.c) para os testes que linkam src/video_tpk.c contra um host falso.
// O ajuste de teste fica em "Desligado" e o conversor indisponivel: o
// video_tpk.c se comporta como o master, que e o que esses testes fixam.
#include "../src/dts/dts_hls_tpk.h"
#include "../src/video.h"
int proxyts_e_url(const char *u) { (void)u; return 0; }
int ajustes_dts_tpk_modo(void) { return DTSTPK_DESLIGADO; }
int ajustes_dts_ac3(void) { return 1; }
int dts_tpk_disponivel(void) { return 0; }
const char *dts_tpk_modo_nome(int m) { (void)m; return "-"; }
int dts_tpk_abrir(const char *url, const char *headers, int audio_ordinal, int stereo_aac,
                  int modo, double inicio, char *out, size_t n) {
  (void)url; (void)headers; (void)audio_ordinal; (void)stereo_aac; (void)modo; (void)inicio;
  if (out && n) out[0] = 0;
  return 0;
}
int dts_tpk_url_em(double s, char *out, size_t n) { (void)s; if (out && n) out[0] = 0; return 0; }
void dts_tpk_fechar(void) {}
int dts_tpk_estado(void) { return DTSTPK_NADA; }
const char *dts_tpk_erro(void) { return ""; }
double dts_tpk_duracao(void) { return 0; }
unsigned dts_tpk_segmentos(void) { return 0; }
double dts_tpk_bytes_por_s(void) { return 0; }
double dts_tpk_segmento_max(void) { return 0; }
int dts_tpk_modo(void) { return -1; }
double dts_tpk_inicio_stream(void) { return -1; }
const char *dts_tpk_caminho(void) { return ""; }
int dts_tpk_prebuffer_ms(void) { return 0; }
int dts_tpk_sem_indice(void) { return 0; }
int dts_tpk_e_url(const char *u) { (void)u; return 0; }
// video.c (o selo do DTS no player) nao entra nesses testes.
int video_faixa_dts(const VideoFaixa *f) { (void)f; return 0; }
// video_tpk.c passes audio offset to remuxer (convAbrirSessao).
int  ajustes_audio_offset_ms(void) { return 0; }
int  ajustes_audio_externo_auto(void) { return 1; }
int  ajustes_audio_externo_seguir(void) { return 0; }
void dts_remux_set_audio_offset_ms(int ms) { (void)ms; }
/* The subtitle match (doadorleg.c): its own test, tests/doadorleg.sh. */
#include "../src/doadorleg.h"
void doadorleg_iniciar(double pos) { (void)pos; }
void doadorleg_cancelar(void) {}
int  doadorleg_resultado(double *k, double *d, double *c, char *m, size_t n) { (void)k; (void)d; (void)c; (void)m; (void)n; return 0; }
int  doadorleg_mapa_resultado(DoaMapa *o, char *m, size_t n) { (void)o; (void)m; (void)n; return 0; }
/* The donor's light-mode check and the series memory of the audio match
 * (video_tpk.c doadorLeve): no data dir and no memory reading in these tests.
 * Weak, so a test that links dados.c / memlog.c keeps the real ones. */
__attribute__((weak)) long memlog_livre_mb(void) { return -1; }
__attribute__((weak)) char *dados_ler(const char *nome) { (void)nome; return 0; }
__attribute__((weak)) int dados_gravar_leve(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 0; }
