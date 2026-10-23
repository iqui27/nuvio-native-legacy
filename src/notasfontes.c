#include "notasfontes.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  const char *nome;
  int   escala;      // maximo da escala nativa
  NfGrupo grupo;
  int   pos;         // posicao na linha do titulo
  int   prio;        // 0 = ultima a sair num aperto
  int   padrao;      // aparece na linha de fabrica
  int   mdb;         // precisa da chave do MDBList
} Fonte;

// Indexada por ExFonte. A ORDEM DA LINHA (pos) e a de leitura; a PRIORIDADE
// (prio) e a de sobrevivencia — sao duas listas diferentes de proposito.
static const Fonte F[EX_NFONTES] = {
  /* EX_TRAKT      */ { "Trakt",                 100, NF_PUBLICO,  5, 2, 1, 0 },
  /* EX_IMDB       */ { "IMDb",                   10, NF_PUBLICO,  0, 0, 1, 0 },
  /* EX_TMDB       */ { "TMDB",                  100, NF_PUBLICO,  6, 5, 0, 1 },
  /* EX_TOMATOES   */ { "Rotten Tomatoes",       100, NF_CRITICA,  1, 1, 1, 1 },
  /* EX_AUDIENCE   */ { "Popcornmeter",          100, NF_PUBLICO,  2, 4, 0, 1 },
  /* EX_METACRITIC */ { "Metacritic",            100, NF_CRITICA,  3, 3, 0, 1 },
  /* EX_LETTERBOXD */ { "Letterboxd",              5, NF_PUBLICO,  7, 6, 0, 1 },
  /* EX_METAUSER   */ { "Metacritic",             10, NF_PUBLICO,  4, 8, 0, 1 },
  /* EX_MAL        */ { "MyAnimeList",            10, NF_PUBLICO,  8, 7, 0, 1 },
  /* EX_EBERT      */ { "Roger Ebert",             4, NF_CRITICA,  9, 9, 0, 1 },
  /* EX_MDBSCORE   */ { "MDBList",                100, NF_AGREGADA,10,10, 0, 1 },
};

static int valida(int f) { return f >= 0 && f < EX_NFONTES; }

int nf_posicao(int f)       { return valida(f) ? F[f].pos : 99; }
int nf_prioridade(int f)    { return valida(f) ? F[f].prio : 99; }
int nf_padrao_titulo(int f) { return valida(f) ? F[f].padrao : 0; }
int nf_precisa_mdblist(int f) { return valida(f) ? F[f].mdb : 0; }
NfGrupo nf_grupo(int f)     { return valida(f) ? F[f].grupo : NF_AGREGADA; }
const char *nf_nome(int f)  { return valida(f) ? F[f].nome : ""; }
int nf_escala_max(int f)    { return valida(f) ? F[f].escala : 100; }

int nf_na_posicao(int pos) {
  int f;
  for (f = 0; f < EX_NFONTES; f++) if (F[f].pos == pos) return f;
  return -1;
}

int nf_na_posicao_ord(int pos, int letterFirst) {
  int pl = F[EX_LETTERBOXD].pos;
  if (!letterFirst) return nf_na_posicao(pos);
  if (pos == 0) return EX_LETTERBOXD;
  return nf_na_posicao(pos <= pl ? pos - 1 : pos);
}

int nf_prioridade_ord(int f, int letterFirst) {
  (void)letterFirst;
  return f == EX_LETTERBOXD ? -1 : nf_prioridade(f);
}

// Decimos da nota crua. O IMDb e 0..10, mas o MDBList as vezes manda 0..100
// (80 = 8,0): sem isto vira "80%" / "80.0".
int nf_cru(int f, double v) {
  int n;
  if (f == EX_IMDB && v > 10.0) v /= 10.0;
  n = (int)(v * 10.0 + 0.5);
  return n < 0 ? 0 : n > 1000 ? 1000 : n;
}

int nf_norm100(int f, int cru) {
  int esc, n;
  if (!valida(f) || cru <= 0) return 0;
  esc = F[f].escala;
  // Fonte que ja e 0..100 no proprio site: o cru/10 e a resposta.
  if (esc == 100) return (cru + 5) / 10 > 100 ? 100 : (cru + 5) / 10;
  // Acima da escala (mais 5% de folga para arredondamento) = a api mandou em
  // porcentagem. Compara em decimos para nao passar por float.
  if (cru > esc * 10 + esc / 2) {
    n = (cru + 5) / 10;
    return n > 100 ? 100 : n;
  }
  n = (cru * 100 + esc * 5) / (esc * 10);    // cru/10/esc*100, arredondado
  return n > 100 ? 100 : n;
}

void nf_texto(int f, int cru, int virgula, int longo, char *dst, size_t cap) {
  int esc, dec;
  if (!cap) return;
  dst[0] = 0;
  if (!valida(f) || cru <= 0) return;
  esc = F[f].escala;
  // Fora da escala nativa a api mandou porcentagem: escreve como tal, para o
  // texto nao contradizer a celula pintada ao lado dele.
  if (esc != 100 && cru > esc * 10 + esc / 2) { snprintf(dst, cap, "%d%%", (cru + 5) / 10); return; }
  if (f == EX_METACRITIC || f == EX_MDBSCORE) {
    snprintf(dst, cap, "%d", (cru + 5) / 10);
    return;
  }
  if (esc == 100) { snprintf(dst, cap, "%d%%", (cru + 5) / 10); return; }
  dec = cru;                      // decimos: 78 = 7,8
  snprintf(dst, cap, virgula ? "%d,%d" : "%d.%d", dec / 10, dec % 10);
  if (longo && (f == EX_LETTERBOXD || f == EX_EBERT)) {
    size_t n = strlen(dst);
    snprintf(dst + n, cap - n, "/%d", esc);
  }
}

