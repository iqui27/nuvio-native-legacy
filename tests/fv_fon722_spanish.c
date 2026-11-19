// #722 characterization (test-only, NOT a reporter reproduction): how the real
// parser (stream_extrair) fills the audio evidence channels for Spanish-ish
// payloads, and what evidenciaAudio/passaChips then decide for an "es" audio
// preference. Synthetic payloads only; the reporter payload is unknown.
// Kind: C = repeats an approved assertion already in tests/source_audio_language.c
// or tests/fon785_rotulos.c (same shape, Spanish values); M = mechanism
// characterization of current source, not an approved product contract.
#include "../src/streams.c"
#include <assert.h>

static void bits(const Stream *s, char *out, size_t tam) {
  out[0] = 0;
  for (int i = 0; i < fontefiltro_opcoes_n(FONTEFILTRO_LINGUAS); i++)
    if (s->filtro.valores[FONTEFILTRO_LINGUAS] & fontefiltro_bit(FONTEFILTRO_LINGUAS, i))
      snprintf(out + strlen(out), tam - strlen(out), "%s%s", out[0] ? "+" : "",
               fontefiltro_opcao(FONTEFILTRO_LINGUAS, i));
}

int main(void) {
  static const struct {
    const char *id, *kind, *pref, *campos, *prov;
    int evid, passa;  // expected evidenciaAudio (-1/0/1) and passaChips
  } c[] = {
    // Declared tracks win (streams.c evidenciaAudio first branch).
    {"declared-es", "M", "es", "\"audioLanguages\":[\"es\"]", 0, 1, 1},
    {"declared-spanish-name", "M", "es", "\"audioLanguages\":[\"Spanish\"]", 0, 1, 1},
    {"declared-track-spa", "M", "es", "\"audioTracks\":[{\"language\":\"spa\"}]", 0, 1, 1},
    // Release aliases (stream_audio_rotulos.inc) and the classifier names.
    {"title-es-code", "C", "es", "\"title\":\"Movie.2010.1080p.BluRay.es.x264.mkv\"", 0, 1, 1},
    {"title-castilian", "C", "es", "\"title\":\"Movie.2010.1080p.BluRay.Castilian.x264.mkv\"", 0, 1, 1},
    {"title-castellano", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.Castellano.x264.mkv\"", 0, 1, 1},
    {"desc-castellano", "M", "es", "\"description\":\"Movie.2010.1080p.WEB-DL.Castellano.x264.mkv\"", 0, 1, 1},
    {"filename-castellano", "M", "es", "\"behaviorHints\":{\"filename\":\"Movie.2010.1080p.WEB-DL.Castellano.x264.mkv\"}", 0, 1, 1},
    {"title-espanol-ascii", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.Espanol.x264.mkv\"", 0, 1, 1},
    // Current source gives UTF-8 "Español" in release text no evidence: the
    // alias table has only ASCII "Espanol" and fontefiltro.c linguaPalavras
    // splits words on ASCII isalnum. Characterized as-is; not a contract.
    {"title-espanol-utf8", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.Espa\xC3\xB1ol.x264.mkv\"", 0, -1, 0},
    {"title-bracket-espanol-utf8", "M", "es", "\"title\":\"Movie (2010) [Espa\xC3\xB1ol] 1080p WEB-DL x264\"", 0, -1, 0},
    // An explicit "Audio:" section is read as declared text (faixasCampoTexto).
    {"desc-audio-espanol-utf8", "M", "es", "\"description\":\"Audio: Espa\xC3\xB1ol\"", 0, 1, 1},
    {"title-spanish", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.Spanish.x264.mkv\"", 0, 1, 1},
    {"title-esp", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.ESP.x264.mkv\"", 0, 1, 1},
    // Generic language fields beat release labels (stream_parse.c:233).
    {"generic-languages-es", "M", "es", "\"languages\":[\"es\"],\"title\":\"Movie.2010.1080p.WEB-DL.mkv\"", 0, 1, 1},
    {"generic-language-spanish", "M", "es", "\"language\":\"Spanish\",\"title\":\"Movie 2010\"", 0, 1, 1},
    {"parsed-languages-spanish", "M", "es", "\"parsed\":{\"languages\":[\"Spanish\"]},\"title\":\"Movie 2010\"", 0, 1, 1},
    {"generic-castellano-value", "M", "es", "\"languages\":[\"Castellano\"],\"title\":\"Movie 2010\"", 0, 1, 1},
    {"generic-und-masks-castellano", "C", "es", "\"languages\":[\"und\"],\"title\":\"Movie.2010.1080p.WEB-DL.Castellano.x264.mkv\"", 0, -1, 0},
    {"generic-en-masks-castellano", "C", "es", "\"language\":\"en\",\"title\":\"Movie.2010.1080p.WEB-DL.Castellano.x264.mkv\"", 0, 0, 0},
    // Declared non-es masks later Spanish text.
    {"declared-en-masks-es-code", "C", "es", "\"title\":\"Movie.1080p.es.mkv\",\"audioLanguages\":[\"en\"]", 0, 0, 0},
    {"declared-en-masks-castellano", "C", "es", "\"audioLanguages\":[\"en\"],\"title\":\"Movie.2010.1080p.WEB-DL.Castellano.x264.mkv\"", 0, 0, 0},
    // Non-es release label masks parsed Spanish in the same title.
    {"label-fr-masks-spanish", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.VFF.Spanish.x264.mkv\"", 0, 0, 0},
    // Latino: label alias maps to es; the filter keeps a separate Latino option.
    {"title-latino", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.Latino.x264.mkv\"", 0, 1, 1},
    {"title-latam", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.LATAM.x264.mkv\"", 0, 1, 1},
    // Root languages is promoted to declared audio (stream_parse.c faixasGenerico)
    // and ling_casa folds es-419 into es, while the filter bit is Latino only.
    {"generic-es-419", "M", "es", "\"languages\":[\"es-419\"],\"title\":\"Movie 2010\"", 0, 1, 1},
    // MULTi alone names no language (fon785_rotulos.c; tr analogue in source_audio_language.c).
    {"title-multi", "C", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.MULTi.x264.mkv\"", 0, 0, 0},
    // No evidence anywhere: -1 hides (current policy, no owner decision recorded).
    {"no-language-text", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.x264.mkv\"", 0, -1, 0},
    {"no-fields", "M", "es", "\"behaviorHints\":{}", 0, -1, 0},
    {"pref-all-open", "C", "*", "\"title\":\"Movie.2010.1080p.WEB-DL.x264.mkv\"", 0, -1, 1},
    // Subtitles, provider and codec text are never audio.
    {"subtitle-languages-es", "C", "es", "\"subtitleLanguages\":[\"es\"]", 0, -1, 0},
    {"title-subtitles-spanish", "C", "es", "\"title\":\"Subtitles: Spanish\"", 0, -1, 0},
    {"subs-suffix-castellano", "M", "es", "\"title\":\"Movie.2010.1080p.WEB-DL.x264 | Subs: Castellano\"", 0, -1, 0},
    {"subtitles-object-es", "M", "es", "\"subtitles\":[{\"lang\":\"es\",\"url\":\"https://example.test/s.srt\"}],\"title\":\"Movie 2010\"", 0, -1, 0},
    {"provider-flag-es", "C", "es", "\"description\":\"Frozen.2013.1080p.WEB-DL.mkv\"", "\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8 IPTV", -1, 0},
    {"codec-dts-es", "C", "es", "\"title\":\"Movie.2010.1080p.BluRay.DTS-ES.x264.mkv\"", 0, -1, 0},
  };
  int falhas = 0, n_casos = (int)(sizeof c / sizeof *c);
  for (int i = 0; i < n_casos; i++) {
    char json[1024], lb[256]; Stream *v = NULL;
    snprintf(json, sizeof json, "{\"streams\":[{\"url\":\"https://example.test/v\",%s}]}", c[i].campos);
    assert(stream_extrair(json, c[i].prov ? c[i].prov : "fixture", &v) == 1);
    ling_local_audio(c[i].pref); lista = v; n = 1; soDub = 1;
    const char *pref = idiomaFiltroAudio();
    int e = evidenciaAudio(v, pref), p = passaChips(0);
    int ok = e == c[i].evid && p == c[i].passa;
    bits(v, lb, sizeof lb);
    printf("%-4s %s %-30s pref=%s declared=[%s] labels=[%s] text=[%s] bits=[%s] evid=%d/%d passa=%d/%d\n",
           ok ? "ok" : "FAIL", c[i].kind, c[i].id, pref, v->audioIdiomas, v->audioRotulos,
           v->audioTexto, lb, e, c[i].evid, p, c[i].passa);
    falhas += !ok;
    free(v);
  }
  // Seeded control (NOT parser output): fixed channel precedence in isolation.
  {
    Stream s = {0};
    s.filtroLido = 1;
    s.filtro.valores[FONTEFILTRO_LINGUAS] = fontefiltro_valores(FONTEFILTRO_LINGUAS, "es");
    snprintf(s.audioRotulos, sizeof s.audioRotulos, "fr");
    int e1 = evidenciaAudio(&s, "es");
    s.audioRotulos[0] = 0;
    int e2 = evidenciaAudio(&s, "es");
    int ok = e1 == 0 && e2 == 1;
    printf("%-4s S seeded-labels-fr-over-bits-es  evid=%d/0 then-without-labels evid=%d/1\n",
           ok ? "ok" : "FAIL", e1, e2);
    falhas += !ok;
  }
  printf("fv_fon722: %d cases, %d mismatches\n", n_casos + 1, falhas);
#ifndef FV722_PROBE
  if (falhas) return 1;
#endif
  return 0;
}
