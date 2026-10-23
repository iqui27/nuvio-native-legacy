// Avanco segurado e SUAVE: posicao que a barra, o tempo escrito e a miniatura do
// Seekr mostram, quadro a quadro (60 fps), com tecla segurada a 100 ms.
// Reproduz o laco do player: tecla -> salto_tecla; quadro -> salto_quadro +
// anim_mola2(posVis). -DANTES reproduz o 2.0.3 antes da correcao (passo inteiro
// a cada 300 ms, tempo e miniatura lendo a posicao crua).
#include <stdio.h>
#include <math.h>
#include "../src/anim.h"
#include "../src/salto.h"

static int falhas;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

#define BARRA_PX 1728.0f   // largura da barra no OSD (1920 - 2*96)
#define MOLA 18.0f         // PLR_BUSCA_MOLA

// Devolve o maior passo do tempo exibido em px/quadro e o menor passo na janela
// estavel (2 s..5 s segurando).
static void rodar(float dur, int dir, float *maxPx, float *minPx, float *mediaPx, float *fimSeg) {
  SaltoEst st = {0};
  float posSeg = dir > 0 ? 0.0f : dur, posVis = posSeg, v = 0.0f, ant = posSeg;
  const float dt = 1.0f / 60.0f;
  unsigned t0 = 1000u, hold = 6000u, proxTecla = 0;
  float soma = 0; int n = 0;
  *maxPx = 0; *minPx = 1e9f;
  for (unsigned q = 0; q * 1000u / 60u < hold + 1500u; q++) {
    unsigned ms = q * 1000u / 60u, agora = t0 + ms;
    if (ms <= hold && ms >= proxTecla) {
      float p = salto_tecla(&st, ms == 0, 0, agora, dur);
#ifdef ANTES
      posSeg += dir * p;
#else
      if (!st.rep) posSeg += dir * p;
#endif
      proxTecla = ms + 100u;
    }
#ifndef ANTES
    posSeg += salto_quadro(&st, dir, agora, dt, dur);
#endif
    posSeg = anim_clamp(posSeg, 0.0f, dur);
    posVis = anim_mola2(&v, posVis, posSeg, dt, MOLA);
#ifdef ANTES
    float exib = posSeg;   // tempo escrito e miniatura liam a posicao crua
#else
    float exib = posVis;
#endif
    float d = fabsf(exib - ant) / dur * BARRA_PX; ant = exib;
    if (ms > 100 && ms <= hold) {
      if (d > *maxPx) *maxPx = d;
      if (ms >= 4700 && ms <= 5900 && posSeg > 0 && posSeg < dur) {
        if (d < *minPx) *minPx = d; soma += d; n++;
      }
    }
    if (ms >= hold) { /* depois de soltar: so a mola assenta */ }
    *fimSeg = posSeg;
  }
  *mediaPx = n ? soma / n : 0;
}

int main(void) {
  const float durs[] = { 1200.0f, 7200.0f, 10800.0f };
  for (int i = 0; i < 3; i++) {
    float mx, mn, md, fim;
    rodar(durs[i], 1, &mx, &mn, &md, &fim);
    printf("dur %5.0f s: max %.2f px/quadro  min %.2f  media %.2f  fim %.0f s\n", durs[i], mx, mn, md, fim);
    if (md > 0.5f) {
      CHECK(mx <= 8.0f, "dur %.0f: salto de %.1f px num quadro (limite 8)", durs[i], mx);
      CHECK(mn >= 0.5f * md, "dur %.0f: quadro parado/lento (%.2f px vs media %.2f)", durs[i], mn, md);
    } else {
      CHECK(mx <= 8.0f, "dur %.0f: salto de %.1f px num quadro", durs[i], mx);
    }
  }
  // Velocidade do dono preservada: o que a rampa antiga (passo a cada 300 ms)
  // andava em 6 s segurando, o avanco continuo anda tambem (+-10%). Valores do
  // 2.0.3 antes da correcao, medidos com -DANTES: 645 / 3345 / 4965 s.
  { const float durs2[] = { 1200.0f, 7200.0f, 10800.0f }, ref[] = { 645.0f, 3345.0f, 4965.0f };
    for (int i = 0; i < 3; i++) {
      float mx, mn, md, fim; rodar(durs2[i], 1, &mx, &mn, &md, &fim);
      CHECK(fabsf(fim - ref[i]) <= 0.1f * ref[i], "dur %.0f: andou %.0f s, esperado ~%.0f", durs2[i], fim, ref[i]); } }
  // Toque unico = 10 s, sem deriva depois.
  { SaltoEst st = {0}; float p = salto_tecla(&st, 1, 0, 1000, 7200.0f);
    CHECK(p == 10.0f && !st.rep, "toque 10 s");
    for (unsigned ms = 0; ms < 2000; ms += 16)
      CHECK(salto_quadro(&st, 1, 1000 + ms, 0.016f, 7200.0f) == 0.0f, "toque nao deriva"); }
  printf(falhas ? "%d falha(s)\n" : "OK\n", falhas);
  return falhas ? 1 : 0;
}
