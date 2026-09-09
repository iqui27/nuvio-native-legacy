#include "vistoep.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TETO. Uma pessoa que acompanha 100 series com 50 episodios cada tem 5000
// entradas; a 24 bytes sao 120 KB, que cabem com folga ate no heap FIXO de
// 256 MiB do alvo Tizen (tools/tizen.sh). O teto existe para uma resposta
// absurda nao virar consumo sem limite, e nao porque 8000 seja um numero
// especial. Estourado, o mapa PARA DE CRESCER e diz no log — o que ja entrou
// continua valendo, porque meio mapa e melhor que nenhum.
#define VE_MAX 8000

typedef struct { char id[16]; short temp, ep; unsigned char visto; } Marca;
static Marca *mapa;
static int n, cap, avisouTeto;

// So o id do titulo. "tt123:2:8" e "tt123" tem de casar: o primeiro e o formato
// que CatItem.imdb carrega num item de "Continuar assistindo", e quem chama
// daqui nem sempre sabe qual dos dois tem na mao.
static void base(const char *origem, char *dst, size_t tam) {
  size_t k = 0;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!origem) return;
  while (origem[k] && origem[k] != ':' && k + 1 < tam) { dst[k] = origem[k]; k++; }
  dst[k] = 0;
}

static int achar(const char *id, int t, int e) {
  int i;
  for (i = 0; i < n; i++)
    if (mapa[i].temp == t && mapa[i].ep == e && !strcmp(mapa[i].id, id)) return i;
  return -1;
}

void vistoep_definir(const char *imdb, int temporada, int episodio, int visto) {
  char id[16];
  int i;
  base(imdb, id, sizeof id);
  if (!id[0] || temporada < 0 || episodio < 1) return;
  i = achar(id, temporada, episodio);
  if (i >= 0) { mapa[i].visto = visto ? 1 : 0; return; }
  if (n >= VE_MAX) {
    if (!avisouTeto) {
      avisouTeto = 1;
      printf("[vistoep] teto de %d episodios; o mapa para de crescer\n", VE_MAX);
      fflush(stdout);
    }
    return;
  }
  if (n == cap) {
    int novo = cap ? cap * 2 : 256;
    Marca *m;
    if (novo > VE_MAX) novo = VE_MAX;
    m = (Marca *)realloc(mapa, (size_t)novo * sizeof *m);
    if (!m) return;
    mapa = m; cap = novo;
  }
  memset(&mapa[n], 0, sizeof mapa[n]);
  snprintf(mapa[n].id, sizeof mapa[n].id, "%s", id);
  mapa[n].temp = (short)temporada;
  mapa[n].ep = (short)episodio;
  mapa[n].visto = visto ? 1 : 0;
  n++;
}

int vistoep_estado(const char *imdb, int temporada, int episodio) {
  char id[16];
  int i;
  base(imdb, id, sizeof id);
  if (!id[0]) return -1;
  i = achar(id, temporada, episodio);
  return i < 0 ? -1 : (int)mapa[i].visto;
}

int vistoep_contar(const char *imdb) {
  char id[16];
  int i, k = 0;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  for (i = 0; i < n; i++) if (mapa[i].visto && !strcmp(mapa[i].id, id)) k++;
  return k;
}

int vistoep_conhecido(const char *imdb) {
  char id[16];
  int i;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  for (i = 0; i < n; i++) if (!strcmp(mapa[i].id, id)) return 1;
  return 0;
}

int vistoep_marcar_lote(const char *imdb, const VistoPar *pares, int qtd, int visto) {
  int i, mudou = 0;
  if (!pares) return 0;
  for (i = 0; i < qtd; i++) {
    if (vistoep_estado(imdb, pares[i].temporada, pares[i].episodio) == (visto ? 1 : 0))
      continue;
    vistoep_definir(imdb, pares[i].temporada, pares[i].episodio, visto);
    mudou++;
  }
  return mudou;
}

