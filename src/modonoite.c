// Modo noite / clareza de dialogo: ver modonoite.h.
#include "modonoite.h"
#include "audioescolha.h"
#include <math.h>
#include <string.h>

static int simulada = -1, armado;

int modonoite_capacidade(void) {
  if (simulada >= 0) return simulada;
#ifdef NV_ANDROID
  return MN_CAP_PROCESSA;
#else
  return MN_CAP_ESTEREO;
#endif
}
void modonoite_simular_capacidade(int c) { simulada = c == MN_CAP_ESTEREO || c == MN_CAP_PROCESSA ? c : -1; }

// Suave: 2:1 acima de -24 dBFS, +6 dB embaixo -> o pico de 0 dBFS sai a -6.
// Forte: 4:1 acima de -32 dBFS, +12 dB embaixo -> o pico de 0 dBFS sai a -12.
// O teto do sinal comprimido fica abaixo do joelho do limitador (-1 dBFS): em
// regime ele nao age; so segura o estalo do ataque.
MnParam modonoite_param(int nivel) {
  static const MnParam P[MN_N] = {
    { 0, 1, 0, 0, 0, 0, 5, 300 },
    { -24, 2, 6, 3, -3, -3, 5, 300 },
    { -32, 4, 12, 6, -6, -9, 5, 300 },
  };
  return P[nivel > MN_DESLIGADO && nivel < MN_N ? nivel : MN_DESLIGADO];
}

double modonoite_ganho_db(int nivel, double entradaDb) {
  MnParam p = modonoite_param(nivel);
  if (entradaDb <= p.limiarDb) return p.compensaDb;
  return p.compensaDb - (entradaDb - p.limiarDb) * (1.0 - 1.0 / p.razao);
}

int modonoite_canal(int canais, int i) {
  if (i < 0 || i >= canais || canais < 3 || canais == 4 || canais > MN_CANAIS_MAX) return MN_FRENTE;
  if (i < 2) return MN_FRENTE;
  if (i == 2) return MN_CENTRO;
  if (canais >= 6 && i == 3) return MN_LFE;
  return MN_SURROUND;
}

static float linear(float db) { return powf(10.0f, db / 20.0f); }

int modonoite_dsp_iniciar(MnDsp *d, int nivel, int canais, int taxaHz) {
  int i;
  memset(d, 0, sizeof *d);
  if (nivel <= MN_DESLIGADO || nivel >= MN_N || canais < 1 || canais > MN_CANAIS_MAX || taxaHz < 8000) return 0;
  d->nivel = nivel; d->canais = canais; d->p = modonoite_param(nivel);
  for (i = 0; i < canais; i++) {
    int papel = modonoite_canal(canais, i);
    d->ganhoCanal[i] = linear(papel == MN_CENTRO ? d->p.centroDb : papel == MN_LFE ? d->p.lfeDb
                              : papel == MN_SURROUND ? d->p.surroundDb : 0.0f);
  }
  d->ataque = 1.0f - expf(-1000.0f / (d->p.ataqueMs * (float)taxaHz));
  d->solta  = 1.0f - expf(-1000.0f / (d->p.soltaMs * (float)taxaHz));
  return 1;
}

// O mesmo limitador do reforco de volume (GanhoMath.kt): linear ate -1 dBFS,
// tanh dali ate o fundo de escala, nunca alem.
#define MN_JOELHO 0.8912509f
static float limitar(float y) {
  float a = fabsf(y), faixa = 1.0f - MN_JOELHO, c;
  if (a <= MN_JOELHO) return y;
  c = MN_JOELHO + faixa * tanhf((a - MN_JOELHO) / faixa);
  return y < 0 ? -c : c;
}

void modonoite_dsp(MnDsp *d, float *pcm, int quadros) {
  int q, c, n = d->canais;
  if (d->nivel <= MN_DESLIGADO || !pcm) return;
  for (q = 0; q < quadros; q++, pcm += n) {
    float pico = 0, g, db;
    for (c = 0; c < n; c++) {
      float a;
      pcm[c] *= d->ganhoCanal[c];
      a = fabsf(pcm[c]);
      if (a > pico) pico = a;
    }
    d->env += (pico - d->env) * (pico > d->env ? d->ataque : d->solta);
    db = 20.0f * log10f(d->env > 1e-6f ? d->env : 1e-6f);
    g = linear((float)modonoite_ganho_db(d->nivel, db));
    for (c = 0; c < n; c++) pcm[c] = limitar(pcm[c] * g);
  }
}

void modonoite_armar(int nivel) {
  armado = nivel > MN_DESLIGADO && nivel < MN_N ? nivel : MN_DESLIGADO;
  audioesc_preferir_estereo(armado != MN_DESLIGADO && modonoite_capacidade() == MN_CAP_ESTEREO);
  modonoite_backend_nivel(modonoite_capacidade() == MN_CAP_PROCESSA ? armado : MN_DESLIGADO);
}
int modonoite_nivel(void) { return armado; }

#ifndef NV_ANDROID
void modonoite_backend_nivel(int nivel) { (void)nivel; }
#endif
