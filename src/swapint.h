// INTERVALO REAL ENTRE SWAPS. O laco assumia 60 Hz (16,7 ms) em dois lugares
// por convencao: o limite de jank (33 ms = 2 quadros de 60 Hz) e a leitura do
// log. Numa TV travada em 30 Hz todo quadro normal virava "jank". Aqui se mede
// o tempo entre dois retornos de SDL_GL_SwapWindow — espera de vsync, compositor
// e driver entram na conta, o que o relogio de fase por CPU nao ve.
//
// Ideia do heartbeat.rs do plx-native (intervalo real de swap); implementacao nossa.
#ifndef NV_SWAPINT_H
#define NV_SWAPINT_H

typedef struct {
  double ult;        // carimbo do ultimo swap (ms), 0 = nenhum
  double ema;        // periodo medido (ms), 0 = sem amostra
  double soma, pior; // janela de relatorio
  int    n;
  int    lentos;     // amostras lentas seguidas fora do periodo
} SwapInt;

// Acima disto o intervalo e suspensao/parada, nao o ritmo da tela.
#define SWAPINT_MAX_MS 250.0
// Quadros lentos seguidos que valem como mudanca de cadencia.
#define SWAPINT_TROCA_N 8

// Chamar logo depois de cada swap, com o relogio em ms.
static inline void swapint_marca(SwapInt *s, double agora_ms) {
  double d = agora_ms - s->ult;
  int tinha = s->ult > 0.0;
  s->ult = agora_ms;
  if (!tinha || d <= 0.0 || d > SWAPINT_MAX_MS) return;
  // Engasgo (> 1,5 periodo) conta como jank mas nao entra no periodo; so uma
  // sequencia de lentos seguidos e troca de ritmo da tela (60 -> 30 Hz).
  if (s->ema <= 0.0) s->ema = d;
  else if (d <= s->ema * 1.5) { s->ema += (d - s->ema) * 0.1; s->lentos = 0; }
  else if (++s->lentos >= SWAPINT_TROCA_N) { s->ema = d; s->lentos = 0; }
  s->soma += d; s->n++;
  if (d > s->pior) s->pior = d;
}

// Periodo medido; 16,7 enquanto nao ha amostra.
static inline double swapint_periodo(const SwapInt *s) { return s->ema > 0.0 ? s->ema : 16.7; }

// Quadro acima disto conta como jank: dois periodos, nunca menos que 33 ms
// (sem vsync o periodo e ~1 ms e o piso mantem o criterio de antes).
static inline double swapint_limite_jank(const SwapInt *s) {
  double l = swapint_periodo(s) * 2.0;
  return l < 33.0 ? 33.0 : l;
}

// Fecha a janela: media e pior intervalo; devolve quantas amostras, e zera.
static inline int swapint_colher(SwapInt *s, double *med, double *pior) {
  int n = s->n;
  *med = n ? s->soma / n : 0.0; *pior = s->pior;
  s->soma = s->pior = 0.0; s->n = 0;
  return n;
}
#endif
