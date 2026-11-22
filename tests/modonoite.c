// Night mode / dialogue clarity (src/modonoite.c) and the stereo preference
// (audioesc_estereo). Pure code. Prints the measures of the DSP reference.
#include "modonoite.h"
#include "audioescolha.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define HZ 48000

static int backendNivel = -1;
#ifdef NV_ANDROID
void modonoite_backend_nivel(int nivel) { backendNivel = nivel; }
#endif

static double rms(const float *p, int quadros, int canais) {
  double s = 0; int i;
  for (i = 0; i < quadros * canais; i++) s += (double)p[i] * p[i];
  return sqrt(s / (quadros * canais));
}
static double db(double x) { return 20.0 * log10(x > 1e-9 ? x : 1e-9); }
// O que uma TV estereo ouve de um 5.1: Lo/Ro (ITU-R BS.775), sem LFE.
static double rmsDownmix(const float *p, int quadros, int canais) {
  double s = 0; int q;
  if (canais != 6) return rms(p, quadros, canais);
  for (q = 0; q < quadros; q++, p += 6) {
    double l = p[0] + 0.7071 * p[2] + 0.7071 * p[4], r = p[1] + 0.7071 * p[2] + 0.7071 * p[5];
    s += (l * l + r * r) / 2;
  }
  return sqrt(s / quadros);
}

static void parametros(void) {
  MnParam p = modonoite_param(MN_DESLIGADO), s = modonoite_param(MN_SUAVE), f = modonoite_param(MN_FORTE);
  double e;
  assert(p.razao == 1 && p.compensaDb == 0 && p.centroDb == 0 && p.surroundDb == 0 && p.lfeDb == 0);
  assert(modonoite_param(-1).compensaDb == 0 && modonoite_param(MN_N).compensaDb == 0);
  assert(s.razao < f.razao && s.compensaDb < f.compensaDb && s.centroDb < f.centroDb && f.lfeDb < s.lfeDb);
  for (e = -90; e <= 6; e += 0.5) {
    int n;
    assert(modonoite_ganho_db(MN_DESLIGADO, e) == 0);
    for (n = MN_SUAVE; n < MN_N; n++) {
      double sai = e + modonoite_ganho_db(n, e), antes = (e - 0.5) + modonoite_ganho_db(n, e - 0.5);
      assert(sai > antes);                       // monotonic: louder in never comes out quieter
      if (e <= 0) assert(sai <= -1.0);           // steady state stays under the limiter knee (-1 dBFS)
    }
  }
  // Baixo sobe o ganho inteiro; o pico de 0 dBFS desce.
  assert(modonoite_ganho_db(MN_SUAVE, -60) == 6 && modonoite_ganho_db(MN_FORTE, -60) == 12);
  assert(fabs(modonoite_ganho_db(MN_SUAVE, 0) + 6) < 1e-9 && fabs(modonoite_ganho_db(MN_FORTE, 0) + 12) < 1e-9);
}

static void canais(void) {
  static const int C51[6] = { MN_FRENTE, MN_FRENTE, MN_CENTRO, MN_LFE, MN_SURROUND, MN_SURROUND };
  int i;
  for (i = 0; i < 6; i++) assert(modonoite_canal(6, i) == C51[i]);
  for (i = 0; i < 2; i++) assert(modonoite_canal(2, i) == MN_FRENTE && modonoite_canal(1, 0) == MN_FRENTE);
  for (i = 0; i < 4; i++) assert(modonoite_canal(4, i) == MN_FRENTE);       // quad: no centre
  assert(modonoite_canal(3, 2) == MN_CENTRO && modonoite_canal(5, 3) == MN_SURROUND);
  assert(modonoite_canal(8, 3) == MN_LFE && modonoite_canal(8, 7) == MN_SURROUND);
  assert(modonoite_canal(6, -1) == MN_FRENTE && modonoite_canal(6, 6) == MN_FRENTE && modonoite_canal(9, 2) == MN_FRENTE);
}

// 2 s de dialogo baixo (central, -30 dBFS) e 2 s de explosao (todos os canais,
// -3 dBFS), em 5.1 ou estereo. Devolve os quadros.
static int cena(float *p, int canais) {
  int q, c, n = 4 * HZ;
  for (q = 0; q < n; q++) {
    double t = (double)q / HZ;
    int alto = q >= 2 * HZ;
    for (c = 0; c < canais; c++) {
      double a = 0;
      if (!alto) a = (canais == 6 ? c == 2 : 1) ? 0.0316 * sin(2 * M_PI * 300 * t) : 0;
      else a = 0.708 * sin(2 * M_PI * (c == 3 ? 50 : 90 + 37 * c) * t);
      p[q * canais + c] = (float)a;
    }
  }
  return n;
}

