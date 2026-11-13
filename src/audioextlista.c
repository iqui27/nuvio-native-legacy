// Candidates for "From another version" in the audio menu (audioextlista.h).
#include "audioextlista.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int (*casaFn)(const char *, const char *);
static int casa(const char *a, const char *b) {
  if (!a || !*a || !b || !*b) return 0;
  return casaFn ? casaFn(a, b) : !strcasecmp(a, b);
}
// Every declared codec plays: DD/DD+/AAC as they are, the rest converted
// (DTS only while DTS conversion is on).
static int codecUsavel(int c, int semDts) { return !(semDts && c == AEXT_DTS); }

typedef struct { AudioExtCand c; int cache, codec; uint64_t bytes; } Item;

static int cmpItem(const void *pa, const void *pb) {
  const Item *a = pa, *b = pb;
  if (a->c.grupo != b->c.grupo) return a->c.grupo - b->c.grupo;
  if (a->c.grupo == 2) { int k = strcasecmp(a->c.idioma, b->c.idioma); if (k) return k; }
  if (a->cache != b->cache) return b->cache - a->cache;
  if (a->codec != b->codec) return a->codec - b->codec;
  if (a->bytes != b->bytes) {
    if (!a->bytes) return 1;
    if (!b->bytes) return -1;
    return a->bytes < b->bytes ? -1 : 1;
  }
  return a->c.indice - b->c.indice;
}

int audioext_candidatos(const AudioExtEntrada *e, int n, const char *pref1, const char *pref2,
                        int ocultarForaCache, int semDts, int (*fn)(const char *, const char *),
                        AudioExtCand *out, int max) {
  Item *it;
  int m = 0, k = 0;
  if (!e || n <= 0 || !out || max <= 0) return 0;
  it = malloc((size_t)n * sizeof *it);
  if (!it) return 0;
  casaFn = fn;
  for (int i = 0; i < n; i++) {
    const AudioExtEntrada *s = &e[i];
    int g = 2, l = 0;
    if (!s->temUrl || s->hls || s->naoVideo || s->atual || s->nIdiomas <= 0) continue;
    if (!codecUsavel(s->codec, semDts) || (ocultarForaCache && s->cache < 0)) continue;
    for (int j = 0; j < s->nIdiomas && j < AUDIOEXT_IDIOMAS; j++) {
      if (casa(s->idiomas[j], pref1)) { g = 0; l = j; break; }
      if (g > 1 && casa(s->idiomas[j], pref2)) { g = 1; l = j; }
    }
    memset(&it[m], 0, sizeof it[m]);
    it[m].c.indice = i; it[m].c.grupo = g;
    snprintf(it[m].c.idioma, sizeof it[m].c.idioma, "%s", s->idiomas[l]);
    it[m].cache = s->cache; it[m].codec = s->codec; it[m].bytes = s->bytes;
    m++;
  }
  qsort(it, (size_t)m, sizeof *it, cmpItem);
  for (int i = 0, porIdioma = 0; i < m && k < max; i++) {
    // The preferred and the second language are one bucket each, whatever
    // code the add-on wrote ("pl", "pol"); the others group by their code.
    if (i && it[i].c.grupo == it[i - 1].c.grupo &&
        (it[i].c.grupo < 2 || !strcasecmp(it[i].c.idioma, it[i - 1].c.idioma))) porIdioma++;
    else porIdioma = 0;
    if (porIdioma < AUDIOEXT_POR_IDIOMA) out[k++] = it[i].c;
  }
  free(it);
  casaFn = NULL;
  return k;
}

void audioext_codec_texto(int codec, int canais, char *out, unsigned tam) {
  const char *c = codec == AEXT_DDP ? "DD+" : codec == AEXT_DD ? "DD" : codec == AEXT_AAC ? "AAC" :
                  codec == AEXT_DTS ? "DTS" : codec == AEXT_TRUEHD ? "TrueHD" : NULL;
  if (!out || !tam) return;
  if (!c) { out[0] = 0; return; }
  if (canais >= 8) snprintf(out, tam, "%s 7.1", c);
  else if (canais == 7) snprintf(out, tam, "%s 6.1", c);
  else if (canais >= 6) snprintf(out, tam, "%s 5.1", c);
  else if (canais == 2) snprintf(out, tam, "%s 2.0", c);
  else snprintf(out, tam, "%s", c);
}

void audioext_tamanho_texto(uint64_t bytes, char *out, unsigned tam) {
  if (!out || !tam) return;
  if (!bytes) { out[0] = 0; return; }
  if (bytes >= UINT64_C(1000000000)) snprintf(out, tam, "%.1f GB", (double)bytes / 1e9);
  else snprintf(out, tam, "%u MB", (unsigned)(bytes / 1000000u));
}

int audioext_converte(int codec) { return codec == AEXT_DTS || codec == AEXT_TRUEHD || codec == AEXT_OUTRO; }
