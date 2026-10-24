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
void dts_remux_set_audio_offset_ms(int ms) { (void)ms; }
