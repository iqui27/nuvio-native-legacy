"""Copy-first keeps the owned 4K pre-warm session while an optimistic copy is checked (upstream 55a4575f).

Production C: copiaPrimeiro cut from src/miniatura_android.c. Substituted: escolherCopiaRapida, adotarPre (the
owner-checked adoption is covered by miniatura_prewarm_lifecycle.py), trabalharDireto, mFechar, mSoltar,
limparDireto. Checks acceptance, rejection, copy-open fallback, cancel, no copy, no session: each handle closed
exactly once, the kept session's hardware decoder released before the copy's engine runs (one lease), and
the kept session handed to the playing-file pass only when the copy fails.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
text = (root / "src/miniatura_android.c").read_text()
a = text.index("static int copiaPrimeiro(")
fn = text[a:text.index("\nstatic void *trabalhar(", a)]

prefix = r'''
#include <pthread.h>
#include <stdio.h>
typedef struct { int parar, dCheck, dCopia, direto; } Job;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
enum { PRE = 7, COPIA = 8 };
static int escolhe, checkInicial, checkDepois, rCopia, temPre, decPre, adotou, fechou[16], soltou[16];
static int corridas, hwPreNaCopia, hParaPlay;
static int checkNaAdocao, pararNaAdocao;    // what the check thread / a cancel does while adotarPre waits
static long long hwVivo;    // handle holding the (single) hardware decoder, 0 = none
static int parou(Job *j) { return j->parar; }
static int escolherCopiaRapida(Job *j) { if (escolhe) { j->dCopia = 1; j->dCheck = checkInicial; } return escolhe; }
static long long adotarPre(Job *j, int *dec) {
  adotou++;
  if (checkNaAdocao) j->dCheck = checkNaAdocao;
  if (pararNaAdocao) j->parar = 1;
  *dec = temPre && decPre; if (*dec) hwVivo = PRE; return temPre ? PRE : 0;
}
static void mFechar(long long h) { if (h) fechou[h]++; if (hwVivo == h) hwVivo = 0; }
static void mSoltar(long long h) { soltou[h]++; if (hwVivo == h) hwVivo = 0; }
static void limparDireto(Job *j) { (void)j; }
static int trabalharDireto(Job *j, long long *hA) {
  corridas++;
  if (corridas == 1) {                 // the copy's direct engine wants the hardware decoder
    if (hwVivo == PRE) hwPreNaCopia = 1;
    *hA = COPIA; hwVivo = COPIA;
    j->dCheck = checkDepois;
    return rCopia;
  }
  hParaPlay = (int)*hA;                // the playing-file pass
  return 1;
}
'''

suffix = r'''
static int falhas;
#define CHECK(c, msg) do { if (c) printf("ok   %s\n", msg); else { printf("FAIL %s\n", msg); falhas++; } } while (0)
static int caso(int e, int ci, int cd, int r, int pre, int dec, int parar, long long *hA, int *tentado) {
  escolhe = e; checkInicial = ci; checkDepois = cd; rCopia = r; temPre = pre; decPre = dec;
  adotou = corridas = hwPreNaCopia = hParaPlay = 0; hwVivo = 0;
  for (int i = 0; i < 16; i++) fechou[i] = soltou[i] = 0;
  Job j = { parar, 0, 0, 0 };
  *hA = 0; *tentado = 0;
  int fim = copiaPrimeiro(&j, hA, tentado);
  if (*hA) mFechar(*hA);               // the worker's fim closes what it still holds
  return fim;
}
int main(void) {
  long long hA; int t;
  int fim = caso(1, 2, 2, 1, 1, 1, 0, &hA, &t);
  CHECK(fim && fechou[PRE] == 1 && !hParaPlay && !hwPreNaCopia, "copy accepted up front: 4K session closed first");
  fim = caso(1, 1, 2, 1, 1, 1, 0, &hA, &t);
  CHECK(fim && soltou[PRE] == 1 && !hwPreNaCopia, "optimistic check: kept session gives up its decoder before the copy runs");
  CHECK(fechou[PRE] == 1 && fechou[COPIA] == 1 && corridas == 1, "optimistic copy accepted: kept session closed once");
  fim = caso(1, 1, 3, 1, 1, 1, 0, &hA, &t);
  CHECK(fim && hParaPlay == PRE && fechou[COPIA] == 1 && fechou[PRE] == 1,
        "optimistic copy rejected: playing-file pass reuses the kept session, each closed once");
  fim = caso(1, 1, 3, 1, 1, 1, 1, &hA, &t);
  CHECK(fim && corridas == 1 && fechou[PRE] == 1 && fechou[COPIA] == 1, "rejected while canceled: kept session closed, no pass");
  fim = caso(1, 1, 1, 0, 1, 1, 0, &hA, &t);
  CHECK(!fim && hA == PRE && !t && fechou[COPIA] == 1 && fechou[PRE] == 1,
        "copy would not open: direct mode goes on with the kept session");
  fim = caso(0, 0, 0, 0, 1, 1, 0, &hA, &t);
  CHECK(!fim && !adotou && !hA && !t, "no copy chosen: the pre-warm stays parked for direct mode's own adoption");
  fim = caso(1, 1, 3, 1, 0, 0, 0, &hA, &t);
  CHECK(fim && hParaPlay == 0 && fechou[COPIA] == 1, "no pre-warm session: rejected copy opens the playing file anew");
  fim = caso(1, 1, 2, 1, 1, 0, 0, &hA, &t);
  CHECK(fim && !soltou[PRE] && fechou[PRE] == 1, "kept session without a decoder: nothing to release, closed once");
  checkNaAdocao = 3;                   // the check rejects the copy while adoption waits (1 -> 3)
  fim = caso(1, 1, 3, 1, 1, 1, 0, &hA, &t);
  CHECK(fim && hParaPlay == PRE && soltou[PRE] == 1 && !hwPreNaCopia && fechou[PRE] == 1 && fechou[COPIA] == 1,
        "rejected while adoption waits: kept session reused by the playing-file pass, decoder released first");
  pararNaAdocao = 1;                   // ... and the job is canceled meanwhile
  fim = caso(1, 1, 3, 1, 1, 1, 0, &hA, &t);
  CHECK(fim && corridas == 1 && !hParaPlay && fechou[PRE] == 1 && fechou[COPIA] == 1,
        "rejected and canceled while adoption waits: kept session closed once, no pass");
  pararNaAdocao = 0;
  checkNaAdocao = 2;                   // the check accepts the copy while adoption waits (1 -> 2)
  fim = caso(1, 1, 2, 1, 1, 1, 0, &hA, &t);
  CHECK(fim && !hParaPlay && !hwPreNaCopia && fechou[PRE] == 1 && fechou[COPIA] == 1,
        "accepted while adoption waits: kept session closed once before the copy's engine");
  checkNaAdocao = 0;
  if (falhas) { printf("FAIL: %d copy pre-warm checks\n", falhas); return 1; }
  puts("PASS: copy-first keeps/reuses/closes the owned 4K session (accept, reject, fallback, cancel)");
  return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="nuvio-copy-prewarm-") as tmp:
    c = Path(tmp) / "copy.c"
    c.write_text(prefix + fn + suffix)
    exe = Path(tmp) / "copy"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-g",
                    str(c), "-pthread", "-o", str(exe)], check=True)
    sys.exit(subprocess.run([str(exe)], timeout=60).returncode)
