// A ordem do audio automatico: ver audioescolha.h. Codigo puro mais uma
// lembranca da sessao; o disco e de audiomemoria.c, ligado por app.c
// (audioesc_definir_memoria). Sem rede, sem tela.
#include "audioescolha.h"
#include "audioinfo.h"
#include "linguas.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static char chaveAtual[96];
static AudioLembrada lembrada;
static int temLembrada;
static char fonteBinge[sizeof lembrada.binge];
static const AudioLembrada *(*memLer)(const char *);
static void (*memGravar)(const char *, const AudioLembrada *);

void audioesc_fonte_binge(const char *binge) {
  snprintf(fonteBinge, sizeof fonteBinge, "%s", binge ? binge : "");
}
void audioesc_definir_memoria(const AudioLembrada *(*ler)(const char *),
                              void (*gravar)(const char *, const AudioLembrada *)) {
  memLer = ler; memGravar = gravar;
}
static const char *bingeAtual(void) { return fonteBinge; }

const char *audioesc_origem_nome(int o) {
  switch (o) {
    case AUDIOESC_LEMBRADA: return "picked by hand for this title";
    case AUDIOESC_PREF1:    return "preferred language";
    case AUDIOESC_PREF2:    return "second language";
    case AUDIOESC_ORIGINAL: return "original language";
    default:                return "no rule";
  }
}

int audioesc_dts_truehd(const char *codec, const char *rotulo) {
  char s[128];
  size_t i;
  snprintf(s, sizeof s, " %s %s ", codec ? codec : "", rotulo ? rotulo : "");
  for (i = 0; s[i]; i++) s[i] = (char)tolower((unsigned char)s[i]);
  return strstr(s, "dts") || strstr(s, " dca") || strstr(s, "truehd") || strstr(s, "true-hd") ||
         strstr(s, "a_mlp") || strstr(s, " mlp");
}

void audioesc_titulo(const char *chave) {
  const char *c = chave ? chave : "";
  if (!strcmp(c, chaveAtual)) return;
  if (temLembrada) {
    printf("[audio] outro titulo: a faixa escolhida a mao no anterior nao vale aqui\n");
    fflush(stdout);
  }
  snprintf(chaveAtual, sizeof chaveAtual, "%s", c);
  temLembrada = 0;
  memset(&lembrada, 0, sizeof lembrada);
  // A ESCOLHA GRAVADA DESTE TITULO (o anime aberto amanha, depois de fechar o
  // app ou de passar por outro titulo): a mesma ordem, primeiro passo.
  if (c[0] && memLer) {
    const AudioLembrada *m = memLer(c);
    if (m) {
      lembrada = *m;
      temLembrada = 1;
      printf("[audio] escolha a mao gravada deste titulo: %s (%s)%s\n",
             lembrada.idioma[0] ? lembrada.idioma : "?", lembrada.rotulo,
             lembrada.binge[0] ? ", com bingeGroup" : "");
      fflush(stdout);
    }
  }
}

void audioesc_soltar(void) {
  chaveAtual[0] = 0;
  temLembrada = 0;
  memset(&lembrada, 0, sizeof lembrada);
}

void audioesc_lembrar_faixa(const char *idioma, const char *codec, const char *rotulo, int canais,
                            int numero) {
  memset(&lembrada, 0, sizeof lembrada);
  snprintf(lembrada.idioma, sizeof lembrada.idioma, "%s", idioma ? idioma : "");
  snprintf(lembrada.codec, sizeof lembrada.codec, "%s", codec ? codec : "");
  snprintf(lembrada.rotulo, sizeof lembrada.rotulo, "%s", rotulo ? rotulo : "");
  lembrada.canais = canais;
  lembrada.numero = numero;
  // Sem numero conhecido nao ha faixa exata: o bingeGroup nao serviria.
  if (numero >= 0) snprintf(lembrada.binge, sizeof lembrada.binge, "%s", bingeAtual());
  temLembrada = 1;
  printf("[audio] escolha a mao lembrada neste titulo: %s (%s)\n",
         lembrada.idioma[0] ? lembrada.idioma : "?", lembrada.rotulo);
  fflush(stdout);
  if (chaveAtual[0] && memGravar) memGravar(chaveAtual, &lembrada);
}
void audioesc_lembrar(const char *idioma, const char *codec, const char *rotulo, int canais) {
  audioesc_lembrar_faixa(idioma, codec, rotulo, canais, -1);
}
int audioesc_tem_lembrada(void) { return temLembrada; }
const AudioLembrada *audioesc_lembrada(void) { return temLembrada ? &lembrada : NULL; }

