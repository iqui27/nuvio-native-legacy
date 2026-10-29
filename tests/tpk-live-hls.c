// Exercise the real backend with the existing offline host fixture.
#define main tpk_recovery_fixture_main
#include "tpk-preso.c"
#undef main

static void openLive(const char *url, const char *headers) {
  (void)headers;
  assert(!strcmp(url, "http://test/live/user/pass/42.m3u8?token=fixture"));
  aberturas++;
}

int main(void) {
  nv_tpk_video_registrar(openLive, parar, pausar, buscar, NULL, NULL, pos);
  video_definir_mp4(0);
  video_definir_reconexao(0); // player.c uses !ehCanal() for this flag.
  video_tocar("http://test/live/user/pass/42.m3u8?token=fixture");
  assert(!reconPermitida);
  assert(mkvEstado == 0 && video_mkv_sondado() == 2);
  assert(!convPossivel());
  prontoHost();
  for (int seconds = 1; seconds <= 120; seconds++) {
    posicao = seconds * 1000;
    video_sondar_mkv_agora();
    passo(1000);
    assert(mkvEstado == 0 && !mkvSondarJa);
    assert(aberturas == 1 && !paradas && !video_terminou());
  }
  // Genuine EOF still reaches the app's existing live reopen fallback.
  nv_tpk_video_evento(4, 0, 0);
  assert(video_terminou());
  video_parar();

  // A later VOD session must still inspect MKV tracks.
  nv_tpk_video_registrar(abrir, parar, pausar, buscar, NULL, NULL, pos);
  video_definir_reconexao(1);
  video_tocar("http://test/movie");
  assert(reconPermitida && mkvEstado == 1 && video_mkv_sondado() == 0);
  video_parar();
  puts("PASS: live HLS keeps its URL, skips media probes, preserves EOF fallback and VOD probing");
  return 0;
}
