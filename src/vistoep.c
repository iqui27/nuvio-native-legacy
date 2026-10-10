#include "vistoep.h"
#include "js.h"
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TETO. Uma pessoa que acompanha 100 series com 50 episodios cada tem 5000
// entradas; a 24 bytes sao 120 KB, que cabem com folga ate no heap FIXO de
// 256 MiB do alvo Tizen (tools/tizen.sh). O teto existe para uma resposta
// absurda nao virar consumo sem limite, e nao porque 8000 seja um numero
// especial. Estourado, o mapa PARA DE CRESCER e diz no log — o que ja entrou
// continua valendo, porque meio mapa e melhor que nenhum.
#define VE_LOTE VE_LOTE_MAX

typedef struct { char id[16]; short temp, ep; unsigned char visto; } Marca;
static Marca *mapa;
static int n, cap, avisouTeto;
// Sobe a cada mudanca de estado no mapa (vistoep_revisao).
static unsigned revisao;
// TRAVA DO MAPA. Escrevem nele o fio de extras (extras.c, parte 0, ao abrir a
// pagina de uma serie) e o fio principal (sync_passo, player, episodios,
// agenda, logout); le o desenho do detalhe. Sem ela o realloc de definir()
// num fio soltava o vetor que o outro estava lendo ou escrevendo — o ASAN
// pegou no Mac (heap-use-after-free em achar, lido pelo desenho do detalhe
// enquanto o fio de extras crescia o mapa). Toda funcao publica trava; as
// estaticas (achar, definir) supoem a trava tomada.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

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

static int definir(const char *imdb, int temporada, int episodio, int visto) {
  char id[16];
  int i;
  base(imdb, id, sizeof id);
  if (!id[0] || temporada < 0 || temporada > SHRT_MAX ||
      episodio < 1 || episodio > SHRT_MAX) return 0;
  i = achar(id, temporada, episodio);
  if (i >= 0) {
    if (mapa[i].visto != (visto ? 1 : 0)) revisao++;
    mapa[i].visto = visto ? 1 : 0;
    return 1;
  }
  if (n >= VE_MAX) {
    if (!avisouTeto) {
      avisouTeto = 1;
      printf("[vistoep] teto de %d episodios; o mapa para de crescer\n", VE_MAX);
      fflush(stdout);
    }
    return 0;
  }
  if (n == cap) {
    int novo = cap ? cap * 2 : 256;
    Marca *m;
    if (novo > VE_MAX) novo = VE_MAX;
    m = (Marca *)realloc(mapa, (size_t)novo * sizeof *m);
    if (!m) return 0;
    mapa = m; cap = novo;
  }
  memset(&mapa[n], 0, sizeof mapa[n]);
  snprintf(mapa[n].id, sizeof mapa[n].id, "%s", id);
  mapa[n].temp = (short)temporada;
  mapa[n].ep = (short)episodio;
  mapa[n].visto = visto ? 1 : 0;
  n++;
  revisao++;
  return 1;
}

// O JUIZ DAS DESMARCACOES (vistonao.c), injetado por app.c. Ponteiros e nao
// include: este arquivo entra sozinho em uma duzia de testes, e o juiz precisa
// de perfil, sessao e disco. Sem juiz ligado nada e barrado — o comportamento
// de antes. SEMPRE chamados FORA da trava do mapa: eles tem a trava deles e
// gravam em disco, e o desenho le o mapa a cada quadro.
static int  (*lapBarra)(const char *imdb, int temporada, int episodio, long long remotoMs);
static void (*lapGesto)(const char *imdb, const VistoPar *pares, int n, int visto);

void vistoep_lapides(int (*barra)(const char *, int, int, long long),
                     void (*gesto)(const char *, const VistoPar *, int, int)) {
  lapBarra = barra;
  lapGesto = gesto;
}

void vistoep_definir(const char *imdb, int temporada, int episodio, int visto) {
  pthread_mutex_lock(&trava);
  definir(imdb, temporada, episodio, visto);
  pthread_mutex_unlock(&trava);
  // MARCAR AQUI E A PESSOA (o player ao concluir o episodio): assistir de novo
  // desfaz a desmarcacao. O 0 daqui nao cria uma — quem desmarca e o gesto da
  // tela (vistoep_aplicar / vistoep_marcar_lote), e leitor de rede nao passa
  // por esta funcao (vistoep_fonte).
  if (visto && lapGesto && episodio >= 1 && episodio <= SHRT_MAX &&
      temporada >= 0 && temporada <= SHRT_MAX) {
    VistoPar par;
    par.temporada = (short)temporada;
    par.episodio = (short)episodio;
    lapGesto(imdb, &par, 1, 1);
  }
}