static int mesmoCodec(const char *a, const char *b) {
  const char *x = audioinfo_codec(a ? a : ""), *y = audioinfo_codec(b ? b : "");
  return x[0] && y[0] && !strcmp(x, y);
}

// Quanto a candidata parece a lembrada: 0 nao e ela.
static int parecenca(const AudioCand *c, const AudioLembrada *m) {
  int p = 0, rot = c->rotulo && m->rotulo[0] && !strcmp(c->rotulo, m->rotulo);
  int cod = mesmoCodec(c->codec, m->codec), ch = c->canais > 0 && c->canais == m->canais;
  if (m->idioma[0]) {
    if (!c->idioma || !c->idioma[0]) return rot ? 4 : 0;
    if (!ling_casa(c->idioma, m->idioma) || !ling_casa(m->idioma, c->idioma)) return 0;
    p = 1;
  } else if (!rot && !(cod && ch)) return 0;
  return p + (rot ? 4 : 0) + (cod ? 2 : 0) + (ch ? 1 : 0);
}

// Num idioma: a primeira que nao e DTS/TrueHD; senao a primeira DTS/TrueHD.
static int noIdioma(const AudioCand *c, int n, const char *pref) {
  int i, dts = -1;
  for (i = 0; i < n; i++) {
    if (!c[i].pode || !c[i].idioma || !c[i].idioma[0] || !ling_casa(c[i].idioma, pref)) continue;
    if (!c[i].dts) return i;
    if (dts < 0) dts = i;
  }
  return dts;
}

// A MESMA RELEASE: o bingeGroup e a declaracao do addon de que estes episodios
// saem do mesmo lugar, com o mesmo arranjo de faixas. Ali a faixa de mesmo
// numero e a que a pessoa escolheu, mesmo quando duas tem o mesmo idioma e
// rotulo (dublagem e comentario, "Japanese 2.0" duas vezes). O idioma ainda
// confere quando os dois lados tem etiqueta: um arranjo que mudou no meio da
// temporada cai para a parecenca, nunca para a faixa errada.
static int faixaExata(const AudioCand *c, int n, const AudioLembrada *m, const char *binge) {
  int i;
  if (!m || m->numero < 0 || !m->binge[0] || !binge || strcmp(binge, m->binge)) return -1;
  for (i = 0; i < n; i++) {
    if (!c[i].pode || c[i].numero != m->numero) continue;
    if (m->idioma[0] && c[i].idioma && c[i].idioma[0] &&
        (!ling_casa(c[i].idioma, m->idioma) || !ling_casa(m->idioma, c[i].idioma))) return -1;
    return i;
  }
  return -1;
}

int audioesc_escolher_com(const AudioCand *c, int n, const AudioLembrada *m,
                          const char *pref1, const char *pref2, const char *original, int *origem) {
  return audioesc_escolher_binge(c, n, m, NULL, pref1, pref2, original, origem);
}

int audioesc_escolher_binge(const AudioCand *c, int n, const AudioLembrada *m, const char *binge,
                            const char *pref1, const char *pref2, const char *original, int *origem) {
  const char *prefs[3] = { pref1, pref2, original };
  static const int ORIG[3] = { AUDIOESC_PREF1, AUDIOESC_PREF2, AUDIOESC_ORIGINAL };
  int i, k, j;
  if (origem) *origem = AUDIOESC_NADA;
  if (!c || n < 1) return -1;
  if ((i = faixaExata(c, n, m, binge)) >= 0) {
    if (origem) *origem = AUDIOESC_LEMBRADA;
    return i;
  }
  if (m) {
    int melhor = -1, nota = 0;
    for (i = 0; i < n; i++) {
      int p = c[i].pode ? parecenca(&c[i], m) : 0;
      if (p > nota) { nota = p; melhor = i; }
    }
    if (melhor >= 0) { if (origem) *origem = AUDIOESC_LEMBRADA; return melhor; }
  }
  for (k = 0; k < 3; k++) {
    const char *p = prefs[k];
    int repetida = 0;
    if (!p || !p[0] || !strcmp(p, "none")) continue;
    for (j = 0; j < k; j++) if (prefs[j] && prefs[j][0] && ling_casa(p, prefs[j]) && ling_casa(prefs[j], p)) repetida = 1;
    if (repetida) continue;
    if ((i = noIdioma(c, n, p)) >= 0) { if (origem) *origem = ORIG[k]; return i; }
  }
  return -1;
}

int audioesc_escolher(const AudioCand *c, int n, int *origem) {
  return audioesc_escolher_binge(c, n, temLembrada ? &lembrada : NULL, bingeAtual(), ling_audio(),
                                 ling_audio2(), ling_original(), origem);
}
