// No network: the pure half of mkvfix.c (see src/mkvfix.h). A hand-built
// MKVToolNix/mkvpropedit header whose first SeekHead only points to a second
// SeekHead (the one that lists the Cues), and a normal header.
#include "mkvfix.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *m) { printf("%s %s\n", c ? "ok  " : "FAIL", m); if (!c) falhas++; }

#define ID_SEEKHEAD 0x114D9B74u
#define ID_CUES     0x1C53BB6Bu
#define ID_INFO     0x1549A966u

// One element: id (its natural width) + 1-byte size (payload < 127).
static long el(unsigned char *o, uint32_t id, const unsigned char *p, long n) {
  long k = 0;
  int w = id > 0xFFFFFF ? 4 : id > 0xFFFF ? 3 : id > 0xFF ? 2 : 1;
  for (int i = w - 1; i >= 0; i--) o[k++] = (unsigned char)(id >> (8 * i));
  o[k++] = (unsigned char)(0x80 | n);
  memcpy(o + k, p, (size_t)n);
  return k + n;
}
static long seek(unsigned char *o, uint32_t alvo, uint32_t pos) {
  unsigned char a[4] = { alvo >> 24, alvo >> 16, alvo >> 8, alvo }, b[4] = { pos >> 24, pos >> 16, pos >> 8, pos };
  unsigned char v[32]; long n = 0;
  n += el(v + n, 0x53AB, a, 4);
  n += el(v + n, 0x53AC, b, 4);
  return el(o, 0x4DBB, v, n);
}
// EBML header + Segment of unknown size + SeekHead (the given entries) + a
// Void that brings the SeekHead region to `regiao` bytes + Info.
static long arquivo(unsigned char *o, const uint32_t *ids, const uint32_t *pos, int n, int regiao, long *segIni) {
  unsigned char s[128], z[128] = {0};
  long k = 0, m = 0, sh;
  k += el(o, 0x1A45DFA3, (const unsigned char *)"\x42\x82\x88matroska", 11);
  o[k++] = 0x18; o[k++] = 0x53; o[k++] = 0x80; o[k++] = 0x67; o[k++] = 0x01;
  memset(o + k, 0xFF, 7); k += 7;   // unknown size
  *segIni = k;
  for (int i = 0; i < n; i++) m += seek(s + m, ids[i], pos[i]);
  sh = el(o + k, ID_SEEKHEAD, s, m); k += sh;
  k += el(o + k, 0xEC, z, regiao - sh - 2);
  k += el(o + k, ID_INFO, z, 8);
  return k;
}

