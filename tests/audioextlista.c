// The audio menu's versions (owner 17/11): one row per language a source has,
// tiers (audio-only container, other container, converted), quality, copies,
// caps per language (24 preference / 6 others).
#include "../src/audioextlista.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static AudioExtEntrada fonte(const char *langs, int codec, int canais, int cache, double gb) {
  AudioExtEntrada e;
  memset(&e, 0, sizeof e);
  e.temUrl = 1; e.codec = codec; e.canais = canais; e.cache = cache;
  e.bytes = (uint64_t)(gb * 1e9);
  char tmp[64]; snprintf(tmp, sizeof tmp, "%s", langs);
  for (char *t = strtok(tmp, ","); t && e.nIdiomas < AUDIOEXT_IDIOMAS; t = strtok(NULL, ","))
    snprintf(e.idiomas[e.nIdiomas++], sizeof e.idiomas[0], "%s", t);
  return e;
}
// "pl" also matches "pol", like ling_casa.
static int casa(const char *code, const char *pref) {
  if (!strcasecmp(code, pref)) return 1;
  return (!strcasecmp(pref, "pl") && !strcasecmp(code, "pol")) ||
         (!strcasecmp(pref, "en") && !strcasecmp(code, "eng")) ||
         (!strcasecmp(pref, "de") && !strcasecmp(code, "ger"));
}

static AudioExtOpcoes opc(const char *p1, const char *p2) {
  AudioExtOpcoes o;
  memset(&o, 0, sizeof o);
  o.pref1 = p1; o.pref2 = p2; o.pref3 = "en"; o.semDts = 1; o.ocultarCopias = 1; o.casa = casa;
  return o;
}
static int conta(const AudioExtCand *c, int k, const char *l) {
  int n = 0;
  for (int i = 0; i < k; i++) n += l[0] ? !strcasecmp(c[i].idioma, l) : !c[i].idioma[0];
  return n;
}

