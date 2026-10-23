// Exercise the real selector with the existing offline subtitle harness.
#define main legmemoria_script
#include "legmemoria.c"
#undef main

int main(int argc, char **argv) {
  assert(argc == 2);
  if (!strcmp(argv[1], "synthetic")) {
    nLeg = 2;
    faixa(0, "en", 3, LING_LEG_COMUM);
    faixa(1, "fr", -2, LING_LEG_COMUM);
    strcpy(leg[0].codec, "S_TEXT/UTF8");
    strcpy(leg[1].codec, "S_TEXT/WEBVTT");
    ativa = 0;
    overlayAssumir(1, 1);
    assert(ativa == -1 && legOverlay == 1 && !legOverlayTV);
    overlayAssumir(0, 0);
    assert(ativa == 0 && legOverlayTV);
    strcpy(leg[0].codec, "S_TEXT/ASS");
    overlayAssumir(0, 0);
    assert(ativa == -1 && !legOverlayTV);
  } else {
    nLeg = 1; nAdds = 2;
    strcpy(adds[0].url, "http://s/first.srt");
    strcpy(adds[1].url, "http://s/second.srt");
    legExterna = 1;
    legendasui_id_addon(&adds[0], legExternaId);
    assert(legendaAtiva() == 1);
    nLeg = 3; // MKV probe publishes two omitted tracks after selection.
    assert(legendaAtiva() == 3);
    Legenda tmp = adds[0]; adds[0] = adds[1]; adds[1] = tmp;
    assert(legendaAtiva() == 4);
    nAdds = 0;
    assert(legendaAtiva() == -1);
  }
  puts("Subtitle selection: PASS");
  return 0;
}
