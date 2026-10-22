#include "proximo.h"
#include <stdio.h>
#include <string.h>

// A lista de episodios do catalogo e UNICA e cobre todas as temporadas, na
// ordem — e por isso que "proximo" e simplesmente o indice seguinte, e que o
// fim de uma temporada cai no comeco da outra sem nenhum caso especial.
//
// O web garante essa ordem em normalizeEpisodeEntries (homeScreen.js:1571),
// que tambem JOGA FORA o que tem temporada 0: especial nao entra na contagem e
// nunca e oferecido como proximo. Aqui a lista nao pode ser reordenada (vem
// const do catalogo), entao a ordem e tomada como dada e so o descarte do
// especial e repetido, em cada varredura.
static int valido(const CatEp *e) {
  return e && e->temporada > 0 && e->episodio > 0;
}

int prox_ancora_absoluta(const CatEp *eps, int n, int temporada, int episodio) {
  int i, pos = 0;
  // Numeracao absoluta sempre chega como temporada 1: e a temporada unica do
  // rastreador que ficou sem mapeamento. Qualquer outra e leitura normal.
  if (!eps || n <= 0 || temporada != 1 || episodio <= 0) return -1;
  for (i = 0; i < n; i++) {
    if (!valido(&eps[i])) continue;
    if (++pos == episodio) return i;
  }
  return -1;
}

int prox_ancora(const CatEp *eps, int n, int temporada, int episodio,
                int absolutaSimkl) {
  int i;
  if (!eps || n <= 0) return -1;
  if (temporada > 0 && episodio > 0)
    for (i = 0; i < n; i++)
      if (valido(&eps[i]) && eps[i].temporada == temporada &&
          eps[i].episodio == episodio) return i;
  // So depois de o par falhar, e so quando quem chama AFIRMA que a numeracao e
  // absoluta. Reinterpretar por conta propria transformaria S2E3 de uma serie
  // normal no terceiro episodio da serie inteira.
  if (absolutaSimkl) return prox_ancora_absoluta(eps, n, temporada, episodio);
  return -1;
}

