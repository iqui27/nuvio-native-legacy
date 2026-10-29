/* Exercise the real primary renderer while native callbacks own fallback. */
#define faixas_legenda_overlay test_overlay
#define video_legenda_nativa test_native
#define legenda_cues test_cues
#include "../src/player.c"
#include <assert.h>

static int overlay, native_reads, collector_reads;
int faixas_legenda_overlay(void) { return overlay; }
int video_legenda_nativa(char *dst, int tam) {
  native_reads++;
  snprintf(dst, tam, "<i>Native fallback</i>");
  return 1;
}
int legenda_cues(double pos, int atraso, LegendaCue *out, int max) {
  (void)pos; (void)atraso; (void)max;
  collector_reads++;
  memset(out, 0, sizeof *out);
  snprintf(out->texto, sizeof out->texto, "Stale collector cue");
  return 1;
}

int main(void) {
  comVideo = 1;
  /* Both initial collection and a network retry can leave published cues. */
  for (int retry = 0; retry < 2; retry++) {
    float top = -1.f;
    desenharLegendaPrincipal(&top);
    assert(native_reads == retry + 1);
    assert(collector_reads == 0);
  }
  overlay = 1;
  float top = -1.f;
  desenharLegendaPrincipal(&top);
  assert(collector_reads == 1 && native_reads == 2);
  puts("primary renderer: native fallback survives stale collector cues; handoff OK");
}
