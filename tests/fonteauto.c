// ISSUE #130: QUANTAS URLS A ESCOLHA AUTOMATICA TOCA ANTES DE TOCAR O VIDEO.
//
// O relator viu 6 arquivos da mesma serie no painel do TorBox depois de UMA
// reproducao. Cada "tocar" aqui e o que custa um arquivo na conta de debrid:
// o GET com Range no link do AIOStreams ou o createtorrent de debrid.c. O
// teste nao tem rede — conta as chamadas de verificar() que streams.c faria.
//
//   bash tests/fonteauto.sh
#include "fonteauto.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHOU %s:%d: ", __FILE__, __LINE__); \
  printf(__VA_ARGS__); printf("\n"); } } while (0)

#define N 12
typedef struct {
  int boa[N];            // 1 = a verificacao aprova
  int tocou[N];          // quantas vezes a URL desta fonte foi tocada
  unsigned char excl[N]; // a exclusao da lista (stream_automatico_excluir)
  int total;             // toques somados
} Cena;

static int verificar(int i, void *u) {
  Cena *c = u;
  c->tocou[i]++;
  c->total++;
  return c->boa[i];
}
static void falhou(int i, void *u) { ((Cena *)u)->excl[i] = 1; }

static void cena(Cena *c, int boas) {
  int i;
  memset(c, 0, sizeof *c);
  for (i = 0; i < N; i++) c->boa[i] = boas;
}

// Uma escolha, como stream_primeira_boa: fila + verificacao em serie.
static int escolher(Cena *c, int modo, int pedidas, int preferida,
                    const long *pontos, const unsigned char *acima, int *tocadas) {
  int fila[16], nf;
  nf = fonteauto_fila(modo, N, preferida, pontos, acima, c->excl,
                      fonteauto_tentativas(modo, pedidas), fila);
  return fonteauto_primeira(fila, nf, verificar, falhou, c, tocadas);
}

// A REPRODUCAO INTEIRA, como app.c a conduz: escolhe, entrega ao player, e se
// o player falhar (ou, em "Primeira da lista", se a conferencia falhar) tenta
// a proxima enquanto couber em 1 + repor. `tocaNoPlayer[i]` = 1 se o player
// abre aquela fonte. Devolve o indice que tocou ou -1.
static int reproduzir(Cena *c, int modo, int repor, const int *tocaNoPlayer,
                      const long *pontos) {
  int tentativas = 0, maximo = 1 + repor;
  for (;;) {
    int e = escolher(c, modo, 8, -1, pontos, NULL, NULL);
    if (e < 0) {
      // app.c: so "Primeira da lista" reenvia depois de conferencia vazia.
      if (modo == FONTEAUTO_PRIMEIRA && ++tentativas < maximo) continue;
      return -1;
    }
    tentativas++;
    if (tocaNoPlayer[e]) return e;
    c->excl[e] = 1;                       // tentarProximaFonteVOD
    if (tentativas >= maximo) return -1;
  }
}