// Dias desde 1970-01-01 pelo calendario civil, sem passar por mktime: mktime
// interpreta no fuso da TV, e a data de estreia do Cinemeta e UTC. Um dia de
// erro perto da borda da janela de 60 dias nao muda nada, mas o fuso da LG ja
// veio errado em aparelho de teste, e ai o erro seria de horas ou de um dia
// inteiro dependendo do aparelho — resultado diferente por TV.
static long long diasCivis(int a, int m, int d) {
  long long y = a - (m <= 2), era, aoe, doy, doe;
  era = (y >= 0 ? y : y - 399) / 400;
  aoe = y - era * 400;
  doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  doe = aoe * 365 + aoe / 4 - aoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

long long prox_data_ms(const char *data) {
  static const char *MES[12] = {
    "janeiro", "fevereiro", "mar\xc3\xa7o", "abril", "maio", "junho",
    "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"
  };
  int a = 0, m = 0, d = 0, i;
  char nome[24];
  if (!data || !data[0]) return PROX_SEM_DATA;
  // ISO, do caso em que a data passou direto sem ser formatada.
  if (sscanf(data, "%4d-%2d-%2d", &a, &m, &d) == 3 && data[4] == '-') {
    if (m < 1 || m > 12 || d < 1 || d > 31) return PROX_SEM_DATA;
    return diasCivis(a, m, d) * 86400000LL;
  }
  // Por extenso, como desc_data_extenso escreve: "27 de janeiro de 2023".
  if (sscanf(data, "%d de %23s de %d", &d, nome, &a) != 3) return PROX_SEM_DATA;
  for (i = 0; i < 12; i++) if (!strcmp(nome, MES[i])) m = i + 1;
  // So o ano ("2026", o que sobra de um ISO curto) cai aqui e sai como sem
  // data: mes desconhecido nao vira 1 de janeiro chutado.
  if (!m || d < 1 || d > 31) return PROX_SEM_DATA;
  return diasCivis(a, m, d) * 86400000LL;
}

int prox_mostrar_nao_acompanhada(long long semeadoEmMs, long long lancamentoMs,
                                 long long agoraMs) {
  long long dist;
  if (lancamentoMs == PROX_SEM_DATA) return 0;    // parseReleaseEpochMs == null
  if (lancamentoMs <= semeadoEmMs) return 0;      // ja existia quando ela parou
  dist = lancamentoMs - agoraMs;
  if (dist < 0) dist = -dist;
  return dist <= PROX_JANELA_NOVIDADE_MS;
}

int prox_para_item(const CatItem *ci, const CatEp *eps, int n,
                   long long agoraMs, ProxSugestao *saida) {
  int ancora, i;
  if (!ci || !saida) return 0;
  if (strcmp(ci->tipo, "series")) return 0;
  if (ci->temporada <= 0 || ci->episodio <= 0) return 0;
  // Episodio semeado ainda em andamento: o proprio card ja oferece a coisa
  // certa. "Proximo" so existe depois que o anterior acabou.
  if (ci->progresso < PROX_CONCLUIDO) return 0;
  // Sem lista de episodios nao ha o que varrer. Acontece de verdade: o
  // catalogo so recebe os episodios quando o titulo e ABERTO (app.c:510), e a
  // home nao vai busca-los so para decorar um card — seria uma ida a rede por
  // item da fileira, exatamente o custo que este port passou meses derrubando.
  if (!eps || n <= 0) return 0;

  // Sem sinal de "numeracao absoluta do Simkl" no nativo: o item vem do
  // /sync/playback do Trakt, que ja entrega temporada e episodio na numeracao
  // dos addons. Entao 0 — nunca reinterpretar.
  ancora = prox_ancora(eps, n, ci->temporada, ci->episodio, 0);
  if (ancora < 0) return 0;

  for (i = ancora + 1; i < n; i++) {
    if (!valido(&eps[i])) continue;
    // Serie nao acompanhada e o unico caso que o nativo conhece (ver o
    // cabecalho): sem data legivel, ou fora da janela de noticia, nao mostra.
    if (!prox_mostrar_nao_acompanhada(0, prox_data_ms(eps[i].data), agoraMs))
      return 0;
    saida->temporada = eps[i].temporada;
    saida->episodio  = eps[i].episodio;
    snprintf(saida->nome, sizeof saida->nome, "%s", eps[i].nome);
    snprintf(saida->data, sizeof saida->data, "%s", eps[i].data);
    return 1;
  }
  // Acabou a lista: a ultima temporada terminou e nao ha proximo.
  return 0;
}

int prox_seguinte(const CatItem *ci, const CatEp *eps, int n, int limiarPct,
                  long long agoraMs, ProxSugestao *saida) {
  int ancora, i;
  if (!ci || !saida) return 0;
  if (strcmp(ci->tipo, "series")) return 0;
  if (ci->temporada <= 0 || ci->episodio <= 0) return 0;
  if (ci->progresso < limiarPct) return 0;
  if (!eps || n <= 0) return 0;
  ancora = prox_ancora(eps, n, ci->temporada, ci->episodio, 0);
  if (ancora < 0) return 0;
  for (i = ancora + 1; i < n; i++) {
    long long quando;
    if (!valido(&eps[i])) continue;
    quando = prox_data_ms(eps[i].data);
    if (quando != PROX_SEM_DATA && quando > agoraMs) return 0;   // nao foi ao ar
    saida->temporada = eps[i].temporada;
    saida->episodio  = eps[i].episodio;
    snprintf(saida->nome, sizeof saida->nome, "%s", eps[i].nome);
    snprintf(saida->data, sizeof saida->data, "%s", eps[i].data);
    return 1;
  }
  return 0;
}

int prox_indice_seguinte(const CatEp *eps, int n, int t, int e) {
  int i, melhor = -1;
  if (!eps || n < 1) return -1;
  for (i = 0; i < n; i++) {
    const CatEp *p = &eps[i];
    if (p->episodio < 1 || p->temporada < 0) continue;
    if (p->temporada == 0 && t > 0) continue;          // especial nao e o proximo
    if (p->temporada < t || (p->temporada == t && p->episodio <= e)) continue;
    if (melhor < 0 || p->temporada < eps[melhor].temporada ||
        (p->temporada == eps[melhor].temporada && p->episodio < eps[melhor].episodio))
      melhor = i;
  }
  return melhor;
}
