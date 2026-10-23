// FPS regression scenes: frame-time percentiles, a p95 gate against a stored
// baseline, and a scripted input runner for the TV (debug flag).
//
// Idea from the plx-native research (their tests/run.py fps scenes + gate):
// every smoothness decision needs a number, and a scene that fails when the
// number regresses. No GL/window, so the gate logic runs headless in
// tests/fpscena.sh; tests/fps_gate.sh compares captured logs offline, and
// main.c feeds it the TV's real frame intervals (including swap/waits).
//
// On the TV: create `fps-cena` in the data dir (any content; a line with
// scene names, e.g. "home-scroll", limits the run). The app scripts the D-pad,
// logs `[fps-cena] ...` per scene and, if `fps-cena-base` exists, the gate
// verdict (10% p95 tolerance + 0.5 ms slack). Empty flag = Home + title.
// Use "player-osd" separately during existing playback: waits for ready video,
// then toggles OSD with DOWN (never on live channels: DOWN opens the guide) without pausing, seeking or opening a source.
// One run per launch; the flag may be created after launch. FPSBASE lines can
// be saved as fps-cena-base, on the SAME device/settings/sample window.
// Off by default; flag checked once per second.
#ifndef NV_FPSCENA_H
#define NV_FPSCENA_H
#include <stddef.h>

#define FPS_MAX 1200

typedef struct {
  double ms[FPS_MAX];
  int    n;
  long   upl;     // texture uploads across the window
} FpsAmostra;

void   fps_zerar(FpsAmostra *a);
void   fps_add(FpsAmostra *a, double ms, int upl);
double fps_percentil(const FpsAmostra *a, double p);   // nearest rank, p in 0..100
double fps_pior(const FpsAmostra *a);

// One line: [fps-cena] <cena> n= p50= p95= p99= pior= upl=
void fps_linha(const FpsAmostra *a, const char *cena, char *out, size_t cap);

// Baseline text: one `<cena> <p95_ms> <upl>` per line, `#` comments. Returns 1
// and fills the values when the scene is there.
int fps_base_achar(const char *texto, const char *cena, double *p95, long *upl);

// Gate: fails when p95 > base*(1+tolPct/100)+folgaMs, or uploads grew past
// base*1.25+2 (uploads are deterministic; ms are not, hence the slack).
// Writes the verdict line; returns 1 on pass.
int fps_gate(const FpsAmostra *a, const char *cena, double p95Base, long uplBase,
             double tolPct, double folgaMs, char *out, size_t cap);

// ---- TV runner ------------------------------------------------------------
// Home needs a row of 4+ cards. Scripted scenes: home scroll, title page, player OSD. fpscena_quadro is called once per
// frame with that frame's time; it returns an SDL keycode to inject (0 = none).
// `base` is the fps-cena-base text or NULL; `escolha` limits scenes or NULL.
void fpscena_iniciar(const char *escolha, const char *base);
int  fpscena_ativo(void);
int  fpscena_falhas(void);   // gate failures so far
int  fpscena_quadro(double ms, int upl);
// Optional guard: ok(cena) must say the app is on that scene's expected screen.
// Checked at the start and end of each measured window; a miss fails the scene
// instead of measuring something else. Survives fpscena_iniciar.
void fpscena_estado(int (*ok)(const char *cena));
#endif