static int estado(const char *imdb, int temporada, int episodio) {
  char id[16];
  int i;
  base(imdb, id, sizeof id);
  if (!id[0]) return -1;
  i = achar(id, temporada, episodio);
  return i < 0 ? -1 : (int)mapa[i].visto;
}
int vistoep_estado(const char *imdb, int temporada, int episodio) {
  int r;
  pthread_mutex_lock(&trava);
  r = estado(imdb, temporada, episodio);
  pthread_mutex_unlock(&trava);
  return r;
}

// ------------------------------------------------------------------ fontes

int vistoep_fonte(const char *imdb, int temporada, int episodio, int visto,
                  long long remotoMs, VistoFonte *c) {
  int ok, juiz = 0;
  // DESMARCAR GANHA (vistonao.h). So o "visto" e julgado: fonte dizendo "nao
  // visto" concorda com a pessoa ou fala de episodio que ela nao tocou.
  if (visto && lapBarra) juiz = lapBarra(imdb, temporada, episodio, remotoMs);
  if (juiz > 0) visto = 0;
  pthread_mutex_lock(&trava);
  ok = definir(imdb, temporada, episodio, visto);
  pthread_mutex_unlock(&trava);
  if (!c) return ok;
  if (juiz > 0) {
    if (c->bloqueados < VE_FONTE_PARES) {
      c->par[c->bloqueados].temporada = (short)temporada;
      c->par[c->bloqueados].episodio = (short)episodio;
    }
    c->bloqueados++;
  } else if (ok && visto) {
    c->vistos++;
    if (juiz < 0) c->venceu++;
  }
  return ok;
}

void vistoep_fonte_log(const char *imdb, const char *fonte, const VistoFonte *c) {
  char quais[VE_FONTE_PARES * 10 + 8];
  size_t u = 0;
  int i, k;
  if (!imdb || !fonte || !c) return;
  quais[0] = 0;
  k = c->bloqueados < VE_FONTE_PARES ? c->bloqueados : VE_FONTE_PARES;
  for (i = 0; i < k && u + 12 < sizeof quais; i++)
    u += (size_t)snprintf(quais + u, sizeof quais - u, "%sT%dE%d", i ? " " : ": ",
                          c->par[i].temporada, c->par[i].episodio);
  if (c->bloqueados > k && u + 5 < sizeof quais) snprintf(quais + u, sizeof quais - u, " ...");
  printf("[vistoep] %s: %s +%d (bloqueados %d%s; remoto mais novo %d)\n", imdb, fonte,
         c->vistos, c->bloqueados, quais, c->venceu);
  fflush(stdout);
}

int vistoep_contar(const char *imdb) {
  char id[16];
  int i, k = 0;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (mapa[i].visto && !strcmp(mapa[i].id, id)) k++;
  pthread_mutex_unlock(&trava);
  return k;
}

int vistoep_total(const char *imdb) {
  char id[16];
  int i, k = 0;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (!strcmp(mapa[i].id, id)) k++;
  pthread_mutex_unlock(&trava);
  return k;
}

int vistoep_conhecido(const char *imdb) {
  char id[16];
  int i;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (!strcmp(mapa[i].id, id)) break;
  i = i < n;
  pthread_mutex_unlock(&trava);
  return i;
}

int vistoep_marcar_lote(const char *imdb, const VistoPar *pares, int qtd, int visto) {
  int i, mudou = 0;
  if (!pares) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < qtd; i++) {
    if (estado(imdb, pares[i].temporada, pares[i].episodio) == (visto ? 1 : 0))
      continue;
    mudou += definir(imdb, pares[i].temporada, pares[i].episodio, visto);
  }
  pthread_mutex_unlock(&trava);
  // O LOTE INTEIRO, e nao so o que mudou de estado: o gesto fala de todos
  // (ver vistoep_aplicar).
  if (lapGesto && qtd > 0) lapGesto(imdb, pares, qtd, visto);
  return mudou;
}

int vistoep_ajustar_vistos(int baseTrakt, int contarBase, int contarAgora, int exibidos) {
  int v = baseTrakt + (contarAgora - contarBase);
  if (v > exibidos) v = exibidos;
  return v < 0 ? 0 : v;
}

int vistoep_primeiro_nao_visto(const char *imdb, int *temporada, int *episodio) {
  char id[16];
  int i, bt = 0, be = 0;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) {
    if (mapa[i].visto || mapa[i].temp < 1 || strcmp(mapa[i].id, id)) continue;
    if (!bt || mapa[i].temp < bt || (mapa[i].temp == bt && mapa[i].ep < be)) {
      bt = mapa[i].temp; be = mapa[i].ep;
    }
  }
  pthread_mutex_unlock(&trava);
  if (!bt) return 0;
  if (temporada) *temporada = bt;
  if (episodio) *episodio = be;
  return 1;
}