// ORDEM DE EPISODIO E (temporada, numero), nesta ordem — nao o numero sozinho.
// A temporada 0 dos especiais fica ANTES da 1, que e onde o Trakt tambem a
// coloca; marcar "ate aqui" no episodio 3 da temporada 2 nao pode arrastar a
// temporada 3 so porque o numero dela e menor.
static int antesOuIgual(int t, int e, int tAlvo, int eAlvo) {
  if (t != tAlvo) return t < tAlvo;
  return e <= eAlvo;
}

int vistoep_ate_aqui(const char *imdb, int temporada, int episodio,
                     VistoPar *saida, int max) {
  char id[16];
  int i, k = 0;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  // `saida` NULO E MODO CONTAGEM, e `max` e ignorado nele. A tela precisa do
  // NUMERO antes de decidir se cabe pedir a acao ("Ate aqui (7 episodios)"), e
  // sem isto ela teria de alocar um vetor so para descobrir o tamanho — ou,
  // pior, passar max=0 e receber 0 sempre, que foi o primeiro erro aqui.
  for (i = 0; i < n; i++) {
    if (strcmp(mapa[i].id, id)) continue;
    if (!antesOuIgual(mapa[i].temp, mapa[i].ep, temporada, episodio)) continue;
    if (saida) {
      if (k >= max) break;
      saida[k].temporada = mapa[i].temp;
      saida[k].episodio = mapa[i].ep;
    }
    k++;
  }
  return k;
}

int vistoep_temporada(const char *imdb, int temporada, VistoPar *saida, int max) {
  char id[16];
  int i, k = 0;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  for (i = 0; i < n; i++) {                 // saida nula = contagem, ver acima
    if (mapa[i].temp != temporada || strcmp(mapa[i].id, id)) continue;
    if (saida) {
      if (k >= max) break;
      saida[k].temporada = mapa[i].temp;
      saida[k].episodio = mapa[i].ep;
    }
    k++;
  }
  return k;
}

int vistoep_n(void) { return n; }

void vistoep_esquecer(void) {
  free(mapa); mapa = NULL; n = 0; cap = 0; avisouTeto = 0;
}

// ------------------------------------------------------------------- Trakt

// /shows/<id>/progress/watched:
//   { aired, completed, seasons: [ { number, aired, completed,
//       episodes: [ { number, completed, last_watched_at } ] } ] }
//
// AQUI `completed` E EXPLICITO, e por isso este leitor escreve 0 TAMBEM. E a
// diferenca com um mapa montado de "o que foi assistido": ali a ausencia de um
// episodio nao prova nada, aqui a resposta enumera a serie inteira e diz sim ou
// nao para cada linha. Depois desta leitura, vistoep_estado so devolve -1 para
// serie que nunca foi consultada — e a tela pode confiar no 0.
int vistoep_ler_progresso(const char *imdb, const char *json) {
  const char *temps;
  const char *fim;
  int total = 0;
  if (!imdb || !imdb[0] || !json) return -1;
  fim = json + strlen(json);
  temps = js_array(json, fim, "seasons");
  if (!temps) { printf("[vistoep] %s: resposta sem \"seasons\"\n", imdb); fflush(stdout); return -1; }
  for (; temps && *temps == '{'; temps = js_prox(js_fim(temps))) {
    const char *ft = js_fim(temps), *eps;
    int nt = (int)js_num(temps, ft, "number", -1.0);
    if (!ft || nt < 0) break;
    eps = js_array(temps, ft, "episodes");
    for (; eps && *eps == '{'; eps = js_prox(js_fim(eps))) {
      const char *fe = js_fim(eps);
      int ne, feito;
      if (!fe) break;
      ne = (int)js_num(eps, fe, "number", -1.0);
      // js_bool nao existe nesta camada; `completed` e true/false cru.
      { const char *c = strstr(eps, "\"completed\"");
        feito = (c && c < fe && strstr(c, "true") && strstr(c, "true") < fe &&
                 (size_t)(strstr(c, "true") - c) < 16) ? 1 : 0; }
      if (ne >= 1) { vistoep_definir(imdb, nt, ne, feito); total++; }
      if (fe >= ft) break;
    }
    if (ft >= fim) break;
  }
  printf("[vistoep] %s: %d episodios no mapa (%d vistos)\n",
         imdb, total, vistoep_contar(imdb));
  fflush(stdout);
  return total;
}
