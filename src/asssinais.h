// SIGNS & SONGS (owner 19/11/2026, .tpk libass engine).
//
// The dialogue settings of an ASS track (Subtitles > Style: size, opacity,
// and "Dialogue by libass" Off, where the app draws the dialogue as its own
// text) must never touch the signs and songs. libass's own rule (\pos,
// \move, \clip, \iclip, \org, \pbo, a drawing, a Banner/Scroll Effect) misses
// the songs: OP/ED karaoke lines usually have none of those tags.
//
//   1. A track whose NAME says Signs/Songs/S&S/Typesetting/Forced (and not
//      Full/Dialogue) is all signs: no line of it is dialogue.
//   2. A Full/Dialogue track with such a partner in the same file (owner
//      19/11/2026: libass's rule first, until we know; then the partner
//      decides alone):
//      - the partner's lines around the line's start are not known yet:
//        libass's rule decides (\pos, \move, a drawing...);
//      - they are known: a line that is also in the partner (same start
//        within 60 ms, same text without tags) is a sign, one that is not is
//        dialogue, whatever its tags.
//      The partner is DATA ONLY: its lines (start + text) are read for free
//      from the local relay (mkvfix), the bytes the TV already downloads. No
//      fonts, no libass track, never drawn: the dialogue track carries the fonts.
//      "Known" = the relay passed that stretch of the file: every Cluster from
//      one to the next went through (asssinais_cluster), so all the partner's
//      blocks in between were seen.
//   3. Everything else: libass's rule, as before.
//
// mkvass sets the track info at the first delivery of a track's lines (and
// moves it along when the same track reloads under a new generation); the
// relay tap (mkvfix) hands the partner's blocks and Cluster times here.
// Thread-safe.
#ifndef NV_ASSSINAIS_H
#define NV_ASSSINAIS_H

// mkvass, first lines of the track delivered under subtitle generation `g`:
// the track itself is signs & songs (1/0), the partner track number to read
// from the relay (0 = none), the file's TimestampScale (ns). Forgets the old.
void asssinais_definir(unsigned g, int trilhaSinais, int faixaSinais, unsigned long escalaNs);
// mkvass: the same track reloaded under a new subtitle generation (e.g. the
// attached fonts arrived and the track was loaded again): the partner's lines
// and what is known stay. Nothing happens if `velha` is not the current one.
void asssinais_trocar_geracao(unsigned velha, unsigned nova);
// 1 = generation g's track is a Signs & Songs track (rule 1).
int  asssinais_trilha_sinais(unsigned g);
// 1 = generation g's track has a partner (rule 2 can decide where it is known).
int  asssinais_ativo(unsigned g);

// mkvfix relay tap: the partner track number now (0 = none).
int  asssinais_relay_faixa(void);
// mkvfix relay tap: one whole BlockGroup / SimpleBlock element of the partner
// track, in a Cluster with timestamp clTs.
void asssinais_bloco(unsigned long clTs, const unsigned char *b, long n);
// mkvfix relay tap `fluxo` (one per connection): a Cluster with timestamp
// clTs starts (while a partner is read). Clusters in a row on the same
// connection make the stretch between them known.
void asssinais_cluster(const void *fluxo, unsigned long clTs);
// mkvfix relay tap: the stream jumped (seek, skipped bytes): the next Cluster
// starts a new known stretch.
void asssinais_salto(void);

// Hash of an ASS Text field without override tags ({...}), \N \n \h and runs
// of spaces folded, ASCII lowercased. 0 = no visible text (never matches).
unsigned asssinais_hash(const char *assText);
// Rule 2: the line (start in ms, asssinais_hash of its Text) is in the partner.
int  asssinais_casa(unsigned g, long long inicioMs, unsigned hash);
// Rule 2 with "known": 1 = a sign (in the partner), 0 = dialogue (the partner
// is known there and lacks it), -1 = not known yet / no partner (libass's rule).
int  asssinais_decidir(unsigned g, long long inicioMs, unsigned hash);
// The same, not counted in the log (asked on every drawn frame).
int  asssinais_decidir_desenho(unsigned g, long long inicioMs, unsigned hash);
// Grows with every partner line added and every known stretch that grows
// (re-check a line still waiting).
unsigned asssinais_versao(void);
// libass's own sign rule on a Text field (\pos \move \clip \iclip \org \p..).
int  asssinais_tags(const char *assText);
// The track name rule (lowercase or not): Signs/Songs/S&S/Typesetting/Forced
// and not Full/Dialogue.
int  asssinais_nome(const char *nome);

// Log (since the last call): partner lines read, lookups, and how they were
// decided: sign by the partner, dialogue by the partner, waiting (libass's rule).
typedef struct {
  int linhas; long consultas, sinais, dialogos, esperando;
  int guardadas;               // partner lines held now
  int trechos;                 // known stretches now
  long long conhecidoIni, conhecidoFim;   // the newest stretch, ms (0/0 none)
} AssSinaisEst;
void asssinais_estatisticas(AssSinaisEst *e);

#endif