int vistoep_aplicar(const char *imdb, const VistoPar *lote, int n, int visto,
                    VistoPar *envio, int *ja) {
  int i, k = 0;
  if (ja) *ja = 0;
  if (!lote || !envio || n < 1) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) {
    if (estado(imdb, lote[i].temporada, lote[i].episodio) == (visto ? 1 : 0)) {
      if (ja) (*ja)++;
      continue;
    }
    if (definir(imdb, lote[i].temporada, lote[i].episodio, visto)) envio[k++] = lote[i];
  }
  pthread_mutex_unlock(&trava);
  // O GESTO VAI PARA O JUIZ COM O LOTE INTEIRO, e nao so com `envio`. Um
  // episodio que ja estava 0 aqui (o Trakt nao o tem) pode estar "visto" em
  // outra fonte que este mapa nao leu; "desmarcar a temporada" fala de todos.
  // Marcar solta a desmarcacao de todos pelo mesmo motivo.
  if (lapGesto) lapGesto(imdb, lote, n, visto);
  return k;
}

// O GESTO NO TITULO INTEIRO ("marcar/desmarcar a serie", visto.c). Nao mexe no
// estado do mapa — isso continua vindo da proxima leitura, como antes —, so
// avisa o juiz: marcar a serie SOLTA toda desmarcacao dela (senao um episodio
// desmarcado ontem ficaria de fora de "marquei tudo"); desmarcar a serie guarda
// a desmarcacao de cada episodio que o mapa tem como visto (senao a fonte que
// nao aplicar o remove traz a serie inteira de volta).
void vistoep_titulo_gesto(const char *imdb, int visto) {
  char id[16];
  VistoPar *pares;
  int i, k = 0;
  if (!lapGesto) return;
  base(imdb, id, sizeof id);
  if (!id[0]) return;
  if (visto) { lapGesto(id, NULL, 0, 1); return; }
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) if (mapa[i].visto && !strcmp(mapa[i].id, id)) k++;
  pares = k ? (VistoPar *)malloc(sizeof *pares * (size_t)k) : NULL;
  k = 0;
  if (pares)
    for (i = 0; i < n; i++)
      if (mapa[i].visto && !strcmp(mapa[i].id, id)) {
        pares[k].temporada = mapa[i].temp;
        pares[k].episodio = mapa[i].ep;
        k++;
      }
  pthread_mutex_unlock(&trava);
  if (pares && k) lapGesto(id, pares, k, 0);
  free(pares);
}

// ORDEM DE EPISODIO E (temporada, numero), nesta ordem — nao o numero sozinho.
// A temporada 0 dos especiais fica ANTES da 1, que e onde o Trakt tambem a
// coloca; marcar "ate aqui" no episodio 3 da temporada 2 nao pode arrastar a
// temporada 3 so porque o numero dela e menor.
static int antesOuIgual(int t, int e, int tAlvo, int eAlvo) {
  if (t != tAlvo) return t < tAlvo;
  return e <= eAlvo;
}

static int intervalo(const char *imdb, int temporada, int episodio, int frente,
                     int agT, int agE, VistoPar *saida, int max) {
  char id[16];
  int i, k = 0;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  // `saida` NULO E MODO CONTAGEM, e `max` e ignorado nele. A tela precisa do
  // NUMERO antes de decidir se cabe pedir a acao ("Ate aqui (7 episodios)"), e
  // sem isto ela teria de alocar um vetor so para descobrir o tamanho — ou,
  // pior, passar max=0 e receber 0 sempre, que foi o primeiro erro aqui.
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) {
    if (strcmp(mapa[i].id, id)) continue;
    int t = mapa[i].temp, e = mapa[i].ep;
    if (frente ? !antesOuIgual(temporada, episodio, t, e)
               : !antesOuIgual(t, e, temporada, episodio)) continue;
    if (agT > 0 && agE > 0 && antesOuIgual(agT, agE, t, e)) continue;
    if (saida) {
      if (k >= max) break;
      saida[k].temporada = mapa[i].temp;
      saida[k].episodio = mapa[i].ep;
    }
    k++;
  }
  pthread_mutex_unlock(&trava);
  return k;
}

int vistoep_ate_aqui(const char *imdb, int temporada, int episodio,
                     VistoPar *saida, int max) {
  return intervalo(imdb, temporada, episodio, 0, 0, 0, saida, max);
}

int vistoep_temporada(const char *imdb, int temporada, VistoPar *saida, int max) {
  char id[16];
  int i, k = 0;
  base(imdb, id, sizeof id);
  if (!id[0]) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < n; i++) {                 // saida nula = contagem, ver acima
    if (mapa[i].temp != temporada || strcmp(mapa[i].id, id)) continue;
    if (saida) {
      if (k >= max) break;
      saida[k].temporada = mapa[i].temp;
      saida[k].episodio = mapa[i].ep;
    }
    k++;
  }
  pthread_mutex_unlock(&trava);
  return k;
}

