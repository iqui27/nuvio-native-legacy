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
// Every declared codec plays: DD/DD+/AAC/FLAC/Opus as they are, the rest
// converted (DTS only while DTS conversion is on).
static int codecUsavel(int c, int semDts) { return !(semDts && c == AEXT_DTS); }

typedef struct { AudioExtCand c; int cache, canais, codec, ordemAddon, nivel, atmos, legendas; uint64_t bytes; } Item;
static int ordemDoAddon;   // AudioExtOpcoes.ordemAddon, for cmpItem

// 5.1 or more first, then channels not declared (add-ons often leave them out
// of full releases), then stereo and mono; 7.1 and 5.1 are one step.
static int nivelCanais(int c) { return c >= 6 ? 0 : c <= 0 ? 1 : 2; }
// The best audio first, inside a tier. Converted: TrueHD, DTS, other.
static int qualidade(int codec, int atmos) {
  switch (codec) {
    case AEXT_FLAC: return 0;
    case AEXT_DDP: return atmos ? 1 : 2;
    case AEXT_DD: return 3;
    case AEXT_OPUS: return 4;
    case AEXT_AAC: return 5;
    case AEXT_DESCONHECIDO: return 6;
    case AEXT_TRUEHD: return 7;
    case AEXT_DTS: return 8;
    default: return 9;
  }
}
int audioext_nivel(const AudioExtEntrada *e) {
  if (!e) return 1;
  if (audioext_converte(e->codec)) return 2;
  return e->soAudio ? 0 : 1;
}

