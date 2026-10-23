// GPU time per frame, measured by the GPU itself (GL_EXT_disjoint_timer_query).
//
// Why: on Android TV the only numbers we had for a slow frame were CPU waits
// (`clr` and `swap` in [quadro]), and vsync quantizes those: a GPU frame of
// 17 ms and one of 30 ms both show up as "33 ms", and 45 / 40 / 30 fps say
// "over budget" without saying by how much. Measured on the TCL Smart TV Pro
// (Mali-G52, 06/10/2026): the home at 40 fps and the profile picker at 45 fps
// both looked identical in [quadro], and removing a 0.28-screen veil was the
// difference between 45 and 60 — invisible in every number we logged.
//
// One timer query brackets the whole frame (texture uploads, draw, the final
// swap excluded: that is the compositor). Results are read a few frames later,
// never blocking; a GPU_DISJOINT event throws the window away.
//
// Android only for now: the Mali/Adreno/PowerVR drivers there expose the
// extension on ES2 contexts; the webOS build is WebGL (no timer queries) and
// the .tpk is not measured here. Everywhere else every function is a no-op
// and gputempo_colher returns 0.
//
// OPT-IN since #318: off unless `adb shell setprop debug.nuvio.gputempo 1`
// before the app opens. A per-frame query in every user's build bought nothing
// and the Xiaomi MiTV-AFKR0 (Mali-G31) returned garbage while its screen
// flickered and froze; results over 1 s or across a disjoint are dropped.
#ifndef NV_GPUTEMPO_H
#define NV_GPUTEMPO_H

void gputempo_iniciar(void);         // after the GL context exists
void gputempo_quadro_inicio(void);   // before the first GL call of the frame
void gputempo_quadro_fim(void);      // right before SDL_GL_SwapWindow
// Window stats since the last call: mean, worst and the most recent frame, in
// ms. Returns how many frames were measured (0 = unsupported / nothing yet)
// and resets the window.
int    gputempo_colher(double *med, double *pior, double *ult);
double gputempo_p90(void);           // p90 of the current window, ms; call BEFORE gputempo_colher
double gputempo_ultimo(void);        // last measured frame, ms (0 if none)

#endif