int main(void) {
  static AudioExtEntrada e[256];
  static AudioExtCand c[512];
  AudioExtOpcoes o;
  int n = 0, k;
  e[n++] = fonte("en", AEXT_DDP, 6, 1, 8);            // 0 English
  e[n++] = fonte("pl", AEXT_DD, 6, 1, 3);             // 1 Polish DD, cached, small
  e[n++] = fonte("pol", AEXT_DDP, 6, 1, 6);           // 2 Polish DD+, cached
  e[n++] = fonte("pl", AEXT_DDP, 6, -1, 2);           // 3 Polish DD+, not cached
  e[n++] = fonte("pl", AEXT_DTS, 6, 1, 4);            // 4 Polish DTS: converted (hidden with DTS off)
  e[n++] = fonte("pl", AEXT_DDP, 6, 0, 0);            // 5 Polish DD+, cache and size unknown
  e[n++] = fonte("cs,en", AEXT_DDP, 6, 1, 5);         // 6 Czech + English: under BOTH
  e[n++] = fonte("de", AEXT_AAC, 2, 1, 1);            // 7 German AAC
  e[n++] = fonte("", AEXT_DDP, 6, 1, 1);              // 8 no declared language
  e[n++] = fonte("pl", AEXT_DDP, 6, 1, 1); e[n - 1].hls = 1;      // 9 HLS
  e[n++] = fonte("pl", AEXT_DDP, 6, 1, 1); e[n - 1].atual = 1;    // 10 playing now
  e[n++] = fonte("pl", AEXT_DDP, 6, 1, 1); e[n - 1].temUrl = 0;   // 11 torrent only
  e[n++] = fonte("pl", AEXT_DESCONHECIDO, 0, 1, 7);  // 12 Polish, codec and channels not declared
  e[n++] = fonte("pl", AEXT_DDP, 2, 1, 0.5);          // 13 Polish DD+ stereo, cached, smallest

  o = opc("pl", "");
  k = audioext_lista(e, n, &o, c, 64);
  // Polish: 5.1 by audio quality (DD+ cached 2, DD+ unknown cache 5, DD 1),
  // undeclared channels (12), stereo (13), not cached last (3).
  assert(c[0].indice == 2 && c[0].grupo == AEXT_G_PREF1);
  assert(c[1].indice == 5 && c[2].indice == 1);
  assert(c[3].indice == 12 && c[4].indice == 13 && c[5].indice == 3);
  // English (the third preference): the Czech+English one and 0, smaller first.
  assert(c[6].grupo == AEXT_G_PREF3 && c[6].indice == 6 && c[7].indice == 0);
  // No language, then the others (Czech: the same source again, German).
  assert(c[8].grupo == AEXT_G_SEM && c[8].indice == 8 && !c[8].idioma[0]);
  assert(c[9].grupo == AEXT_G_OUTRA && !strcmp(c[9].idioma, "cs") && c[9].indice == 6);
  assert(!strcmp(c[10].idioma, "de") && c[10].indice == 7);
  assert(k == 11);
  for (int i = 0; i < k; i++)
    assert(c[i].indice != 4 && c[i].indice != 9 && c[i].indice != 10 && c[i].indice != 11);
  // Hide not cached.
  o.ocultarForaCache = 1;
  k = audioext_lista(e, n, &o, c, 64);
  for (int i = 0; i < k; i++) assert(c[i].indice != 3);
  o.ocultarForaCache = 0;
  // DTS conversion on: the Polish DTS joins AFTER every version that plays as
  // it is, the not cached one included (owner 17/11).
  o.semDts = 0;
  k = audioext_lista(e, n, &o, c, 64);
  assert(c[5].indice == 3 && c[6].indice == 4);
  o.semDts = 1;
  // AUDIO-ONLY CONTAINERS FIRST (MP4: no picture bytes for its audio), then
  // MKV / others; FLAC and Opus play as they are, ranked by quality.
  { AudioExtEntrada f[6];
    f[0] = fonte("it", AEXT_DDP, 6, 1, 9);                       // MKV DD+
    f[1] = fonte("it", AEXT_AAC, 2, 1, 2); f[1].soAudio = 1;     // MP4 AAC 2.0
    f[2] = fonte("it", AEXT_DDP, 6, 1, 4); f[2].soAudio = 1;     // MP4 DD+ 5.1
    f[3] = fonte("it", AEXT_DDP, 6, 1, 8); f[3].atmos = 1;       // MKV DD+ Atmos
    f[4] = fonte("it", AEXT_FLAC, 6, 1, 20);                     // MKV FLAC 5.1
    f[5] = fonte("it", AEXT_TRUEHD, 8, 1, 30); f[5].soAudio = 1; // MP4 TrueHD: converted, last
    o = opc("it", ""); o.semDts = 0;
    k = audioext_lista(f, 6, &o, c, 64);
    assert(k == 6 && c[0].indice == 2 && c[1].indice == 1 && c[2].indice == 4 && c[3].indice == 3 && c[4].indice == 0 &&
           c[5].indice == 5);
    assert(audioext_nivel(&f[1]) == 0 && audioext_nivel(&f[0]) == 1 && audioext_nivel(&f[5]) == 2); }
  // WITH SUBTITLES FIRST (owner 18/11, the sync matches by them): the MKV DD+
  // with subtitles goes above the MP4s; a converted one with subtitles stays last.
  { AudioExtEntrada f[4];
    f[0] = fonte("it", AEXT_DDP, 6, 1, 4); f[0].soAudio = 1;     // MP4, no subtitles
    f[1] = fonte("it", AEXT_DDP, 6, 1, 9); f[1].legendas = 1;    // MKV with subtitles
    f[2] = fonte("it", AEXT_TRUEHD, 8, 1, 30); f[2].legendas = 1; // converted: last anyway
    f[3] = fonte("it", AEXT_DD, 6, 1, 5);                        // MKV, no subtitles
    o = opc("it", ""); o.semDts = 0;
    k = audioext_lista(f, 4, &o, c, 64);
    assert(k == 4 && c[0].indice == 1 && c[1].indice == 0 && c[2].indice == 3 && c[3].indice == 2); }
  // Settings > Addons switched its add-on off: not listed.
  o = opc("pl", "");
  e[7].foraFiltro = 1;
  k = audioext_lista(e, n, &o, c, 64);
  for (int i = 0; i < k; i++) assert(c[i].indice != 7);
  e[7].foraFiltro = 0;
  // Total limit.
  assert(audioext_lista(e, n, &o, c, 2) == 2);

  // "Sort like the addons": inside a language (and tier), the add-on's place
  // then the list's order; converted ones still last.
  { AudioExtEntrada f[5];
    memset(f, 0, sizeof f);
    f[0] = fonte("pl", AEXT_DTS, 8, 1, 60); f[0].ordemAddon = 0;     // add-on 0 sends a REMUX first
    f[1] = fonte("pl", AEXT_DDP, 6, 1, 4);  f[1].ordemAddon = 0;
    f[2] = fonte("pl", AEXT_DDP, 6, 1, 2);  f[2].ordemAddon = 1;     // a smaller one from add-on 1
    f[3] = fonte("en", AEXT_DDP, 6, 1, 3);  f[3].ordemAddon = 0;
    f[4] = fonte("pl", AEXT_DD, 6, -1, 1);  f[4].ordemAddon = 0;     // not cached, still in the add-on's place
    o = opc("pl", ""); o.semDts = 0; o.ordemAddon = 1;
    k = audioext_lista(f, 5, &o, c, 64);
    assert(k == 5 && c[0].indice == 1 && c[1].indice == 4 && c[2].indice == 2 && c[3].indice == 0 && c[4].indice == 3);
    o.ordemAddon = 0;
    k = audioext_lista(f, 5, &o, c, 64);
    assert(c[0].indice == 2 && c[1].indice == 1); }

  // COPIES OF ONE FILE: the same torrent file through two add-ons, and the
  // same size/codec/channels/languages without a hash. The cached one stays,
  // the others become its fallbacks, best first; off: all shown.
  n = 0;
  e[n++] = fonte("pl", AEXT_DDP, 6, 0, 4.2); snprintf(e[n - 1].ident, sizeof e[0].ident, "t:abc:1");  // 0
  e[n++] = fonte("pl", AEXT_DDP, 6, 1, 4.2); snprintf(e[n - 1].ident, sizeof e[0].ident, "t:abc:1");  // 1 cached copy
  e[n++] = fonte("pl", AEXT_DDP, 6, 1, 4.2); snprintf(e[n - 1].ident, sizeof e[0].ident, "t:abc:2");  // 2 another file, same size
  e[n++] = fonte("pl", AEXT_DD, 6, 1, 3.1);                                                         // 3
  e[n++] = fonte("pol", AEXT_DD, 6, -1, 3.1);                                                       // 4 copy of 3 (no hash)
  e[n++] = fonte("pl", AEXT_DD, 2, 1, 3.1);                                                         // 5 stereo: not a copy
  e[n++] = fonte("pl", AEXT_DD, 6, 1, 3.1); snprintf(e[n - 1].ident, sizeof e[0].ident, "v:ffff");  // 6 hash vs none: by size
  o = opc("pl", "");
  k = audioext_lista(e, n, &o, c, 64);
  assert(k == 4);
  assert(c[0].indice == 1 && c[0].nReservas == 1 && c[0].reservas[0] == 0);
  assert(c[1].indice == 2 && c[1].nReservas == 0);
  assert(c[2].indice == 3 && c[2].nReservas == 2 && c[2].reservas[0] == 6 && c[2].reservas[1] == 4);
  assert(c[3].indice == 5);
  assert(audioext_mesmo_arquivo(&e[0], &e[1]) && !audioext_mesmo_arquivo(&e[1], &e[2]));
  o.ocultarCopias = 0;
  assert(audioext_lista(e, n, &o, c, 64) == n);

  // CAPS PER LANGUAGE (owner 17/11, like the subtitles' 12 / 6): 24 for the
  // preference's languages (Polish, English), 6 for any other.
  n = 0;
  for (int i = 0; i < 40; i++) e[n++] = fonte("pl", AEXT_DDP, 6, 1, 1 + i);
  for (int i = 0; i < 30; i++) e[n++] = fonte("eng", AEXT_DDP, 6, 1, 1 + i);
  for (int i = 0; i < 7; i++) e[n++] = fonte("", AEXT_DDP, 6, 1, 1 + i);
  for (int i = 0; i < 11; i++) e[n++] = fonte("fr", AEXT_DDP, 6, 1, 1 + i);
  for (int i = 0; i < 3; i++) e[n++] = fonte("de", AEXT_DDP, 6, 1, 1 + i);
  o = opc("pl", ""); o.capPref = 24; o.capOutro = 6;
  k = audioext_lista(e, n, &o, c, 512);
  assert(conta(c, k, "pl") == 24 && conta(c, k, "eng") == 24 && conta(c, k, "") == 6 && conta(c, k, "fr") == 6 && conta(c, k, "de") == 3);
  for (int i = 0; i < 24; i++) assert(c[i].indice == i);   // the smallest kept
  // No caps: everything.
  o.capPref = o.capOutro = 0;
  assert(audioext_lista(e, n, &o, c, 512) == n);

  // One language written two ways ("de" and "ger") with another between them
  // alphabetically: one run, one language row (owner's log 15/11).
  n = 0;
  e[n++] = fonte("de", AEXT_DDP, 6, 1, 4);
  e[n++] = fonte("es", AEXT_DDP, 6, 1, 4);
  e[n++] = fonte("ger", AEXT_DDP, 6, 1, 3);
  e[n++] = fonte("de", AEXT_AAC, 2, 1, 1);
  o = opc("pl", "");
  k = audioext_lista(e, n, &o, c, 64);
  assert(k == 4);
  { int runs = 1;
    for (int i = 1; i < k; i++) runs += strcasecmp(c[i - 1].idioma, c[i].idioma) != 0;
    assert(runs == 2 && !strcmp(c[0].idioma, "de") && !strcmp(c[2].idioma, "de") && !strcmp(c[3].idioma, "es")); }
  // A source declaring one language twice ("en,eng") is one row.
  n = 0;
  e[n++] = fonte("en,eng,it", AEXT_DDP, 6, 1, 4);
  o = opc("", "");
  k = audioext_lista(e, n, &o, c, 64);
  assert(k == 2);

  assert(audioext_converte(AEXT_DTS) && audioext_converte(AEXT_TRUEHD) && !audioext_converte(AEXT_DDP));
  assert(!audioext_converte(AEXT_FLAC) && !audioext_converte(AEXT_OPUS));
  char t[32];
  audioext_codec_texto(AEXT_DDP, 6, t, sizeof t); assert(!strcmp(t, "DD+ 5.1"));
  audioext_codec_texto(AEXT_AAC, 2, t, sizeof t); assert(!strcmp(t, "AAC 2.0"));
  audioext_codec_texto(AEXT_FLAC, 2, t, sizeof t); assert(!strcmp(t, "FLAC 2.0"));
  audioext_codec_texto(AEXT_DESCONHECIDO, 6, t, sizeof t); assert(!t[0]);
  audioext_codec_texto(AEXT_TRUEHD, 8, t, sizeof t); assert(!strcmp(t, "TrueHD 7.1"));
  audioext_tamanho_texto(6100000000ULL, t, sizeof t); assert(!strcmp(t, "6.1 GB"));
  audioext_tamanho_texto(850000000ULL, t, sizeof t); assert(!strcmp(t, "850 MB"));
  audioext_tamanho_texto(0, t, sizeof t); assert(!t[0]);
  puts("audioextlista: filters, a row per language, tiers (audio-only / other / converted), quality, copies, caps 24/6 passed");
  return 0;
}
