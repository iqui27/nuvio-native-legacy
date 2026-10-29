// SEEK GESTURE FOR THE .tpk (owner, 20/10/2026): taps and holds share one
// remembered step, in the spirit of stremio-web's useKeyboardSeek, with a
// smooth glide while the key is held. Pure: no SDL, no globals, so
// tests/seekgesto.c can drive it with simulated key streams.
//
//  - The FIRST press of a scrub jumps one tap step at once (no waiting to
//    find out whether it is a hold).
//  - Every counted press (taps, and repeats throttled to one per throttleMs)
//    grows the remembered step by growthPct, never past capPct of the duration.
//  - HOLD: a keydown that arrives within holdGapMs of the previous one in the
//    same direction. While held, the position glides at step / throttleMs.
//  - TAP AFTER HOLD: the hold ended (no key for SEEKG_VIVO_MS) but the scrub
//    is still open, so a tap jumps the remembered (grown) step: same speed as
//    the hold.
//  - DIRECTION FLIP: the remembered step is multiplied by flipPct (homing in).
//  - The memory lives until the seek commits (seekgesto_zerar).
//
// THE NUMBERS (SEEKG_PADRAO) are the owner's, tuned on the TV with a live
// panel (20/10/2026): tap 5 s, +30% per press, cap 4% of the length, flip
// x0.65. With hold detection at 80 ms and the Smart Remote repeating every
// ~100-117 ms, a held key never enters the glide: every repeat is a press that
// jumps the step and grows it 30% — stremio-web's discrete feel, which is what
// felt right. The glide stays for remotes that repeat faster than 80 ms.
#ifndef NV_SEEKGESTO_H
#define NV_SEEKGESTO_H

typedef struct {
  int tapS;        // base tap step, seconds
  int growthPct;   // growth per counted press, percent
  int throttleMs;  // at most one counted repeat per this many ms (also the glide tick)
  int holdGapMs;   // keydown this close to the previous one = held key
  int capPct;      // step never above this percent of the duration
  int flipPct;     // direction change: step *= flipPct/100
} SeekParams;

#define SEEKG_PADRAO { 5, 30, 40, 80, 4, 65 }
#define SEEKG_MIN_S  1.0f

typedef struct {
  int ativo, dir, segurando, contadas;
  float passo;               // remembered step (seconds)
  unsigned ultTecla, ultContada;
} SeekGesto;

// Time without a key after which a hold is over (the step stays remembered).
static inline unsigned seekgesto_vivo_ms(const SeekParams *p) {
  unsigned t = (unsigned)p->throttleMs * 2u, g = (unsigned)p->holdGapMs + 60u;
  if (t < 260u) t = 260u;
  return t > g ? t : g;
}

static inline float seekgesto_teto(const SeekParams *p, float dur) {
  float base = p->tapS > 0 ? (float)p->tapS : SEEKG_MIN_S;
  float t = dur > 0.0f ? dur * (float)p->capPct / 100.0f : 1e9f;
  return t > base ? t : base;
}

static inline float seekgesto_crescer(const SeekParams *p, float passo, float dur) {
  float n = passo * (1.0f + (float)p->growthPct / 100.0f), t = seekgesto_teto(p, dur);
  return n > t ? t : n;
}

static inline void seekgesto_zerar(SeekGesto *g) {
  g->ativo = g->dir = g->segurando = g->contadas = 0;
  g->passo = 0.0f; g->ultTecla = g->ultContada = 0;
}

// Modes reported to the caller (for the log).
enum { SEEKG_TOQUE = 1, SEEKG_SEGURA_INICIO, SEEKG_REPETICAO, SEEKG_REP_IGNORADA, SEEKG_VIRADA };

// One keydown in direction `dir` (+1/-1). Returns the IMMEDIATE jump in
// seconds, signed (0 = no jump: the glide does the moving). *modo says why.
static inline float seekgesto_tecla(SeekGesto *g, const SeekParams *p, int dir, unsigned agora,
                                    float dur, int *modo) {
  float salto;
  unsigned gap;
  int m;
  if (!g->ativo) {
    float base = p->tapS > 0 ? (float)p->tapS : SEEKG_MIN_S, t = seekgesto_teto(p, dur);
    g->ativo = 1; g->dir = dir; g->segurando = 0; g->contadas = 1;
    g->passo = base > t ? t : base;
    g->ultTecla = g->ultContada = agora;
    salto = g->passo;
    g->passo = seekgesto_crescer(p, g->passo, dur);
    if (modo) *modo = SEEKG_TOQUE;
    return dir * salto;
  }
  if (dir != g->dir) {
    g->dir = dir; g->segurando = 0; g->contadas++;
    g->passo *= (float)p->flipPct / 100.0f;
    if (g->passo < SEEKG_MIN_S) g->passo = SEEKG_MIN_S;
    g->ultTecla = g->ultContada = agora;
    if (modo) *modo = SEEKG_VIRADA;
    return dir * g->passo;
  }
  gap = agora - g->ultTecla;
  g->ultTecla = agora;
  if (g->segurando && gap > seekgesto_vivo_ms(p)) g->segurando = 0;   // frame didn't run
  if (g->segurando) {
    if (agora - g->ultContada < (unsigned)p->throttleMs) m = SEEKG_REP_IGNORADA;
    else {
      // Advance by whole ticks so a 100 ms remote and a 40 ms keyboard both
      // average one counted repeat per throttleMs (no quantization drift).
      g->ultContada += (unsigned)p->throttleMs;
      if (agora - g->ultContada >= (unsigned)p->throttleMs) g->ultContada = agora - (unsigned)p->throttleMs;
      g->contadas++; g->passo = seekgesto_crescer(p, g->passo, dur); m = SEEKG_REPETICAO;
    }
    if (modo) *modo = m;
    return 0.0f;
  }
  if (p->holdGapMs > 0 && gap <= (unsigned)p->holdGapMs) {
    g->segurando = 1; g->ultContada = agora; g->contadas++;
    g->passo = seekgesto_crescer(p, g->passo, dur);
    if (modo) *modo = SEEKG_SEGURA_INICIO;
    return 0.0f;
  }
  g->ultContada = agora; g->contadas++;
  salto = g->passo;
  g->passo = seekgesto_crescer(p, g->passo, dur);
  if (modo) *modo = SEEKG_TOQUE;
  return dir * salto;
}

// Glide for this frame (seconds, signed). Ends the hold after SEEKG vivo time.
static inline float seekgesto_quadro(SeekGesto *g, const SeekParams *p, unsigned agora, float dt) {
  if (!g->ativo || !g->segurando || dt <= 0.0f) return 0.0f;
  // Signed: a key stamped after the frame clock must not read as ~49 days ago.
  if ((int)(agora - g->ultTecla) > (int)seekgesto_vivo_ms(p)) { g->segurando = 0; return 0.0f; }
  if (dt > 0.05f) dt = 0.05f;
  return g->dir * g->passo * 1000.0f / (float)(p->throttleMs > 0 ? p->throttleMs : 130) * dt;
}

// Glide speed (media seconds per real second) right now, for the log.
static inline float seekgesto_vel(const SeekGesto *g, const SeekParams *p) {
  return g->segurando ? g->passo * 1000.0f / (float)(p->throttleMs > 0 ? p->throttleMs : 130) : 0.0f;
}

#endif