static void medir(int nivel, int canais, double *faixaAntes, double *faixaDepois) {
  float *p = malloc(sizeof(float) * 4 * HZ * canais), *o = malloc(sizeof(float) * 4 * HZ * canais);
  int n = cena(p, canais), i, meio = HZ;   // janelas de 1 s no fim de cada trecho (regime)
  double bAntes, aAntes, bDepois, aDepois, pico = 0;
  MnDsp d;
  memcpy(o, p, sizeof(float) * n * canais);
  assert(modonoite_dsp_iniciar(&d, nivel, canais, HZ));
  // Em blocos irregulares: o estado atravessa os buffers.
  for (i = 0; i < n; ) { int b = 1 + (i * 7) % 1023; if (i + b > n) b = n - i; modonoite_dsp(&d, o + i * canais, b); i += b; }
  for (i = 0; i < n * canais; i++) { assert(isfinite(o[i])); if (fabsf(o[i]) > pico) pico = fabsf(o[i]); }
  assert(pico <= 1.0);                                   // never beyond full scale
  bAntes = db(rmsDownmix(p + meio * canais, HZ, canais)); aAntes = db(rmsDownmix(p + 3 * meio * canais, HZ, canais));
  bDepois = db(rmsDownmix(o + meio * canais, HZ, canais)); aDepois = db(rmsDownmix(o + 3 * meio * canais, HZ, canais));
  *faixaAntes = aAntes - bAntes; *faixaDepois = aDepois - bDepois;
  printf("  %s %-5s dialogue %6.1f -> %6.1f dB, blast %6.1f -> %6.1f dB, gap %4.1f -> %4.1f dB, peak %.3f\n",
         canais == 6 ? "5.1   " : "stereo", nivel == MN_SUAVE ? "Leve" : "Forte", bAntes, bDepois, aAntes, aDepois,
         *faixaAntes, *faixaDepois, pico);
  assert(bDepois > bAntes + 3);                          // the quiet dialogue got louder
  assert(aDepois < aAntes - 3);                          // the blast got quieter
  free(p); free(o);
}

static void dsp(void) {
  MnDsp d;
  float q[6] = { 0.5f, -0.5f, 0.25f, 0.1f, 0, 0 }, copia[6];
  double a, s51, f51, s20, f20;
  assert(!modonoite_dsp_iniciar(&d, MN_DESLIGADO, 6, HZ) && !modonoite_dsp_iniciar(&d, MN_N, 6, HZ));
  assert(!modonoite_dsp_iniciar(&d, MN_FORTE, 0, HZ) && !modonoite_dsp_iniciar(&d, MN_FORTE, 9, HZ));
  assert(!modonoite_dsp_iniciar(&d, MN_FORTE, 2, 0));
  memcpy(copia, q, sizeof q);
  modonoite_dsp(&d, q, 1);                               // not started: untouched
  assert(!memcmp(copia, q, sizeof q));
  puts("night mode DSP reference, steady-state RMS of what a stereo TV plays (Lo/Ro downmix):");
  medir(MN_SUAVE, 6, &a, &s51); medir(MN_FORTE, 6, &a, &f51);
  assert(f51 < s51 && s51 < a - 8 && f51 < a - 18);      // 5.1: the gap closes by > 8 dB (Leve) and > 18 dB (Forte)
  medir(MN_SUAVE, 2, &a, &s20); medir(MN_FORTE, 2, &a, &f20);
  assert(f20 < s20 && s20 < a - 6 && f20 < a - 12);
}

// Vetores para o porte Kotlin (tests/modonoite_android.py compara com estes).
static void vetores(const char *arquivo) {
  FILE *f = fopen(arquivo, "w");
  int nivel, canais, q, c;
  assert(f);
  for (nivel = MN_SUAVE; nivel < MN_N; nivel++)
    for (canais = 1; canais <= 8; canais++) {
      MnDsp d;
      unsigned s = 12345u + (unsigned)(nivel * 100 + canais);
      assert(modonoite_dsp_iniciar(&d, nivel, canais, HZ));
      for (q = 0; q < 2000; q++) {
        float x[MN_CANAIS_MAX], y[MN_CANAIS_MAX];
        float amp = q < 600 ? 0.02f : q < 1200 ? 0.9f : 0.2f;
        for (c = 0; c < canais; c++) { s = s * 1664525u + 1013904223u; x[c] = amp * ((float)(s >> 8) / 8388608.0f - 1.0f); y[c] = x[c]; }
        modonoite_dsp(&d, y, 1);
        if (q % 50 == 0 || (q >= 600 && q < 620))
          for (c = 0; c < canais; c++) fprintf(f, "%d %d %d %d %.9g %.9g\n", nivel, canais, q, c, x[c], y[c]);
      }
    }
  fclose(f);
}