static int cmpItem(const void *pa, const void *pb) {
  const Item *a = pa, *b = pb;
  if (a->c.grupo != b->c.grupo) return a->c.grupo - b->c.grupo;
  if (a->c.grupo == AEXT_G_OUTRA) { int k = strcasecmp(a->c.idioma, b->c.idioma); if (k) return k; }
  // DTS/TrueHD (converted) stay last; above them, versions WITH SUBTITLES
  // first (owner 18/11: the sync matches subtitles before the audio, which
  // is quicker and surer), then audio-only containers, then the others.
  { int ca = a->nivel == 2, cb = b->nivel == 2; if (ca != cb) return ca - cb; }
  if (a->legendas != b->legendas) return b->legendas - a->legendas;
  if (a->nivel != b->nivel) return a->nivel - b->nivel;
  if (ordemDoAddon) {
    if (a->ordemAddon != b->ordemAddon) return a->ordemAddon - b->ordemAddon;
    return a->c.indice - b->c.indice;
  }
  if ((a->cache < 0) != (b->cache < 0)) return a->cache < 0 ? 1 : -1;
  if (nivelCanais(a->canais) != nivelCanais(b->canais)) return nivelCanais(a->canais) - nivelCanais(b->canais);
  { int qa = qualidade(a->codec, a->atmos), qb = qualidade(b->codec, b->atmos);
    if (qa != qb) return qa - qb; }
  if (a->cache != b->cache) return b->cache - a->cache;
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

static int pref(const char *l, const char *p) { return p && p[0] && casa(l, p); }
static int mesmaLingua(const AudioExtCand *a, const AudioExtCand *b) {
  if (a->grupo != b->grupo) return 0;
  if (a->grupo != AEXT_G_OUTRA) return 1;
  return !strcasecmp(a->idioma, b->idioma) || casa(a->idioma, b->idioma) || casa(b->idioma, a->idioma);
}

int audioext_lista(const AudioExtEntrada *e, int n, const AudioExtOpcoes *o, AudioExtCand *out, int max) {
  Item *it;
  int m = 0, k = 0, *mantido, cap = 0;
  if (!e || n <= 0 || !o || !out || max <= 0) return 0;
  it = malloc((size_t)n * AUDIOEXT_IDIOMAS * sizeof *it);
  mantido = malloc((size_t)n * AUDIOEXT_IDIOMAS * sizeof *mantido);
  if (!it || !mantido) { free(it); free(mantido); return 0; }
  casaFn = o->casa;
  for (int i = 0; i < n; i++) {
    const AudioExtEntrada *s = &e[i];
    int nl = s->nIdiomas > AUDIOEXT_IDIOMAS ? AUDIOEXT_IDIOMAS : s->nIdiomas;
    if (!s->temUrl || s->hls || s->naoVideo || s->atual || s->foraFiltro) continue;
    if (!codecUsavel(s->codec, o->semDts) || (o->ocultarForaCache && s->cache < 0)) continue;
    // One row per language it has (none: one row, "Unknown").
    for (int j = 0; j < (nl > 0 ? nl : 1); j++) {
      int g = AEXT_G_SEM, dup = 0;
      if (nl > 0) {
        const char *l = s->idiomas[j];
        for (int q = 0; q < j && !dup; q++) dup = !strcasecmp(s->idiomas[q], l) || casa(s->idiomas[q], l) || casa(l, s->idiomas[q]);
        if (dup) continue;
        g = pref(l, o->pref1) ? AEXT_G_PREF1 : pref(l, o->pref2) ? AEXT_G_PREF2 : pref(l, o->pref3) ? AEXT_G_PREF3 : AEXT_G_OUTRA;
      }
      memset(&it[m], 0, sizeof it[m]);
      it[m].c.indice = i; it[m].c.grupo = g;
      if (nl > 0) snprintf(it[m].c.idioma, sizeof it[m].c.idioma, "%s", s->idiomas[j]);
      it[m].cache = s->cache; it[m].canais = s->canais; it[m].codec = s->codec; it[m].bytes = s->bytes;
      it[m].ordemAddon = s->ordemAddon; it[m].nivel = audioext_nivel(s); it[m].atmos = s->atmos;
      it[m].legendas = s->legendas != 0;
      m++;
    }
  }
  // One spelling per language among the other languages, the first met: the
  // sort and the menu's language rows go by it (owner's log 15/11: "de" and
  // "ger" sorted apart, two German tabs with 5 and 1 versions).
  { char rep[128][sizeof it[0].c.idioma];
    int nr = 0;
    for (int i = 0; i < m; i++) {
      int r = 0;
      if (it[i].c.grupo != AEXT_G_OUTRA) continue;
      while (r < nr && strcasecmp(it[i].c.idioma, rep[r]) && !casa(it[i].c.idioma, rep[r]) && !casa(rep[r], it[i].c.idioma)) r++;
      if (r < nr) snprintf(it[i].c.idioma, sizeof it[i].c.idioma, "%s", rep[r]);
      else if (nr < 128) snprintf(rep[nr++], sizeof rep[0], "%s", it[i].c.idioma);
    }
  }
  ordemDoAddon = o->ordemAddon;
  qsort(it, (size_t)m, sizeof *it, cmpItem);
  ordemDoAddon = 0;
  // Copies of one file under one language: the first (best) stays, the rest
  // become its fallbacks. `mantido[i]` = the position of the row that kept
  // item i, or -1.
  for (int i = 0; i < m; i++) {
    mantido[i] = -1;
    if (!o->ocultarCopias) continue;
    for (int j = i - 1; j >= 0 && mesmaLingua(&it[j].c, &it[i].c); j--) {
      if (mantido[j] != -1 || it[j].c.indice == it[i].c.indice) continue;
      if (audioext_mesmo_arquivo(&e[it[i].c.indice], &e[it[j].c.indice])) {
        AudioExtCand *c = &it[j].c;
        if (c->nReservas < AUDIOEXT_RESERVAS) c->reservas[c->nReservas++] = it[i].c.indice;
        mantido[i] = j;
        break;
      }
    }
  }
  // The rows per language (owner 17/11, as the subtitles): capPref for the
  // preference's languages, capOutro for the others. The fallbacks of a
  // kept row ride with it (hidden copies never count).
  for (int i = 0; i < m && k < max; i++) {
    int limite;
    if (mantido[i] != -1) continue;
    if (i == 0 || !mesmaLingua(&it[i - 1].c, &it[i].c)) cap = 0;
    limite = it[i].c.grupo <= AEXT_G_PREF3 ? o->capPref : o->capOutro;
    if (limite > 0 && cap >= limite) continue;
    cap++;
    out[k++] = it[i].c;
  }
  free(it); free(mantido);
  casaFn = NULL;
  return k;
}

void audioext_codec_texto(int codec, int canais, char *out, unsigned tam) {
  const char *c = codec == AEXT_DDP ? "DD+" : codec == AEXT_DD ? "DD" : codec == AEXT_AAC ? "AAC" :
                  codec == AEXT_FLAC ? "FLAC" : codec == AEXT_OPUS ? "Opus" :
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
