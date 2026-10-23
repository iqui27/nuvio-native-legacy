// Seek gesture of the .tpk (seekgesto.h): simulated Smart Remote key streams.
#include <stdio.h>
#include <math.h>
#include "../src/seekgesto.h"

static int falhas;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)
#define PERTO(a, b) (fabsf((a) - (b)) < 0.01f)

// Frames at 60 fps between t0 and t1 (glide), returns the distance.
static float deslizar(SeekGesto *g, const SeekParams *p, unsigned t0, unsigned t1) {
  float tot = 0;
  for (unsigned t = t0; t < t1; t += 16) tot += seekgesto_quadro(g, p, t, 0.016f);
  return tot;
}

// Hold from t0 for `ms`: first repeat after `atraso`, then every `rep` ms.
static float segurar(SeekGesto *g, const SeekParams *p, int dir, unsigned t0, unsigned ms,
                     unsigned atraso, unsigned rep, float dur) {
  float tot = seekgesto_tecla(g, p, dir, t0, dur, NULL);
  unsigned prox = t0 + atraso, t;
  for (t = t0; t <= t0 + ms; t += 16) {
    if (t >= prox) { tot += seekgesto_tecla(g, p, dir, t, dur, NULL); prox = t + rep; }
    tot += seekgesto_quadro(g, p, t, 0.016f);
  }
  return tot;
}

