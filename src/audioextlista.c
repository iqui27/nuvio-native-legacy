// Candidates for "From another source" in the audio menu (audioextlista.h).
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

typedef struct { AudioExtCand c; int cache, canais, codec, ordemAddon; uint64_t bytes; } Item;
static int ordemDoAddon;   // AudioExtOpcoes.ordemAddon, for cmpItem

// 5.1 or more first, then channels not declared (add-ons often leave them out
// of full releases), then stereo and mono; 7.1 and 5.1 are one step (the TV
// gets AC-3 5.1 either way).
static int nivelCanais(int c) { return c >= 6 ? 0 : c <= 0 ? 1 : 2; }

static int cmpItem(const void *pa, const void *pb) {
  const Item *a = pa, *b = pb;
  if (a->c.grupo != b->c.grupo) return a->c.grupo - b->c.grupo;
  if (a->c.grupo == AEXT_G_OUTRA) { int k = strcasecmp(a->c.idioma, b->c.idioma); if (k) return k; }
  if (ordemDoAddon) {
    if (a->ordemAddon != b->ordemAddon) return a->ordemAddon - b->ordemAddon;
    return a->c.indice - b->c.indice;
  }
  if (a->cache != b->cache) return b->cache - a->cache;
  if (nivelCanais(a->canais) != nivelCanais(b->canais)) return nivelCanais(a->canais) - nivelCanais(b->canais);
  if (a->codec != b->codec) return a->codec - b->codec;
  if (a->bytes != b->bytes) {
    if (!a->bytes) return 1;
    if (!b->bytes) return -1;
    return a->bytes < b->bytes ? -1 : 1;
  }
  return a->c.indice - b->c.indice;
}

static int mesmasLinguas(const AudioExtEntrada *a, const AudioExtEntrada *b) {
  if (a->nIdiomas != b->nIdiomas) return 0;
  for (int i = 0; i < a->nIdiomas && i < AUDIOEXT_IDIOMAS; i++) {
    int achou = 0;
    for (int j = 0; j < b->nIdiomas && j < AUDIOEXT_IDIOMAS && !achou; j++)
      achou = !strcasecmp(a->idiomas[i], b->idiomas[j]) || casa(a->idiomas[i], b->idiomas[j]);
    if (!achou) return 0;
  }
  return 1;
}
int audioext_mesmo_arquivo(const AudioExtEntrada *a, const AudioExtEntrada *b) {
  if (!a || !b) return 0;
  // Both name the file the same way (hash with hash, torrent with torrent):
  // that settles it, either way.
  if (a->ident[0] && a->ident[0] == b->ident[0]) return !strcmp(a->ident, b->ident);
  // Otherwise the exact size with the same audio and languages: in practice
  // the same release reached through two add-ons.
  return a->bytes && a->bytes == b->bytes && a->codec == b->codec && a->canais == b->canais &&
         mesmasLinguas(a, b);
}

int audioext_lista(const AudioExtEntrada *e, int n, const AudioExtOpcoes *o, AudioExtCand *out, int max) {
  Item *it;
  int m = 0, k = 0, *mantido;
  if (!e || n <= 0 || !o || !out || max <= 0) return 0;
  it = malloc((size_t)n * sizeof *it);
  mantido = malloc((size_t)n * sizeof *mantido);
  if (!it || !mantido) { free(it); free(mantido); return 0; }
  casaFn = o->casa;
  for (int i = 0; i < n; i++) {
    const AudioExtEntrada *s = &e[i];
    int g = AEXT_G_OUTRA, l = 0;
    if (!s->temUrl || s->hls || s->naoVideo || s->atual || s->foraFiltro) continue;
    if (!codecUsavel(s->codec, o->semDts) || (o->ocultarForaCache && s->cache < 0)) continue;
    if (s->nIdiomas <= 0) g = AEXT_G_SEM;
    for (int j = 0; j < s->nIdiomas && j < AUDIOEXT_IDIOMAS; j++) {
      if (casa(s->idiomas[j], o->pref1)) { g = AEXT_G_PREF1; l = j; break; }
      if (g > AEXT_G_PREF2 && casa(s->idiomas[j], o->pref2)) { g = AEXT_G_PREF2; l = j; }
    }
    memset(&it[m], 0, sizeof it[m]);
    it[m].c.indice = i; it[m].c.grupo = g;
    if (g != AEXT_G_SEM) snprintf(it[m].c.idioma, sizeof it[m].c.idioma, "%s", s->idiomas[l]);
    it[m].cache = s->cache; it[m].canais = s->canais; it[m].codec = s->codec; it[m].bytes = s->bytes;
    it[m].ordemAddon = s->ordemAddon;
    m++;
  }
  // One spelling per language among the other languages, the first met: the
  // sort and the bar's tabs go by it (owner's log 15/11: "de" and "ger"
  // sorted apart, two German tabs with 5 and 1 versions).
  { char rep[64][sizeof it[0].c.idioma];
    int nr = 0;
    for (int i = 0; i < m; i++) {
      int r = 0;
      if (it[i].c.grupo != AEXT_G_OUTRA) continue;
      while (r < nr && strcasecmp(it[i].c.idioma, rep[r]) && !casa(it[i].c.idioma, rep[r]) && !casa(rep[r], it[i].c.idioma)) r++;
      if (r < nr) snprintf(it[i].c.idioma, sizeof it[i].c.idioma, "%s", rep[r]);
      else if (nr < 64) snprintf(rep[nr++], sizeof rep[0], "%s", it[i].c.idioma);
    }
  }
  ordemDoAddon = o->ordemAddon;
  qsort(it, (size_t)m, sizeof *it, cmpItem);
  ordemDoAddon = 0;
  // Copies of one file: the first (best) stays, the rest become its fallbacks.
  // `mantido[i]` = the position in `it` of the row that kept item i, or -1.
  for (int i = 0; i < m; i++) {
    mantido[i] = -1;
    if (!o->ocultarCopias) continue;
    for (int j = 0; j < i; j++) {
      if (mantido[j] != -1) continue;
      if (audioext_mesmo_arquivo(&e[it[i].c.indice], &e[it[j].c.indice])) {
        AudioExtCand *c = &it[j].c;
        if (c->nReservas < AUDIOEXT_RESERVAS) c->reservas[c->nReservas++] = it[i].c.indice;
        mantido[i] = j;
        break;
      }
    }
  }
  for (int i = 0; i < m && k < max; i++)
    if (mantido[i] == -1) out[k++] = it[i].c;
  free(it); free(mantido);
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
