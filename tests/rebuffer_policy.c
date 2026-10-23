// Run the production app failover path with Android signals and real policy.
#define NV_ANDROID 1
#define main playback_tests_main
#include "fontefalha_playback.c"
#undef main
void assrender_aliviar(void) {}
void memlog_devolver(void) {}
long memlog_livre_mb(void) { return 512; }
int memlog_pode_reabrir(long (*livre)(void), void (*aliviar)(void)) { (void)livre; (void)aliviar; return 1; }
void mkvass_parar(void) {}
void tex_aliviar(void) {}
void txt_aliviar(void) {}
int video_n_audio(void) { return 0; }
int video_n_legenda(void) { return 0; }
static int redeLenta, temProxima = 1;
int video_rede_lenta(void) { return redeLenta; }
int video_quadro_visto(void) { return tocou; }
int video_erro_decoder_codigo(void) { return 0; }
void stream_automatico_erro_decoder(int i, int c, int v) { (void)i; (void)c; (void)v; }
void video_estatisticas(VideoStats *s) { video_stats_vazio(s); }
int stream_falha_tem_proxima(const FonteFalhaSessao *s, unsigned g) {
  assert(s == fonteFalha && g == lista); return temProxima;
}
int main(int argc, char **argv) {
  assert(argc == 2); fontefalha_dir(argv[1]);
  for (int flag = 1; flag <= 2; flag++) {
    resetar(); redeLenta = flag; temProxima = 1; posicao = 123.5;
    tentarProximaFonteVOD();
    assert(worker && paradas == 1 && retomada == 123.5);
    assert(!legadas && !mortas && !irmas);
    concluirWorker(); processarFonteJob();
    assert(fonteEscolhida == 1 && fontefalha_tentativas(fonteFalha) == 2);
    resetar(); temProxima = 0;
    tentarProximaFonteVOD();
    assert(!worker && !paradas && fonteVODAutomatica);
  }
  limparFonteVOD(); puts("rebuffer policy: OK");
}
