// FON785: languages carried only by recognized release labels (audioRotulos)
// already pass the audio filter (evidenciaAudio) but the sheet showed no
// "Possível áudio" row. The inferred row must read the same evidence.
#include "../src/streams.c"
#include <assert.h>

int main(void) {
  char out[4096];
  Stream s = {0};
  s.filtroLido = 1;
  snprintf(s.audioRotulos, sizeof s.audioRotulos, "fr");
  assert(linhasFaixas(&s) == 1 && "label-only audio must reserve the row");
  assert(audioInferido(&s, out, sizeof out));
  assert(strstr(out, i18n("Francês")) && "VFF label must show French");
  // Declared tracks keep precedence over labels.
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "English");
  assert(!audioInferido(&s, out, sizeof out) && "declared audio wins");
  assert(linhasFaixas(&s) == 1 && "declared audio row unchanged");
  s.audioIdiomas[0] = 0;
  // No evidence anywhere: the inferred row stays absent.
  Stream vazio = {0};
  vazio.filtroLido = 1;
  assert(linhasFaixas(&vazio) == 0);
  assert(!audioInferido(&vazio, out, sizeof out));
  // MULTI alone never fabricates a language list.
  Stream m = {0};
  m.filtroLido = 1;
  snprintf(m.audioRotulos, sizeof m.audioRotulos, "es, en");
  assert(linhasFaixas(&m) == 1);
  assert(audioInferido(&m, out, sizeof out) &&
         strstr(out, i18n("Espanhol")) && strstr(out, i18n("Inglês")));
  puts("fon785 rotulos: ok");
  return 0;
}
