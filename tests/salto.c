// Avanco segurado: fluxo de teclas simulado a 100 ms e a 40 ms.
#include <stdio.h>
#include <math.h>
#include "../src/salto.h"

static int falhas;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

// Segura a tecla por `holdMs` com repeticao a cada `repMs` (repeat=0, como o
// firmware da TV); devolve segundos.
static float segurar(unsigned holdMs, unsigned repMs, float dur) {
  SaltoEst st = {0}; float tot = 0;
  for (unsigned t = 0; t <= holdMs; t += repMs)
    tot += salto_tecla(&st, t == 0, 0, 1000u + t, dur);
  return tot;
}

// `n` toques separados por `gapMs`, cada um uma rajada de uma tecla so.
static float toques(int n, unsigned gapMs, float dur) {
  SaltoEst st = {0}; float tot = 0;
  for (int i = 0; i < n; i++)
    tot += salto_tecla(&st, i == 0, 0, 1000u + i * gapMs, dur);
  return tot;
}

int main(void) {
  const float filme = 7200.0f;
  CHECK(toques(5, 200, filme) == 50.0f, "5 toques em 1 s = 50 s, deu %.0f", toques(5, 200, filme));
  CHECK(toques(2, 0, filme) == 20.0f, "dois toques no mesmo instante = 20 s");
  CHECK(toques(3, 160, filme) == 30.0f, "toques a 160 ms nao sao repeticao");
  { SaltoEst st = {0}; float t = salto_tecla(&st, 1, 0, 1000, filme);   // flag do SDL
    t += salto_tecla(&st, 0, 1, 1050, filme);
    CHECK(t == 10.0f, "repeat=1 dentro de 300 ms e ignorado"); }
  CHECK(segurar(0, 100, filme) == 10.0f, "toque unico deve ser 10 s");
  // Rampa (#340): toque = 10 s; mais segurado, passo cresce monotonicamente.
  CHECK(salto_passo(0, filme) == 10.0f && salto_passo(1499, filme) == 10.0f, "ate T1: 10 s");
  { float ant = 0; for (unsigned h = 0; h <= 12000; h += 100) {
      float p = salto_passo(h, filme); CHECK(p >= ant, "rampa monotona em %u", h); ant = p; } }
  CHECK(salto_passo(60000, filme) > salto_passo(60000, 1200.0f), "arquivo maior, passo maior");
  CHECK(salto_passo(60000, 100.0f) == 10.0f, "arquivo curtissimo: so 10 s");
  CHECK(salto_passo(60000, 0.0f) == 10.0f, "duracao 0: 10 s");
  // Atravessar o arquivo inteiro segurando: 8-12 s (20 min, 2 h, 3 h), com
  // repeticao de 100 ms e de 40 ms, ida e volta, e clamp em [0,dur].
  { float durs[] = {1200.0f, 7200.0f, 10800.0f};
    for (int i = 0; i < 3; i++) for (int r = 0; r < 2; r++) {
      unsigned rep = r ? 40 : 100; SaltoEst st = {0}; float pos = 0, dur = durs[i]; unsigned t = 0;
      for (; t < 30000 && pos < dur; t += rep) {
        pos += salto_tecla(&st, t == 0, 0, 1000u + t, dur);
        if (pos > dur) pos = dur;
      }
      printf("dur %5.0f s rep %3u ms: fim em %.1f s\n", dur, rep, t / 1000.0f);
      CHECK(pos == dur && t >= 8000 && t <= 12000, "travessia de %.0f s levou %u ms", dur, t);
      float volta = dur; st = (SaltoEst){0};
      for (t = 0; t < 30000 && volta > 0; t += rep) {
        volta -= salto_tecla(&st, t == 0, 0, 1000u + t, dur);
        if (volta < 0) volta = 0;
      }
      CHECK(volta == 0 && t >= 8000 && t <= 12000, "volta de %.0f s levou %u ms", dur, t);
    } }
  // #235: janela de confirmacao. Com Seekr 1 s, sem ele 420 ms; a rajada a 100 ms
  // nunca fecha o avanco no meio (a janela e maior que a repeticao), e uma
  // rajada com pausa de 600 ms continua UMA so decisao com Seekr, duas sem.
  CHECK(salto_fim_ms(1) == 1000u && salto_fim_ms(0) == 420u, "janela de fim");
  CHECK(salto_fim_ms(0) > SALTO_REP_MS * 2, "janela maior que a repeticao");
  { unsigned pausa = 600; int fim1 = pausa > salto_fim_ms(1), fim0 = pausa > salto_fim_ms(0);
    CHECK(!fim1 && fim0, "pausa de 600 ms: com Seekr segue o mesmo avanco"); }
  // Repeticoes rapidas (<300 ms) nao empilham passos: o backend recebe o total.
  { SaltoEst st = {0}; float p = 0; for (int i = 0; i < 4; i++) p += salto_tecla(&st, i == 0, 0, 1000u + i * 90u, filme);
    CHECK(p == 10.0f, "4 repeticoes em 270 ms valem um passo so, deu %.0f", p); }
  printf(falhas ? "salto: %d falha(s)\n" : "salto OK\n", falhas);
  return falhas != 0;
}
