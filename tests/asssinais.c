// Signs & Songs matching (src/asssinais.c, owner 19/11/2026). See tests/asssinais.sh.
#include "asssinais.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define OK(c, m) do { if (c) printf("ok   %s\n", m); else { printf("FAIL %s\n", m); falhas++; } } while (0)

/* One BlockGroup { Block(track, rel, flags, payload), BlockDuration } */
static long bloco(unsigned char *b, int trk, int rel, const char *payload) {
  unsigned char blk[1200]; long bn = 0, pn = (long)strlen(payload), gn, o = 0;
  blk[bn++] = 0xA1;                         // Block
  blk[bn++] = (unsigned char)(0x40 | ((4 + pn) >> 8)); blk[bn++] = (unsigned char)((4 + pn) & 0xFF);   // 2-byte size
  blk[bn++] = (unsigned char)(0x80 | trk);  // track vint
  blk[bn++] = (unsigned char)((rel >> 8) & 0xFF); blk[bn++] = (unsigned char)(rel & 0xFF);
  blk[bn++] = 0x00;                         // flags
  memcpy(blk + bn, payload, (size_t)pn); bn += pn;
  blk[bn++] = 0x9B; blk[bn++] = 0x81; blk[bn++] = 0x10;   // BlockDuration 16
  gn = bn;
  b[o++] = 0xA0; b[o++] = (unsigned char)(0x40 | (gn >> 8)); b[o++] = (unsigned char)(gn & 0xFF);
  memcpy(b + o, blk, (size_t)gn);
  return o + gn;
}

