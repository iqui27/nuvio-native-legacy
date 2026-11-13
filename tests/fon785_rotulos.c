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
  // A generic language field (even unknown values) beats a release alias.
  const char *genericos[] = {"\"language\":\"en\"", "\"languages\":[\"und\"]"};
  for (int i = 0; i < 2; i++) {
    char json[256]; Stream *v = NULL;
    snprintf(json, sizeof json, "{\"streams\":[{\"url\":\"https://example.test/v\",%s,"
             "\"title\":\"Frozen.2013.1080p.WEB-DL.VFF.x264.mkv\"}]}", genericos[i]);
    assert(stream_extrair(json, "fixture", &v) == 1);
    assert(!v->audioRotulos[0] && evidenciaAudio(v, "fr") != 1);
    free(v);
  }
  puts("fon785 rotulos: ok");
  return 0;
}