int main(void) {
  Cena c;
  int e, tocadas, i;
  long pts[N];
  unsigned char acima[N];
  int player[N];

  for (i = 0; i < N; i++) pts[i] = 1000 + i;   // a MELHOR e a ultima

  // 1) O caso do relato: tudo em cache, tudo serve. "Primeira da lista" toca
  //    UMA URL — a do indice 0, na ordem do addon.
  cena(&c, 1);
  e = escolher(&c, FONTEAUTO_PRIMEIRA, 8, -1, pts, NULL, &tocadas);
  CONFERE(e == 0, "primeira da lista escolheu %d", e);
  CONFERE(tocadas == 1 && c.total == 1, "primeira da lista tocou %d URLs", c.total);

  // 2) "Melhor fonte" com tudo servindo: tambem UMA URL, a de maior pontuacao.
  //    O lote paralelo de antes tocava no minimo 4 aqui (4 fios saindo juntos).
  cena(&c, 1);
  e = escolher(&c, FONTEAUTO_MELHOR, 8, -1, pts, NULL, &tocadas);
  CONFERE(e == N - 1, "melhor fonte escolheu %d", e);
  CONFERE(c.total == 1, "melhor fonte tocou %d URLs com a primeira servindo", c.total);

  // 3) A preferida (fontepref) entra na frente nos dois modos, e continua
  //    sendo UMA candidata no modo "Primeira da lista".
  cena(&c, 1);
  e = escolher(&c, FONTEAUTO_PRIMEIRA, 8, 5, pts, NULL, &tocadas);
  CONFERE(e == 5 && c.total == 1, "preferida: escolheu %d, tocou %d", e, c.total);

  // 4) "Primeira da lista" nunca confere uma segunda, mesmo quando a primeira
  //    nao serve: quem decide se tenta outra e o orcamento de app.c.
  cena(&c, 1); c.boa[0] = 0;
  e = escolher(&c, FONTEAUTO_PRIMEIRA, 8, -1, pts, NULL, &tocadas);
  CONFERE(e == -1 && c.total == 1, "primeira que falha: escolheu %d, tocou %d", e, c.total);
  CONFERE(c.excl[0] == 1, "a que falhou nao saiu da fila");
  //    ...e a chamada seguinte vai para a 1, sem tocar a 0 de novo.
  e = escolher(&c, FONTEAUTO_PRIMEIRA, 8, -1, pts, NULL, &tocadas);
  CONFERE(e == 1 && c.tocou[0] == 1 && c.total == 2, "reenvio: %d, 0 tocada %dx", e, c.tocou[0]);

  // 5) Teto de qualidade: a de fora do teto vai para o fim, sem sair.
  cena(&c, 1);
  memset(acima, 0, sizeof acima); acima[0] = acima[1] = 1;
  e = escolher(&c, FONTEAUTO_PRIMEIRA, 8, -1, pts, acima, &tocadas);
  CONFERE(e == 2 && c.total == 1, "teto: escolheu %d", e);
  memset(acima, 1, sizeof acima);
  cena(&c, 1);
  e = escolher(&c, FONTEAUTO_PRIMEIRA, 8, -1, pts, acima, &tocadas);
  CONFERE(e == 0, "tudo acima do teto: escolheu %d (tem de tocar mesmo assim)", e);

  // 6) "Melhor fonte" com as duas melhores falhando: em serie, para na
  //    terceira — 3 URLs, nunca a quarta.
  cena(&c, 1); c.boa[N - 1] = c.boa[N - 2] = 0;
  e = escolher(&c, FONTEAUTO_MELHOR, 8, -1, pts, NULL, &tocadas);
  CONFERE(e == N - 3 && c.total == 3, "melhor com 2 falhas: %d, tocou %d", e, c.total);
  CONFERE(c.tocou[N - 4] == 0, "tocou uma candidata depois da que serviu");

  // 7) O limite de `tentativas` vale: nada serve, 8 pedidas, 8 tocadas.
  cena(&c, 0);
  e = escolher(&c, FONTEAUTO_MELHOR, 8, -1, pts, NULL, &tocadas);
  CONFERE(e == -1 && c.total == 8, "nada serve: tocou %d", c.total);

  // 8) A REPRODUCAO INTEIRA em "Primeira da lista" com o padrao (repor = 2):
  //    a 0 nao passa na conferencia, a 1 trava no player, a 2 toca.
  //    3 URLs, cada uma UMA vez.
  cena(&c, 1); c.boa[0] = 0;
  for (i = 0; i < N; i++) player[i] = 1;
  player[1] = 0;
  e = reproduzir(&c, FONTEAUTO_PRIMEIRA, 2, player, pts);
  CONFERE(e == 2 && c.total == 3, "fluxo repor=2: tocou %d, total %d", e, c.total);
  for (i = 0; i < N; i++) CONFERE(c.tocou[i] <= 1, "URL %d tocada %dx", i, c.tocou[i]);

  // 9) "Outra fonte se falhar" desligado: uma URL e acabou.
  cena(&c, 1);
  for (i = 0; i < N; i++) player[i] = 0;
  e = reproduzir(&c, FONTEAUTO_PRIMEIRA, 0, player, pts);
  CONFERE(e == -1 && c.total == 1, "repor=0: tocou %d URLs", c.total);

  // 10) Pior caso limitado: nada toca no player, repor = 3 -> 4 URLs.
  cena(&c, 1);
  e = reproduzir(&c, FONTEAUTO_PRIMEIRA, 3, player, pts);
  CONFERE(e == -1 && c.total == 4, "repor=3, nada toca: %d URLs", c.total);

  // 11) "Melhor fonte" no fluxo inteiro com tudo servindo: UMA URL.
  cena(&c, 1);
  for (i = 0; i < N; i++) player[i] = 1;
  e = reproduzir(&c, FONTEAUTO_MELHOR, 2, player, pts);
  CONFERE(e == N - 1 && c.total == 1, "melhor, fluxo: %d URLs", c.total);

  CONFERE(fonteauto_tentativas(FONTEAUTO_PRIMEIRA, 8) == 1, "primeira pede mais de 1");
  CONFERE(fonteauto_tentativas(FONTEAUTO_MELHOR, 8) == 8, "melhor mudou de 8");

  if (falhas) { printf("fonteauto: %d falha(s)\n", falhas); return 1; }
  printf("fonteauto: tudo ok\n");
  return 0;
}
