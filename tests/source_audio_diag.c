#include "../src/streams.c"
#include <assert.h>
#include <unistd.h>

// #611: the sheet snapshot logs one bounded aggregate audio-language line.
static Stream fonte(const char *campos, const char *addon, int mp4, int cache) {
  char json[512]; Stream *v = NULL, s;
  snprintf(json, sizeof json, "{\"streams\":[{\"url\":\"https://example.test/v\",%s}]}", campos);
  assert(stream_extrair(json, addon, &v) == 1);
  s = *v; free(v);
  s.mp4 = mp4; s.emCache = cache;
  return s;
}
// Runs a production path and returns its single audio-language log line.
static const char *capturar(void (*f)(void)) {
  static char linha[512];
  char modelo[1024], tudo[4096];
  snprintf(modelo, sizeof modelo, "%s/diag.XXXXXX", getenv("TMPDIR"));
  int fd = mkstemp(modelo), out = dup(1);
  assert(fd >= 0 && out >= 0);
  fflush(stdout); dup2(fd, 1);
  f();
  fflush(stdout); dup2(out, 1); close(out);
  ssize_t k = pread(fd, tudo, sizeof tudo - 1, 0);
  close(fd); unlink(modelo);
  tudo[k > 0 ? k : 0] = 0;
  const char *p = strstr(tudo, "[fonte] audio-lang");
  linha[0] = 0;
  if (p) snprintf(linha, sizeof linha, "%.*s", (int)strcspn(p, "\n"), p);
  assert(!strstr(tudo + (p ? (size_t)(p - tudo) + 1 : 0), "[fonte] audio-lang") && "one line per snapshot");
  return linha;
}
static const char *snapshot(void) { return capturar(folhaDados); }

int main(void) {
  const char *tr = "\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264.mkv\"";
  const char *tr720 = "\"title\":\"Frozen.2013.720p.WEB-DL.Turkish.x264.mkv\"";
  Stream f[] = {
    fonte("\"title\":\"Frozen 2013\"", "Alpha", 1, 1),          // missing evidence
    fonte("\"audioLanguages\":[\"en\"]", "Alpha", 1, 1),        // mismatch
    fonte(tr, "Alpha", 1, 1),                                   // visible match
    fonte("\"languages\":[\"tr\"]", "Alpha", 0, 1),             // Only MP4
    fonte(tr, "Alpha", 1, 0),                                   // In cache
    fonte(tr720, "Alpha", 1, 1),                                // saved rule
    fonte(tr, "Beta", 1, 1),                                    // other addon
    fonte(tr720, "Beta", 0, 0),                                 // all four at once
  };
  FonteFiltroConfig cfg = {0};
  fontefalha_dir(getenv("NUVIO_DADOS"));
  for (int k = 0; k < fontefiltro_opcoes_n(FONTEFILTRO_RESOLUCAO); k++)
    if (!strcmp(fontefiltro_opcao(FONTEFILTRO_RESOLUCAO, k), "720p"))
      cfg.excluir[FONTEFILTRO_RESOLUCAO] = fontefiltro_bit(FONTEFILTRO_RESOLUCAO, k);
  assert(cfg.excluir[FONTEFILTRO_RESOLUCAO] && fontefalha_filtro_definir(&cfg));
  ling_local_audio("tr");
  lista = f; n = sizeof f / sizeof *f;
  soMp4 = soCache = soDub = 1;
  atualizarProvedores();
  filtro = -1;
  for (int p = 1; p < nProvedores; p++) if (!strcmp(provedores[p], "Alpha")) filtro = p;
  assert(filtro > 0);
  const char *l = snapshot();
  puts(l);
  assert(!strcmp(l, "[fonte] audio-lang chip=1 sources=8 missing=1 mismatch=1 match=6 match_visible=1 "
                    "hidden_rules=2 hidden_mp4=2 hidden_cache=2 hidden_addon=2"));
  assert(!snapshot()[0] && "unchanged snapshot does not log again");
  soMp4 = 0;
  l = snapshot();
  puts(l);
  assert(!strcmp(l, "[fonte] audio-lang chip=1 sources=8 missing=1 mismatch=1 match=6 match_visible=2 "
                    "hidden_rules=2 hidden_mp4=0 hidden_cache=2 hidden_addon=2"));
  // Opening the sheet logs it once even when the snapshot is unchanged.
  l = capturar(stream_folha_abrir);
  assert(strstr(l, " sources=8 missing=1 mismatch=1 match=6 ") && !strstr(l, "Alpha") && !strstr(l, "Frozen"));
  ling_local_audio("*"); soDub = 0;
  assert(!snapshot()[0] && "All audio languages logs nothing");
  puts("source audio diagnostics: ok");
  return 0;
}
