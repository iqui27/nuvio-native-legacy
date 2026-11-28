// The .tpk player's audio-from-another-source hooks (video_tpk.c) for the
// tests that link video_tpk.c without tests/dts_tpk_stub.c. All weak: a test
// that links the real module (or dts_tpk_stub.c) keeps its own.
#include <stddef.h>
#include "../src/doadorleg.h"
__attribute__((weak)) int  ajustes_audio_externo_auto(void) { return 1; }
__attribute__((weak)) int  ajustes_audio_externo_seguir(void) { return 0; }
__attribute__((weak)) void doadorleg_iniciar(double pos) { (void)pos; }
__attribute__((weak)) void doadorleg_cancelar(void) {}
__attribute__((weak)) int  doadorleg_resultado(double *k, double *d, double *c, char *m, size_t n) { (void)k; (void)d; (void)c; (void)m; (void)n; return 0; }
__attribute__((weak)) int  doadorleg_mapa_resultado(DoaMapa *o, char *m, size_t n) { (void)o; (void)m; (void)n; return 0; }
__attribute__((weak)) long memlog_livre_mb(void) { return -1; }
__attribute__((weak)) char *dados_ler(const char *nome) { (void)nome; return 0; }
__attribute__((weak)) int  dados_gravar_leve(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 0; }
// tpk45.c's watchdog log reads the converter's queues (dts_remux.c / dts_engine.c).
#include <string.h>
#include "../src/dts/dts_engine.h"
__attribute__((weak)) int dts_remux_available(void) { return 0; }
__attribute__((weak)) size_t dts_remux_queued_bytes(void) { return 0; }
__attribute__((weak)) int dts_remux_queue_budget_mib(void) { return 0; }
__attribute__((weak)) void dts_engine_rings(DtsEngineRings *out, int reset_peak) { (void)reset_peak; if (out) memset(out, 0, sizeof *out); }