int main(void) {
  const float ep = 24 * 60.0f;
  SeekParams p = { 10, 5, 130, 200, 10, 50 };   // mechanism checks; the shipped values are below
  SeekGesto g;
  int m;

  // A single tap: exactly the tap step, at once.
  seekgesto_zerar(&g);
  CHECK(PERTO(seekgesto_tecla(&g, &p, 1, 1000, ep, &m), 10.0f) && m == SEEKG_TOQUE, "first tap = 10 s");
  CHECK(PERTO(deslizar(&g, &p, 1000, 2000), 0.0f), "a tap never glides");

  // Separate taps grow 5% each (stremio-like).
  CHECK(PERTO(seekgesto_tecla(&g, &p, 1, 1500, ep, &m), 10.5f) && m == SEEKG_TOQUE, "second tap = 10.5 s");
  CHECK(PERTO(seekgesto_tecla(&g, &p, 1, 2000, ep, &m), 11.025f), "third tap = 11.025 s");

  // Direction flip halves the remembered step.
  { float s = seekgesto_tecla(&g, &p, -1, 2600, ep, &m);
    CHECK(m == SEEKG_VIRADA && PERTO(s, -11.57625f * 0.5f), "flip = -half, got %.3f", s); }

  // Cap: never more than 10% of the duration per step.
  seekgesto_zerar(&g);
  for (int i = 0; i < 200; i++) seekgesto_tecla(&g, &p, 1, 1000 + i * 400, ep, NULL);
  CHECK(PERTO(g.passo, ep * 0.10f), "cap 10%% = 144 s, got %.2f", g.passo);

  // Hold detection: Smart Remote first repeat ~450 ms, then ~100 ms. With the
  // default 200 ms gap the 450 ms one is still a tap, the next ones are a hold.
  seekgesto_zerar(&g);
  seekgesto_tecla(&g, &p, 1, 1000, ep, NULL);
  seekgesto_tecla(&g, &p, 1, 1450, ep, &m); CHECK(m == SEEKG_TOQUE, "450 ms gap with 200 ms threshold = tap");
  seekgesto_tecla(&g, &p, 1, 1550, ep, &m); CHECK(m == SEEKG_SEGURA_INICIO && g.segurando, "100 ms gap = hold");
  seekgesto_tecla(&g, &p, 1, 1650, ep, &m); CHECK(m == SEEKG_REP_IGNORADA, "100 ms < 130 ms throttle: not counted");
  seekgesto_tecla(&g, &p, 1, 1750, ep, &m); CHECK(m == SEEKG_REPETICAO, "200 ms after last counted: counted");

  // With holdGap 500 the first repeat already starts the hold.
  { SeekParams q = p; q.holdGapMs = 500;
    seekgesto_zerar(&g);
    seekgesto_tecla(&g, &q, 1, 1000, ep, NULL);
    seekgesto_tecla(&g, &q, 1, 1450, ep, &m); CHECK(m == SEEKG_SEGURA_INICIO, "holdGap 500: first repeat = hold"); }

  // Throttle makes the remote's repeat rate irrelevant to the growth.
  { SeekGesto a, b; float da, db;
    seekgesto_zerar(&a); seekgesto_zerar(&b);
    da = segurar(&a, &p, 1, 1000, 3000, 450, 100, 7200.0f);
    db = segurar(&b, &p, 1, 1000, 3000, 450, 40, 7200.0f);
    CHECK(fabsf(da - db) / da < 0.08f, "100 ms vs 40 ms repeat: %.0f vs %.0f s", da, db);
    printf("hold 3 s (2 h movie): %.0f s, step now %.1f s, speed %.0f s/s\n", da, a.passo, seekgesto_vel(&a, &p)); }

  // Hold ends with no key for the vivo time; the step stays remembered.
  { float lembrado;
    seekgesto_zerar(&g);
    segurar(&g, &p, 1, 1000, 2000, 450, 100, ep);
    CHECK(g.segurando, "still holding at the end of the stream");
    deslizar(&g, &p, 3000, 3400);
    CHECK(!g.segurando, "hold over after the vivo time");
    lembrado = g.passo;
    // Tap after hold: same remembered step, then grows.
    CHECK(PERTO(seekgesto_tecla(&g, &p, 1, 3500, ep, &m), lembrado) && m == SEEKG_TOQUE,
          "tap after hold = remembered step %.1f", lembrado);
    CHECK(g.passo > lembrado, "and it keeps growing"); }

  // Glide speed = step / throttle.
  seekgesto_zerar(&g);
  seekgesto_tecla(&g, &p, -1, 1000, ep, NULL);
  seekgesto_tecla(&g, &p, -1, 1100, ep, NULL);
  { float v = seekgesto_vel(&g, &p);
    CHECK(PERTO(v, g.passo * 1000.0f / 130.0f), "speed = step/throttle");
    CHECK(seekgesto_quadro(&g, &p, 1110, 0.016f) < 0.0f, "glides backwards"); }

  // Commit forgets everything: next press is a fresh tap step.
  seekgesto_zerar(&g);
  CHECK(PERTO(seekgesto_tecla(&g, &p, 1, 9000, ep, NULL), 10.0f), "after commit: base step again");

  // Short file: cap below the tap step keeps the tap step.
  seekgesto_zerar(&g);
  CHECK(PERTO(seekgesto_tecla(&g, &p, 1, 1000, 60.0f, NULL), 10.0f), "60 s clip: tap 10 s");
  CHECK(PERTO(seekgesto_tecla(&g, &p, 1, 1600, 60.0f, NULL), 10.0f), "60 s clip: no growth past the tap step");

  // THE SHIPPED VALUES (owner, 20/10): replay of the TV log. Smart Remote held
  // (repeats ~100-117 ms) = a press each, +30%, never a glide.
  { SeekParams d = SEEKG_PADRAO; const float ep2 = 10215.6f;
    float esperado[] = { 5.0f, 6.5f, 8.45f, 10.985f, 14.2805f };
    unsigned t = 1000;
    seekgesto_zerar(&g);
    for (int i = 0; i < 5; i++, t += (i == 1 ? 362 : 100)) {
      float s2 = seekgesto_tecla(&g, &d, 1, t, ep2, &m);
      CHECK(m == SEEKG_TOQUE && PERTO(s2, esperado[i]), "press %d = %.2f s (got %.2f, mode %d)", i, esperado[i], s2, m);
      CHECK(PERTO(deslizar(&g, &d, t, t + 100), 0.0f), "no glide with the owner's values");
    }
    for (int i = 0; i < 60; i++, t += 100) seekgesto_tecla(&g, &d, 1, t, ep2, NULL);
    CHECK(PERTO(g.passo, ep2 * 0.04f), "cap 4%% of the length = %.1f s, got %.1f", ep2 * 0.04f, g.passo);
    { float antes = g.passo, s3 = seekgesto_tecla(&g, &d, -1, t + 300, ep2, &m);
      CHECK(m == SEEKG_VIRADA && PERTO(s3, -antes * 0.65f), "flip x0.65"); }
    // A frame clock taken BEFORE the key stamp must not end anything.
    { SeekParams h = d; h.holdGapMs = 500;
      seekgesto_zerar(&g);
      seekgesto_tecla(&g, &h, 1, 5000, ep2, NULL); seekgesto_tecla(&g, &h, 1, 5100, ep2, NULL);
      CHECK(g.segurando && seekgesto_quadro(&g, &h, 5090, 0.016f) > 0.0f, "frame clock 10 ms behind the key: still gliding"); } }

  if (falhas) { printf("seekgesto: %d falha(s)\n", falhas); return 1; }
  printf("seekgesto: ok\n");
  return 0;
}