int vistoep_lote(const char *imdb, int modo, int temporada, int episodio,
                 const VistoPar *cat, int nCat, int agT, int agE,
                 VistoPar *saida, int max) {
  static VistoPar buf[VE_LOTE];
  int k, i, j;
  if (saida && max < 1) return 0;
  if (modo == VE_LOTE_DAQUI)
    k = intervalo(imdb, temporada, episodio, 1, agT, agE, buf, VE_LOTE);
  else
    k = modo == VE_LOTE_ATE ? vistoep_ate_aqui(imdb, temporada, episodio, buf, VE_LOTE)
                           : vistoep_temporada(imdb, temporada, buf, VE_LOTE);
  for (i = 0; cat && i < nCat && k < VE_LOTE; i++) {
    int t = cat[i].temporada, e = cat[i].episodio, dentro;
    if (e < 1) continue;
    // Nao foi ao ar: a partir de (agT, agE), o proximo episodio da agenda.
    if (agT > 0 && agE > 0 && (t != agT ? t > agT : e >= agE)) continue;
    dentro = modo == VE_LOTE_DAQUI ? t >= 0 && antesOuIgual(temporada, episodio, t, e)
           : modo == VE_LOTE_ATE ? t > 0 && antesOuIgual(t, e, temporada, episodio)
                                : t == temporada;
    if (!dentro) continue;
    for (j = 0; j < k; j++) if (buf[j].temporada == t && buf[j].episodio == e) break;
    if (j < k) continue;
    buf[k].temporada = (short)t;
    buf[k].episodio = (short)e;
    k++;
  }
  if (saida) {
    if (k > max) k = max;
    memcpy(saida, buf, sizeof(VistoPar) * (size_t)(k > 0 ? k : 0));
  }
  return k;
}

int vistoep_n(void) {
  int r;
  pthread_mutex_lock(&trava); r = n; pthread_mutex_unlock(&trava);
  return r;
}
unsigned vistoep_revisao(void) {
  unsigned r;
  pthread_mutex_lock(&trava); r = revisao; pthread_mutex_unlock(&trava);
  return r;
}

void vistoep_esquecer(void) {
  pthread_mutex_lock(&trava);
  free(mapa); mapa = NULL; n = 0; cap = 0; avisouTeto = 0;
  revisao++;
  pthread_mutex_unlock(&trava);
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
  VistoFonte conta;
  int total = 0;
  if (!imdb || !imdb[0] || !json) return -1;
  memset(&conta, 0, sizeof conta);
  fim = json + strlen(json);
  temps = js_array(json, fim, "seasons");
  if (!temps) { printf("[vistoep] %s: resposta sem \"seasons\"\n", imdb); fflush(stdout); return -1; }
  for (; temps && *temps == '{'; temps = js_prox(js_fim(temps))) {
    const char *ft = js_fim(temps), *eps;
    double temporada;
    int nt;
    if (!ft || ft > fim || ft[-1] != '}') return -1;
    temporada = js_num(temps, ft, "number", -1.0);
    if (!isfinite(temporada) || temporada < 0 || temporada > SHRT_MAX) continue;
    nt = (int)temporada;
    if (temporada != nt) continue;
    eps = js_array(temps, ft, "episodes");
    for (; eps && *eps == '{'; eps = js_prox(js_fim(eps))) {
      const char *fe = js_fim(eps);
      double episodio;
      char concluido[16], quando[40];
      int ne, feito;
      if (!fe || fe > ft || fe[-1] != '}') return -1;
      episodio = js_num(eps, fe, "number", -1.0);
      if (!isfinite(episodio) || episodio < 1 || episodio > SHRT_MAX) continue;
      ne = (int)episodio;
      if (episodio != ne) continue;
      // Somente a afirmacao explicita true/false define o estado. Procurar
      // "true" perto da chave lia strings como bool e falhava com espacos.
      if (!js_bruto(eps, fe, "completed", concluido, sizeof concluido)) continue;
      if (!strcmp(concluido, "true")) feito = 1;
      else if (!strcmp(concluido, "false")) feito = 0;
      else continue;
      // QUANDO o Trakt diz que foi visto: e o que deixa um visto de verdade,
      // feito em outro aparelho DEPOIS de a pessoa desmarcar aqui, ganhar.
      quando[0] = 0;
      if (feito) js_texto(eps, fe, "last_watched_at", quando, sizeof quando);
      total += vistoep_fonte(imdb, nt, ne, feito, quando[0] ? js_ms_iso(quando) : 0, &conta);
      if (fe >= ft) break;
    }
    if (ft >= fim) break;
  }
  printf("[vistoep] %s: %d episodios no mapa (%d vistos)\n",
         imdb, total, vistoep_contar(imdb));
  vistoep_fonte_log(imdb, "trakt", &conta);
  return total;
}