int main(void) {
  unsigned char f[512], conserto[70];
  long segIni = 0, n;
  MkvfixCab c;
  uint32_t id[MKVFIX_ENTRADAS]; int64_t pos[MKVFIX_ENTRADAS];

  // THE CASE (logs 21/24, Baka and Test [KH] S1-01): one entry, to a second
  // SeekHead at the end; 70-byte SeekHead + Void region.
  { uint32_t i1[] = { ID_SEEKHEAD }, p1[] = { 900000 };
    n = arquivo(f, i1, p1, 1, 70, &segIni); }
  ok(mkvfix_ler_inicio(f, n, &c) == 1, "hidden index: Matroska header read");
  ok(c.segIni == segIni && c.regiao == segIni, "hidden index: the region starts at the first SeekHead");
  ok(c.regiaoN == 70, "hidden index: the region is the SeekHead plus the Void after it (70 B)");
  ok(c.n == 1 && mkvfix_entrada(c.id, c.pos, c.n, ID_SEEKHEAD) == 900000, "hidden index: one entry, to the second SeekHead");
  ok(mkvfix_entrada(c.id, c.pos, c.n, ID_CUES) < 0, "hidden index: the first SeekHead does not list the Cues");

  // The rewrite: Cues first, then the path to the second SeekHead, in exactly
  // the region size, so no offset of the file moves.
  { uint32_t ids[] = { ID_CUES, ID_SEEKHEAD, ID_CUES };   // repeated id stays out
    int64_t ps[] = { 5000000000LL, 900000, 1 };
    int e = mkvfix_montar(ids, ps, 3, conserto, sizeof conserto);
    int k = mkvfix_ler_seekhead(conserto, sizeof conserto, id, pos, MKVFIX_ENTRADAS);
    ok(e == 2, "rewrite: Cues and the second SeekHead fit, the duplicate is dropped");
    ok(k == 2 && mkvfix_entrada(id, pos, k, ID_CUES) == 5000000000LL, "rewrite: the new SeekHead lists the Cues (past 4 GB)");
    ok(mkvfix_entrada(id, pos, k, ID_SEEKHEAD) == 900000, "rewrite: the entry to the second SeekHead is kept");
    memcpy(f + c.regiao, conserto, sizeof conserto);
    ok(mkvfix_ler_inicio(f, n, &c) == 1 && c.regiaoN == 70 && mkvfix_entrada(c.id, c.pos, c.n, ID_CUES) == 5000000000LL,
       "rewrite: the patched header reads as a normal one, same region size");
    ok(!memcmp(f + 70 + segIni, "\x15\x49\xA9\x66", 4), "rewrite: the element after the region did not move");
  }
  { uint32_t ids[] = { ID_CUES }; int64_t ps[] = { 1234 };
    ok(mkvfix_montar(ids, ps, 1, conserto, 8) == 0, "rewrite: no room -> 0 (left to the player)"); }

  // NORMAL FILE: the first SeekHead lists the Cues; nothing to fix.
  { uint32_t i1[] = { ID_INFO, ID_CUES }, p1[] = { 100, 777777 };
    n = arquivo(f, i1, p1, 2, 90, &segIni); }
  ok(mkvfix_ler_inicio(f, n, &c) == 1 && mkvfix_entrada(c.id, c.pos, c.n, ID_CUES) == 777777,
     "normal file: the first SeekHead lists the Cues");
  ok(mkvfix_ler_inicio((const unsigned char *)"RIFF....WAVEfmt ", 16, &c) == 0, "not Matroska: 0");

  // SUBTITLE TRACKS HIDDEN FROM THE TV (S01E03 Itsuki Gang: a 71 KB vector ASS
  // event hangs the TV's own subtitle decoder). Tracks: video, audio, ASS, SRT,
  // PGS. With an ASS track present, every TEXT subtitle (ASS and SRT) is
  // hidden; video, audio and image subtitles stay.
  { unsigned char tr[512], te[128], cont[400];
    long nc = 0, nt;
    int64_t off[MKVFIX_OCULTAS];
    struct { unsigned char tipo; const char *codec; } fx[] = {
      { 0x01, "V_MPEGH/ISO/HEVC" }, { 0x02, "A_OPUS" }, { 0x11, "S_TEXT/ASS" },
      { 0x11, "S_TEXT/UTF8" }, { 0x11, "S_HDMV/PGS" } };
    long ini[5];
    for (int i = 0; i < 5; i++) {
      long m = 0; unsigned char num = (unsigned char)(i + 1);
      m += el(te + m, 0xD7, &num, 1);                 // TrackNumber
      m += el(te + m, 0x83, &fx[i].tipo, 1);           // TrackType
      m += el(te + m, 0x86, (const unsigned char *)fx[i].codec, (long)strlen(fx[i].codec));
      ini[i] = nc;
      nc += el(cont + nc, 0xAE, te, m);                // TrackEntry
    }
    nt = el(tr, 0x1654AE6B, cont, nc);                 // Tracks (4-byte id + 1-byte size)
    int k = mkvfix_legendas_texto(tr, nt, off, MKVFIX_OCULTAS);
    ok(k == 2, "subtitles: ASS present -> the two text subtitle tracks are hidden");
    // TrackEntry header (2) + TrackNumber (3) + TrackType id and size (2).
    ok(k == 2 && off[0] == 5 + ini[2] + 2 + 3 + 2 && off[1] == 5 + ini[3] + 2 + 3 + 2,
       "subtitles: offsets point at the TrackType values of the ASS and SRT tracks");
    ok(k == 2 && tr[off[0]] == 0x11 && tr[off[1]] == 0x11, "subtitles: the bytes to swap are TrackType 0x11");
    // After the swap the Tracks still parses, the tracks are "control" and
    // nothing is left to hide.
    if (k == 2) { tr[off[0]] = MKVFIX_TIPO_ESCONDIDA; tr[off[1]] = MKVFIX_TIPO_ESCONDIDA; }
    ok(mkvfix_legendas_texto(tr, nt, off, MKVFIX_OCULTAS) == 0, "subtitles: patched Tracks parses, nothing left to hide");
  }
  // Only SRT: left alone (the TV handles plain text; nothing to fix).
  { unsigned char tr[128], te[64]; long m = 0, nt; int64_t off[MKVFIX_OCULTAS];
    unsigned char num = 1, tipo = 0x11;
    m += el(te + m, 0xD7, &num, 1); m += el(te + m, 0x83, &tipo, 1);
    m += el(te + m, 0x86, (const unsigned char *)"S_TEXT/UTF8", 11);
    unsigned char cont[80]; long nc = el(cont, 0xAE, te, m);
    nt = el(tr, 0x1654AE6B, cont, nc);
    ok(mkvfix_legendas_texto(tr, nt, off, MKVFIX_OCULTAS) == 0, "subtitles: SRT only -> nothing hidden");
    ok(mkvfix_legendas_texto(tr, nt - 1, off, MKVFIX_OCULTAS) == -1, "subtitles: cut Tracks -> -1");
  }
  // Tracks position: from the SeekHead entry, else by walking the top level.
  { uint32_t i1[] = { 0x1654AE6B }, p1[] = { 4321 };
    n = arquivo(f, i1, p1, 1, 60, &segIni);
    ok(mkvfix_ler_inicio(f, n, &c) == 1 && mkvfix_tracks_pos(f, n, &c) == segIni + 4321,
       "tracks position: taken from the SeekHead"); }
  { uint32_t i1[] = { ID_INFO }, p1[] = { 60 };
    unsigned char z[4] = {0};
    n = arquivo(f, i1, p1, 1, 60, &segIni);
    long tpos = n;
    n += el(f + n, 0x1654AE6B, z, 4);
    ok(mkvfix_ler_inicio(f, n, &c) == 1 && mkvfix_tracks_pos(f, n, &c) == tpos,
       "tracks position: found walking the top-level elements"); }

  // #648: headers the Tizen player cannot send -> relay through the server.
  { ok(mkvfix_cab_fora_da_tv(NULL) == 0, "headers: none -> direct");
    ok(mkvfix_cab_fora_da_tv("") == 0, "headers: empty -> direct");
    ok(mkvfix_cab_fora_da_tv("User-Agent: Stremio\nCookie: a=b") == 0,
       "headers: UA+Cookie the TV sends -> direct");
    ok(mkvfix_cab_fora_da_tv("user-agent: x\ncookie: y") == 0,
       "headers: names are case-insensitive");
    ok(mkvfix_cab_fora_da_tv("Referer: https://cdn.example/") == 1,
       "headers: Referer -> relay");
    ok(mkvfix_cab_fora_da_tv("User-Agent: x\nOrigin: https://a.b\nCookie: c") == 1,
       "headers: one unsupported among supported -> relay"); }

  // ---- the point in the index (prefetch on leaving the app, 09/11) ----------
  // Cues: video track 1 at 0/2000/4000/6000 ms; subtitle track 3 at 3000 ms
  // (fewer points: never the chosen track). Positions relative to the Segment.
  { unsigned char cu[512]; int k = 0, ini;
    struct { unsigned t, faixa; unsigned pos; } pt[] = {
      {0, 1, 1000}, {2000, 1, 900000}, {3000, 3, 777777}, {4000, 1, 1800000}, {6000, 1, 2700000} };
    int64_t cueMs, maxMs, r;
    cu[k++] = 0x1C; cu[k++] = 0x53; cu[k++] = 0xBB; cu[k++] = 0x6B;
    cu[k++] = 0x01; cu[k++] = 0; cu[k++] = 0; cu[k++] = 0; cu[k++] = 0; cu[k++] = 0; cu[k++] = 0; cu[k++] = 0; // 8-byte size, set below
    ini = k;
    for (unsigned i = 0; i < sizeof pt / sizeof pt[0]; i++) {
      // CuePoint { CueTime(4 B) CueTrackPositions { CueTrack(1 B) CueClusterPosition(4 B) } }
      cu[k++] = 0xBB; cu[k++] = 0x80 | 17;
      cu[k++] = 0xB3; cu[k++] = 0x84;
      cu[k++] = (unsigned char)(pt[i].t >> 24); cu[k++] = (unsigned char)(pt[i].t >> 16); cu[k++] = (unsigned char)(pt[i].t >> 8); cu[k++] = (unsigned char)pt[i].t;
      cu[k++] = 0xB7; cu[k++] = 0x80 | 9;
      cu[k++] = 0xF7; cu[k++] = 0x81; cu[k++] = (unsigned char)pt[i].faixa;
      cu[k++] = 0xF1; cu[k++] = 0x84;
      cu[k++] = (unsigned char)(pt[i].pos >> 24); cu[k++] = (unsigned char)(pt[i].pos >> 16); cu[k++] = (unsigned char)(pt[i].pos >> 8); cu[k++] = (unsigned char)pt[i].pos;
    }
    cu[11] = (unsigned char)(k - ini);
    r = mkvfix_cues_posicao(cu, k, 3500, &cueMs, &maxMs);
    ok(r == 900000 && cueMs == 2000, "index point: 3.5 s -> the video cue at 2 s (not the subtitle cue at 3 s)");
    ok(maxMs == 6000, "index point: the last video cue time is reported (scale check)");
    ok(mkvfix_cues_posicao(cu, k, 6000, &cueMs, NULL) == 2700000 && cueMs == 6000, "index point: exactly on a cue");
    ok(mkvfix_cues_posicao(cu, k, 10, &cueMs, NULL) == 1000 && cueMs == 0, "index point: before the second cue -> the first");
    ok(mkvfix_cues_posicao(cu, k - 10, 99999, &cueMs, NULL) == 1800000, "index point: a cut index still gives the points that came");
    ok(mkvfix_cues_posicao(cu + 4, k - 4, 3500, NULL, NULL) < 0, "index point: not a Cues element -> -1");
    { unsigned char v[600]; memset(v, 0, sizeof v);
      v[0] = 0xEC; v[1] = 0x80 | 20;               // a Void before the Cues
      memcpy(v + 22, cu, (size_t)k);
      ok(mkvfix_cues_posicao(v, 22 + k, 3500, &cueMs, NULL) == 900000, "index point: the Cues after another element is found"); }
    { int64_t pm = 0, pp = 0;
      ok(mkvfix_cues_ponto(cu, k, 3500, &cueMs, NULL, &pm, &pp) == 900000 && pm == 4000 && pp == 1800000,
         "index point: the next video cue is reported (4 s, not the subtitle cue)");
      ok(mkvfix_cues_ponto(cu, k, 7000, &cueMs, NULL, &pm, &pp) == 2700000 && pm == -1 && pp == -1,
         "index point: after the last cue there is no next one"); }
    // The window (log 13): the TV asks for the Cluster that CONTAINS the second,
    // 9.9 MB after the cue in run 1 and 6.4 MB in run 2. Between two cues the
    // local rate places the estimate; the window starts 3 MiB before it.
    { const int64_t M = 1024 * 1024;
      int64_t j1 = mkvfix_janela_ini(335240, 0, 340710, 40 * M, 336465, 0, 3 * M);
      int64_t j2 = mkvfix_janela_ini(340710, 0, 345000, 30 * M, 341914, 0, 3 * M);
      ok(j1 <= 9884000 && j1 + 8 * M > 9884000, "window: run 1's request (9.9 MB after the cue) falls inside");
      ok(j2 <= 6357680 && j2 + 8 * M > 6357680, "window: run 2's request (6.4 MB after the cue) falls inside");
      ok(mkvfix_janela_ini(1000, 5 * M, 3000, 6 * M, 1100, 0, 3 * M) == 5 * M, "window: never before the cue");
      ok(mkvfix_janela_ini(1000, 0, 2000, 10 * M, 1999, 0, 0) <= 10 * M, "window: never after the next cue");
      ok(mkvfix_janela_ini(1000, 0, -1, -1, 3000, 2000.0, 1 * M) == 4000000 - M, "window: no next cue -> the file's average rate");
      ok(mkvfix_janela_ini(-1, -1, -1, -1, 3000, 0, M) == -1, "window: no cue -> -1");
      { int64_t j = mkvfix_janela_ini(0, 0, -1, -1, 1000000, 1e15, 3 * M);   // corrupt index / rate
        ok(j > 0 && j <= 4000000000000000000LL, "window: absurd values stay in range (no int64 overflow)"); } }
  }

  printf(falhas ? "mkvfix: %d FAIL(S)\n" : "mkvfix: ok\n", falhas);
  return falhas != 0;
}