int main(void) {
  unsigned char b[1400]; long n;
  unsigned h1 = asssinais_hash("{\\fad(200,200)\\k20}Kimi {\\k30}no\\Nnamae"),
           h2 = asssinais_hash("Kimi no namae"), h3 = asssinais_hash("  KIMI  {\\b1}no\\h namae "),
           h4 = asssinais_hash("Kimi no nawa");
  OK(h1 && h1 == h2 && h2 == h3, "hash: tags, \\N, \\h, spaces and case fold");
  OK(h4 != h2, "hash: other text differs");
  OK(asssinais_hash("{\\pos(10,10)}") == 0 && asssinais_hash("") == 0 && asssinais_hash(NULL) == 0, "hash: no visible text = 0");

  OK(asssinais_tags("{\\pos(1,2)}x") && asssinais_tags("{\\an8\\move(1,2,3,4)}x") && asssinais_tags("{\\p1}m 0 0 l 1 1") &&
     asssinais_tags("{\\iclip(1,2,3,4)}x") && asssinais_tags("{\\org(1,1)\\frz5}x"), "tags: libass's sign rule");
  OK(!asssinais_tags("{\\an8\\fad(1,1)}x") && !asssinais_tags("plain \\pos(1,2) outside braces"), "tags: dialogue stays dialogue");

  OK(asssinais_nome("Signs & Songs") && asssinais_nome("English [Signs/Songs]") && asssinais_nome("S&S") &&
     asssinais_nome("Typesetting") && asssinais_nome("Forced"), "name: signs & songs tracks");
  OK(!asssinais_nome("Full") && !asssinais_nome("Dialogue") && !asssinais_nome("Full (incl. Signs & Songs)") &&
     !asssinais_nome("English") && !asssinais_nome(NULL), "name: full / dialogue / plain");

  /* No partner: nothing matches, never active. */
  asssinais_definir(7, 0, 0, 1000000UL);
  OK(asssinais_relay_faixa() == 0 && !asssinais_ativo(7), "no partner: not active");
  n = bloco(b, 3, 100, "1,0,OP,,0,0,0,,{\\k20}Kimi no namae");
  asssinais_bloco(65000, b, n);
  OK(!asssinais_casa(7, 65100, h2), "no partner: a block is ignored");

  /* Partner track 3, TimestampScale 1 ms. */
  asssinais_definir(8, 0, 3, 1000000UL);
  OK(asssinais_relay_faixa() == 3 && !asssinais_trilha_sinais(8), "partner set");
  asssinais_bloco(65000, b, n);              // Cluster 65000 + 100 = 65100 ms
  OK(asssinais_ativo(8), "active with a partner (rule 2 can decide where known)");
  OK(asssinais_casa(8, 65100, h1), "match: same start, the Full line has other tags");
  OK(asssinais_casa(8, 65150, h2) && asssinais_casa(8, 65045, h2), "match: within 60 ms (also across a 100 ms bucket)");
  OK(!asssinais_casa(8, 65200, h2), "no match: 100 ms off");
  OK(!asssinais_casa(8, 65100, h4), "no match: other text");
  OK(!asssinais_casa(9, 65100, h2), "no match: other subtitle generation");
  n = bloco(b, 4, 100, "2,0,Default,,0,0,0,,Kimi no nawa");
  asssinais_bloco(65000, b, n);
  OK(!asssinais_casa(8, 65100, h4), "a block of another track is not read");
  n = bloco(b, 3, -50, "3,0,Sign,,0,0,0,,{\\pos(1,1)}Shuchiin");
  asssinais_bloco(70000, b, n);
  OK(asssinais_casa(8, 69950, asssinais_hash("Shuchiin")), "negative relative timestamp");
  /* TimestampScale 100 us: cluster 650000 units = 65000 ms. */
  asssinais_definir(10, 0, 3, 100000UL);
  n = bloco(b, 3, 1000, "1,0,OP,,0,0,0,,Kimi no namae");
  asssinais_bloco(650000, b, n);
  OK(asssinais_casa(10, 65100, h2), "TimestampScale applied");
  /* The same generation and partner again (a resumed harvest) keeps the lines. */
  asssinais_definir(10, 0, 3, 100000UL);
  OK(asssinais_casa(10, 65100, h2), "same track again: lines kept");
  /* A Signs & Songs track itself. */
  asssinais_definir(11, 1, 0, 1000000UL);
  OK(asssinais_trilha_sinais(11) && !asssinais_trilha_sinais(10) && !asssinais_ativo(11), "signs & songs track: all signs, no partner");
  /* Many lines: emptied when full, never stuck. */
  asssinais_definir(12, 0, 3, 1000000UL);
  { int i; char p[64];
    for (i = 0; i < 200000; i++) {
      snprintf(p, sizeof p, "%d,0,S,,0,0,0,,line %d", i, i);
      n = bloco(b, 3, 0, p);
      asssinais_bloco((unsigned long)i * 10u, b, n);
    }
    snprintf(p, sizeof p, "line %d", 199999);
    OK(asssinais_casa(12, 1999990, asssinais_hash(p)), "200 000 lines: the newest still found");
  }
  { AssSinaisEst e; asssinais_estatisticas(&e); OK(e.linhas > 0, "stats: lines counted");
    asssinais_estatisticas(&e); OK(e.linhas == 0 && e.consultas == 0, "stats reset"); }

  /* Owner 19/11/2026: libass's rule until the partner is known, then the
   * partner alone. Clusters every 5 s on one relay connection. */
  { int A = 1, B = 2; unsigned hs = asssinais_hash("Shuchiin"), hd = asssinais_hash("Ishigami");
    AssSinaisEst e;
    asssinais_definir(20, 0, 3, 1000000UL);
    OK(asssinais_ativo(20), "partner set: active before any line");
    OK(asssinais_decidir(20, 12000, hs) == -1, "nothing passed yet: waiting (libass rule)");
    asssinais_cluster(&A, 10000);
    n = bloco(b, 3, 2000, "3,0,Sign,,0,0,0,,{\\pos(1,1)}Shuchiin");
    asssinais_bloco(10000, b, n);
    OK(asssinais_decidir(20, 12000, hs) == 1, "a partner line: sign at once");
    OK(asssinais_decidir(20, 12000, hd) == -1, "not in the partner, cluster not whole yet: waiting");
    { unsigned v = asssinais_versao(); asssinais_cluster(&A, 15000);
      OK(asssinais_versao() != v, "known stretch grew: version moved (lines re-checked)"); }
    OK(asssinais_decidir(20, 12000, hd) == 0, "cluster passed, not in the partner: dialogue (whatever its tags)");
    OK(asssinais_decidir(20, 16000, hd) == -1, "the newest cluster is not whole: waiting");
    OK(asssinais_decidir(20, 9000, hd) == -1, "before the first cluster seen: waiting");
    asssinais_cluster(&A, 20000);
    OK(asssinais_decidir(20, 16000, hd) == 0, "the next cluster arrived: known");
    /* A jump (seek): the gap is never known. */
    asssinais_salto();
    asssinais_cluster(&A, 40000); asssinais_cluster(&A, 45000);
    OK(asssinais_decidir(20, 30000, hd) == -1, "after a jump: the skipped stretch stays unknown");
    OK(asssinais_decidir(20, 42000, hd) == 0, "after a jump: the new run is known");
    /* A big gap between two clusters on the same connection is a jump too. */
    asssinais_cluster(&A, 90000);
    OK(asssinais_decidir(20, 60000, hd) == -1, "a 45 s gap: not known");
    /* Another connection never extends this run. */
    asssinais_cluster(&B, 95000);
    asssinais_cluster(&A, 95000);
    OK(asssinais_decidir(20, 92000, hd) == -1, "clusters from two connections: no run across them");
    /* Seeking back into a known stretch and playing on past its end joins it. */
    asssinais_salto();
    asssinais_cluster(&A, 15000); asssinais_cluster(&A, 20000); asssinais_cluster(&A, 25000);
    OK(asssinais_decidir(20, 22000, hd) == 0 && asssinais_decidir(20, 12000, hd) == 0, "re-read and extended: known");
    /* The same track reloaded under a new generation keeps everything. */
    asssinais_trocar_geracao(20, 21);
    OK(asssinais_decidir(21, 12000, hs) == 1 && asssinais_decidir(21, 12000, hd) == 0, "generation 20 -> 21: lines and stretches kept");
    OK(asssinais_decidir(20, 12000, hs) == -1, "the old generation no longer decides");
    asssinais_trocar_geracao(99, 22);
    OK(asssinais_decidir(21, 12000, hs) == 1, "a swap from another generation is ignored");
    OK(asssinais_decidir_desenho(21, 12000, hs) == 1, "decide for drawing: same answer");
    asssinais_estatisticas(&e);
    OK(e.consultas > 0 && e.sinais > 0 && e.dialogos > 0 && e.esperando > 0 && e.trechos > 0 && e.guardadas > 0,
       "stats: signs, dialogue, waiting, stretches, held");
    { long antes; asssinais_estatisticas(&e); antes = e.consultas;
      asssinais_decidir_desenho(21, 12000, hs); asssinais_estatisticas(&e);
      OK(antes == 0 && e.consultas == 0, "decide for drawing is not counted"); }
    /* A new track forgets everything. */
    asssinais_definir(23, 0, 3, 1000000UL);
    OK(asssinais_decidir(23, 12000, hs) == -1, "new track: nothing known");
    /* More stretches than slots: never stuck, the newest kept. */
    { int i;
      for (i = 0; i < 100; i++) { asssinais_salto(); asssinais_cluster(&A, (unsigned long)(i * 100000)); asssinais_cluster(&A, (unsigned long)(i * 100000 + 5000)); }
      OK(asssinais_decidir(23, 99 * 100000 + 1000, hd) == 0, "100 stretches: the newest still known"); }
  }
  printf(falhas ? "asssinais: %d FAIL\n" : "asssinais: ok\n", falhas);
  return falhas != 0;
}