// --- cores -------------------------------------------------------------------
// Dez paradas da viridis (matplotlib), interpoladas em linha reta no RGB: entre
// paradas tao proximas o erro e menor que o da quantizacao de 8 bits.
static const float VIR[10][3] = {
  { 0.267f, 0.005f, 0.329f }, { 0.283f, 0.141f, 0.458f },
  { 0.254f, 0.265f, 0.530f }, { 0.207f, 0.372f, 0.553f },
  { 0.164f, 0.471f, 0.558f }, { 0.128f, 0.567f, 0.551f },
  { 0.135f, 0.659f, 0.518f }, { 0.267f, 0.749f, 0.441f },
  { 0.478f, 0.821f, 0.318f }, { 0.993f, 0.906f, 0.144f },
};

void nf_viridis(float t, float *r, float *g, float *b) {
  float x, k; int i;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  x = t * 9.0f;
  i = (int)x;
  if (i > 8) i = 8;
  k = x - (float)i;
  *r = VIR[i][0] + (VIR[i + 1][0] - VIR[i][0]) * k;
  *g = VIR[i][1] + (VIR[i + 1][1] - VIR[i][1]) * k;
  *b = VIR[i][2] + (VIR[i + 1][2] - VIR[i][2]) * k;
}

void nf_cor_nota(int n100, int piso, float *r, float *g, float *b, float *tinta) {
  float t, lum;
  if (piso < 0) piso = 0;
  if (piso > 90) piso = 90;
  t = (float)(n100 - piso) / (float)(100 - piso);
  nf_viridis(t, r, g, b);
  // Luminancia relativa (pesos de Rec.709 sobre o valor ja em gama, que e o que
  // o olho compara aqui): escuro no fim claro da rampa, claro no resto.
  lum = 0.2126f * *r + 0.7152f * *g + 0.0722f * *b;
  *tinta = lum > 0.52f ? 0.0f : 1.0f;
}

void nf_cor_metacritic(int s, float *r, float *g, float *b) {
  if (s >= 61)      { *r = 0.400f; *g = 0.800f; *b = 0.200f; }   // #66cc33
  else if (s >= 40) { *r = 1.000f; *g = 0.800f; *b = 0.200f; }   // #ffcc33
  else              { *r = 1.000f; *g = 0.000f; *b = 0.000f; }   // #ff0000
}

int nf_cor_episodio(int dec, float *r, float *g, float *b) {
  float t;
  if (dec <= 0) return 0;
  t = (float)(dec - 50) / 50.0f;
  nf_viridis(t, r, g, b);
  return 1;
}

// --- resumo ------------------------------------------------------------------
void nf_resumo(const int *fontes, const int *norm, int n, NfResumo *o) {
  int i, soma = 0, sc = 0, nc = 0, sp = 0, np = 0;
  memset(o, 0, sizeof *o);
  o->criticos = o->publico = -1;
  o->fonteMin = o->fonteMax = -1;
  o->min = 101; o->max = -1;
  for (i = 0; i < n; i++) {
    NfGrupo g = nf_grupo(fontes[i]);
    if (norm[i] <= 0) continue;
    // A nota agregada do MDBList ja e uma media das outras: entrar na conta
    // contaria a mesma opiniao duas vezes.
    if (g == NF_AGREGADA) continue;
    o->n++;
    soma += norm[i];
    if (norm[i] < o->min) { o->min = norm[i]; o->fonteMin = fontes[i]; }
    if (norm[i] > o->max) { o->max = norm[i]; o->fonteMax = fontes[i]; }
    if (g == NF_CRITICA) { sc += norm[i]; nc++; } else { sp += norm[i]; np++; }
  }
  if (!o->n) { o->min = o->max = 0; return; }
  o->media = (soma * 2 + o->n) / (o->n * 2);
  if (nc) o->criticos = (sc * 2 + nc) / (nc * 2);
  if (np) o->publico  = (sp * 2 + np) / (np * 2);
  if (nc && np) { o->temDiff = 1; o->diff = o->publico - o->criticos; }
}

// --- encaixe -----------------------------------------------------------------
int nf_encaixar(const float *larg, const int *prio, int n, float disp,
                unsigned char *manter) {
  float total = 0.0f;
  int i, quantos = 0;
  for (i = 0; i < n; i++) { manter[i] = 1; total += larg[i]; quantos++; }
  while (quantos > 0 && total > disp) {
    int pior = -1;
    for (i = 0; i < n; i++)
      if (manter[i] && (pior < 0 || prio[i] >= prio[pior])) pior = i;
    manter[pior] = 0;
    total -= larg[pior];
    quantos--;
  }
  return quantos;
}

// --- grade de episodios --------------------------------------------------------
int nf_grade_celula(int nt, int maxEps, float dispW, float dispH,
                    float *cw, float *ch) {
  float w, h;
  if (nt < 1 || maxEps < 1) return 0;
  // Largura: divide a faixa (com um vao de 4 px entre celulas), teto de 64 —
  // uma temporada de 6 episodios nao vira seis tijolos.
  w = (dispW - 4.0f * (float)(maxEps - 1)) / (float)maxEps;
  if (w > 64.0f) w = 64.0f;
  // Altura: cabe as temporadas na faixa, teto de 34 (legivel a distancia).
  h = (dispH - 4.0f * (float)(nt - 1)) / (float)nt;
  if (h > 34.0f) h = 34.0f;
  if (w < 10.0f || h < 8.0f) return 0;
  *cw = w; *ch = h;
  return 1;
}