static AudioCand cand(const char *id, const char *rot, int ch, int dts, int pode) {
  AudioCand c = { id, "", rot, ch, dts, pode, -1 };
  return c;
}

static void estereo(void) {
  AudioCand c[5];
  int o;
  c[0] = cand("en", "English · E-AC-3 5.1", 6, 0, 1);
  c[1] = cand("en", "English · Commentary 2.0", 2, 0, 1);
  c[2] = cand("pt", "Português · AAC 2.0", 2, 0, 1);
  c[3] = cand("eng", "English · AAC 2.0", 2, 0, 1);
  c[4] = cand("pt-BR", "Português · AC-3 5.1", 6, 0, 1);
  assert(audioesc_estereo(c, 5, 0, AUDIOESC_PREF1) == 3);        // same language, not the commentary
  assert(audioesc_estereo(c, 5, 4, AUDIOESC_PREF2) == 2);        // pt-BR 5.1 -> pt 2.0
  assert(audioesc_estereo(c, 5, 2, AUDIOESC_PREF1) == 2);        // already stereo
  assert(audioesc_estereo(c, 5, 0, AUDIOESC_LEMBRADA) == 0);     // picked by hand: never overridden
  assert(audioesc_estereo(c, 5, -1, AUDIOESC_NADA) == -1);       // no rule decided: no guess
  assert(audioesc_estereo(c, 5, 9, AUDIOESC_PREF1) == 9 && audioesc_estereo(NULL, 0, 0, AUDIOESC_PREF1) == 0);
  c[3].pode = 0; assert(audioesc_estereo(c, 5, 0, AUDIOESC_PREF1) == 0);   // the TV can't play it
  c[3].pode = 1; c[3].dts = 1; assert(audioesc_estereo(c, 5, 0, AUDIOESC_PREF1) == 0);   // would need conversion
  c[3].dts = 0; c[3].canais = 0; assert(audioesc_estereo(c, 5, 0, AUDIOESC_PREF1) == 0); // unknown channels
  c[3].canais = 2; c[0].idioma = ""; assert(audioesc_estereo(c, 5, 0, AUDIOESC_PREF1) == 0);   // untagged: no guess
  c[0].idioma = "en"; c[3].rotulo = "English · Audio Description"; assert(audioesc_estereo(c, 5, 0, AUDIOESC_PREF1) == 0);
  c[3].rotulo = "English · AAC 2.0";

  // Dentro de audioesc_escolher: desligado nao muda nada; ligado troca e diz por que.
  audioesc_titulo("tt-noite");
  audioesc_preferir_estereo(0);
  assert(audioesc_escolher_com(c, 5, NULL, "en", "", "", &o) == 0 && o == AUDIOESC_PREF1);
  modonoite_simular_capacidade(MN_CAP_ESTEREO);
  modonoite_armar(MN_FORTE); assert(modonoite_nivel() == MN_FORTE);
  assert(audioesc_estereo(c, 5, audioesc_escolher_com(c, 5, NULL, "en", "", "", &o), o) == 3);
  modonoite_armar(MN_DESLIGADO); assert(modonoite_nivel() == MN_DESLIGADO);
  modonoite_armar(99); assert(modonoite_nivel() == MN_DESLIGADO);
  // Onde o app processa o PCM (Android), a faixa nao e trocada.
  modonoite_simular_capacidade(MN_CAP_PROCESSA);
  modonoite_armar(MN_SUAVE); assert(modonoite_nivel() == MN_SUAVE);
  modonoite_simular_capacidade(-1);
  modonoite_armar(MN_DESLIGADO);
  assert(modonoite_capacidade() == MN_CAP_ESTEREO || modonoite_capacidade() == MN_CAP_PROCESSA);
  (void)backendNivel;
}

int main(int argc, char **argv) {
  parametros(); canais(); dsp(); estereo();
  if (argc > 1) vetores(argv[1]);
  puts("modonoite: ok");
  return 0;
}
