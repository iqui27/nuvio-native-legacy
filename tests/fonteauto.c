// ISSUE #130: QUANTAS URLS A ESCOLHA AUTOMATICA TOCA ANTES DE TOCAR O VIDEO.
//
// O relator viu 6 arquivos da mesma serie no painel do TorBox depois de UMA
// reproducao. Cada "tocar" aqui e o que custa um arquivo na conta de debrid:
// o GET com Range no link do AIOStreams ou o createtorrent de debrid.c. O
// teste nao tem rede — conta as chamadas de verificar() que streams.c faria.
//
//   bash tests/fonteauto.sh
#include "fonteauto.h"
#include "fonteregra.h"
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

// #221: addons 0 e 2 faltam; a lista parcial tem fontes dos addons 1 e 3.
static int faltaAntes(int addon, void *u) { (void)u; return addon > 0; }
static void parcial(void) {
  long pt[3] = { 1080, 2160, 720 };
  unsigned char ac[3] = { 0, 0, 0 }, ex[3] = { 0, 0, 0 }, bo[3] = { 0, 1, 0 };
  int ad[3] = { 1, 1, 3 };
  FonteautoParcial p;
  memset(&p, 0, sizeof p);
  p.modo = FONTEAUTO_MELHOR; p.total = 3; p.preferida = -1; p.algumPendente = 1;
  p.pontos = pt; p.acimaTeto = ac; p.excluida = ex; p.boa = bo; p.addon = ad;
  p.pendenteAntes = faltaAntes;
  CONFERE(fonteauto_pode_decidir(&p), "melhor: a 4K boa ja presente nao decide");
  bo[1] = 0;
  CONFERE(!fonteauto_pode_decidir(&p), "melhor: sem boa decidiu antes do prazo");
  p.prazoPassou = 1;
  CONFERE(fonteauto_pode_decidir(&p), "melhor: prazo passou e nao decidiu");
  p.prefPendente = 1;
  CONFERE(!fonteauto_pode_decidir(&p), "lembrada pendente: o prazo nao vale");
  p.preferida = 2;
  CONFERE(fonteauto_pode_decidir(&p), "lembrada presente nao decidiu");
  p.preferida = -1; p.prefPendente = 0; p.prazoPassou = 0;
  p.modo = FONTEAUTO_PRIMEIRA;
  CONFERE(!fonteauto_pode_decidir(&p), "primeira: o addon 0 falta e decidiu");
  ad[0] = ad[1] = 0;
  p.pendenteAntes = NULL;
  CONFERE(fonteauto_pode_decidir(&p), "primeira: nada antes falta e nao decidiu");
  ac[0] = 1;   // a primeira (pela ordem) passa a estar acima do teto
  CONFERE(fonteauto_pode_decidir(&p), "primeira: a de dentro do teto (1) e a da fila");
  ac[1] = ac[2] = 1;
  CONFERE(!fonteauto_pode_decidir(&p), "primeira: so acima do teto e alguem falta");
  ex[0] = ex[1] = ex[2] = 1;
  p.prazoPassou = 1;
  CONFERE(!fonteauto_pode_decidir(&p), "sem candidata nao excluida decidiu");
  p.algumPendente = 0; ex[0] = 0;
  CONFERE(fonteauto_pode_decidir(&p), "ninguem falta e nao decidiu");
}

// #202: grupos de fonteregra.h. Fontes 0,1 = addon de fora (grupo 2); 2 =
// permitido sem casar a regex (1); 3 = permitido que casa (0); 4 = nunca (-1).
static void grupos(void) {
  long pt[5] = { 9000, 8000, 100, 50, 99999 };
  signed char g[5] = { 2, 2, 1, 0, -1 };
  unsigned char ex[5] = { 0 }, ac[5] = { 0 }, bo[5] = { 1, 1, 0, 0, 1 };
  int fila[8], nf;
  FonteautoParcial p;
  // A fila: grupo por grupo; dentro dele, a regra do modo.
  nf = fonteauto_fila_g(FONTEAUTO_MELHOR, 5, -1, pt, NULL, ex, g, 8, fila);
  CONFERE(nf == 4 && fila[0] == 3 && fila[1] == 2 && fila[2] == 0 && fila[3] == 1,
          "melhor em grupos: %d [%d %d %d %d]", nf, fila[0], fila[1], fila[2], fila[3]);
  nf = fonteauto_fila_g(FONTEAUTO_PRIMEIRA, 5, -1, pt, NULL, ex, g, 1, fila);
  CONFERE(nf == 1 && fila[0] == 3, "primeira em grupos escolheu %d", fila[0]);
  // A lembrada continua na frente, mesmo de outro grupo.
  nf = fonteauto_fila_g(FONTEAUTO_MELHOR, 5, 0, pt, NULL, ex, g, 8, fila);
  CONFERE(fila[0] == 0 && fila[1] == 3, "lembrada em grupos: %d %d", fila[0], fila[1]);
  // Sem grupos e igual a fila de sempre.
  nf = fonteauto_fila_g(FONTEAUTO_MELHOR, 5, -1, pt, NULL, ex, NULL, 1, fila);
  CONFERE(fila[0] == 4, "sem grupos mudou a fila: %d", fila[0]);
  // So sobram os de fora (2 e 3 falharam): eles tocam, em ordem.
  ex[2] = ex[3] = 1;
  nf = fonteauto_fila_g(FONTEAUTO_MELHOR, 5, -1, pt, NULL, ex, g, 8, fila);
  CONFERE(nf == 2 && fila[0] == 0, "fallback: %d, %d", nf, fila[0]);
  ex[2] = ex[3] = 0;

  // Parcial: os de fora chegaram (boa 4K), o permitido ainda nao respondeu.
  memset(&p, 0, sizeof p);
  {
    signed char g2[2] = { 2, 2 };
    long p2[2] = { 9000, 8000 };
    unsigned char e2[2] = { 0, 0 }, a2[2] = { 0, 0 }, b2[2] = { 1, 1 };
    p.modo = FONTEAUTO_MELHOR; p.total = 2; p.preferida = -1; p.algumPendente = 1;
    p.pontos = p2; p.acimaTeto = a2; p.excluida = e2; p.boa = b2; p.grupo = g2;
    p.pendenteGrupoMin = 0;
    CONFERE(!fonteauto_pode_decidir(&p), "permitido pendente: os de fora sairam antes do prazo");
    p.prazoPassou = 1;
    CONFERE(fonteauto_pode_decidir(&p), "prazo passou: os de fora nao sairam");
    p.prazoPassou = 0; p.pendenteGrupoMin = 2;   // so falta gente de fora
    CONFERE(fonteauto_pode_decidir(&p), "permitidos responderam sem fonte: nao usou os outros");
    p.pendenteGrupoMin = 99; g2[0] = g2[1] = -1; p.prazoPassou = 1;
    CONFERE(!fonteauto_pode_decidir(&p), "tudo fora da regra decidiu");
  }
  // Instantaneo: a primeira do melhor grupo sai sem ser "boa".
  memset(&p, 0, sizeof p);
  p.modo = FONTEAUTO_MELHOR; p.total = 5; p.preferida = -1; p.algumPendente = 1;
  p.pontos = pt; p.acimaTeto = ac; p.excluida = ex; p.boa = bo; p.grupo = g;
  p.pendenteGrupoMin = 0;
  CONFERE(fonteauto_pode_decidir(&p) == 0, "esperar: a 3 nao e boa e decidiu");
  p.instantaneo = 1;
  CONFERE(fonteauto_pode_decidir(&p) == 1, "instantaneo nao decidiu com a permitida presente");
  g[2] = g[3] = 2;            // sem permitida presente
  CONFERE(fonteauto_pode_decidir(&p) == 0, "instantaneo: usou os outros com permitido pendente");
  p.pendenteGrupoMin = 99;
  CONFERE(fonteauto_pode_decidir(&p) == 1, "instantaneo: ninguem permitido falta e nao decidiu");
}

// 2.0.2: ordem dos add-ons. Fontes 0..5 de tres add-ons: A (rank 0) tem 0 e 1,
// B (rank 1) tem 2 e 3, C (fora da ordem, rank 98) tem 4 e 5. Pontos: o 4K do C
// (4) e o melhor, o 1080p do A (0) empata na faixa com o do B (2).
static void ordemAddons(void) {
  long pt[6] = { 20500, 20400, 20300, 10000, 41000, 40000 };
  int rk[6] = { 0, 0, 1, 1, FR_ORDEM_SEM, FR_ORDEM_SEM };
  unsigned char ex[6] = { 0 }, ac[6] = { 0 }, bo[6] = { 0 };
  int fila[8], nf;
  FonteautoParcial p;
  // Nao: a fila de sempre, a de maior pontuacao primeiro.
  nf = fonteauto_fila_o(FONTEAUTO_MELHOR, 6, -1, pt, NULL, ex, NULL, rk, FR_ORDEM_NAO, 8, fila);
  CONFERE(nf == 6 && fila[0] == 4 && fila[1] == 5 && fila[2] == 0, "ordem nao: %d %d %d", fila[0], fila[1], fila[2]);
  // Desempatar: na mesma faixa (pontos / 10000) vence o add-on mais cedo; a
  // faixa maior ainda manda (o 4K do C segue na frente dos 1080p).
  nf = fonteauto_fila_o(FONTEAUTO_MELHOR, 6, -1, pt, NULL, ex, NULL, rk, FR_ORDEM_DESEMPATE, 8, fila);
  CONFERE(fila[0] == 4 && fila[1] == 5 && fila[2] == 0 && fila[3] == 1 && fila[4] == 2 && fila[5] == 3,
          "desempate: %d %d %d %d %d %d", fila[0], fila[1], fila[2], fila[3], fila[4], fila[5]);
  // Faixas iguais, ordem diferente da pontuacao: B (mais pontos) perde para A.
  pt[2] = 29000;
  nf = fonteauto_fila_o(FONTEAUTO_MELHOR, 4, -1, pt, NULL, ex, NULL, rk, FR_ORDEM_DESEMPATE, 8, fila);
  CONFERE(fila[0] == 0 && fila[1] == 1 && fila[2] == 2, "desempate A antes de B: %d %d %d", fila[0], fila[1], fila[2]);
  nf = fonteauto_fila_o(FONTEAUTO_MELHOR, 4, -1, pt, NULL, ex, NULL, rk, FR_ORDEM_NAO, 8, fila);
  CONFERE(fila[0] == 2, "sem ordem B (mais pontos) vence: %d", fila[0]);
  pt[2] = 20300;
  // Estrita: todas as fontes do A, depois as do B, depois as de fora, cada
  // add-on pela regra do modo; o 4K do C so depois de A e B.
  nf = fonteauto_fila_o(FONTEAUTO_MELHOR, 6, -1, pt, NULL, ex, NULL, rk, FR_ORDEM_ESTRITA, 8, fila);
  CONFERE(nf == 6 && fila[0] == 0 && fila[1] == 1 && fila[2] == 2 && fila[3] == 3 && fila[4] == 4 && fila[5] == 5,
          "estrita: %d %d %d %d %d %d", fila[0], fila[1], fila[2], fila[3], fila[4], fila[5]);
  // Estrita nao baixa abaixo do teto: a fonte acima do teto do A vai depois
  // das de dentro do teto dos outros.
  ac[0] = ac[1] = 1;
  nf = fonteauto_fila_o(FONTEAUTO_MELHOR, 6, -1, pt, ac, ex, NULL, rk, FR_ORDEM_ESTRITA, 8, fila);
  CONFERE(fila[0] == 2 && fila[1] == 3 && fila[2] == 4 && fila[4] == 0, "estrita com teto: %d %d %d [%d]", fila[0], fila[1], fila[2], fila[4]);
  ac[0] = ac[1] = 0;
  // Excluida (falhou) sai e a estrita segue para a proxima.
  ex[0] = ex[1] = 1;
  nf = fonteauto_fila_o(FONTEAUTO_MELHOR, 6, -1, pt, NULL, ex, NULL, rk, FR_ORDEM_ESTRITA, 8, fila);
  CONFERE(nf == 4 && fila[0] == 2, "estrita sem o A: %d, %d", nf, fila[0]);
  ex[0] = ex[1] = 0;
  // Primeira da lista: com a ordem, o add-on mais cedo; sem ela, a ordem do addon.
  nf = fonteauto_fila_o(FONTEAUTO_PRIMEIRA, 6, -1, pt, NULL, ex, NULL, rk, FR_ORDEM_ESTRITA, 1, fila);
  CONFERE(nf == 1 && fila[0] == 0, "primeira estrita: %d", fila[0]);
  { int rk2[6] = { 1, 1, 0, 0, FR_ORDEM_SEM, FR_ORDEM_SEM };
    nf = fonteauto_fila_o(FONTEAUTO_PRIMEIRA, 6, -1, pt, NULL, ex, NULL, rk2, FR_ORDEM_ESTRITA, 8, fila);
    CONFERE(fila[0] == 2 && fila[1] == 3 && fila[2] == 0, "primeira com B antes: %d %d %d", fila[0], fila[1], fila[2]);
    nf = fonteauto_fila_o(FONTEAUTO_PRIMEIRA, 6, -1, pt, NULL, ex, NULL, rk2, FR_ORDEM_NAO, 8, fila);
    CONFERE(fila[0] == 0 && fila[1] == 1, "primeira sem ordem: %d %d", fila[0], fila[1]); }
  // O grupo continua mandando mais que a ordem.
  { signed char g[6] = { 1, 1, 0, 0, 0, 0 };
    nf = fonteauto_fila_o(FONTEAUTO_MELHOR, 6, -1, pt, NULL, ex, g, rk, FR_ORDEM_ESTRITA, 8, fila);
    CONFERE(fila[0] == 2 && fila[4] == 0, "grupo antes da ordem: %d [%d]", fila[0], fila[4]); }
  // Decidir cedo: so o A e o C responderam (B, o segundo da ordem, falta).
  memset(&p, 0, sizeof p);
  p.modo = FONTEAUTO_MELHOR; p.total = 3; p.preferida = -1; p.algumPendente = 1;
  { long p3[3] = { 20500, 41000, 40000 };
    int r3[3] = { 0, FR_ORDEM_SEM, FR_ORDEM_SEM };
    unsigned char e3[3] = { 0 }, a3[3] = { 0 }, b3[3] = { 0, 1, 1 };
    p.pontos = p3; p.excluida = e3; p.acimaTeto = a3; p.boa = b3; p.rank = r3;
    p.ordemUso = FR_ORDEM_ESTRITA; p.pendenteRankMin = 1;
    // A (rank 0) e a primeira da ordem: nada antes dele falta, decide ja, sem esperar o B.
    CONFERE(fonteauto_pode_decidir(&p) == 1, "estrita: A respondeu, B pendente deve decidir");
    // Sem a ordem estrita o A (nao boa) esperaria o prazo, pois o 4K do C e a candidata e B pende.
    p.ordemUso = FR_ORDEM_NAO; p.pendenteRankMin = 99;
    CONFERE(fonteauto_pode_decidir(&p) == 1, "sem ordem: a candidata e o 4K (boa)");
    // O primeiro da ordem ainda falta: espera.
    p.ordemUso = FR_ORDEM_ESTRITA; p.pendenteRankMin = 0;
    CONFERE(fonteauto_pode_decidir(&p) == 0, "estrita: add-on antes ainda falta");
    // Candidata fora da ordem e add-on da ordem pendente: espera tambem.
    { int r4[3] = { FR_ORDEM_SEM, FR_ORDEM_SEM, FR_ORDEM_SEM };
      p.rank = r4; p.pendenteRankMin = 1;
      CONFERE(fonteauto_pode_decidir(&p) == 0, "estrita: a ordem tem pendente e a candidata esta fora"); } }
  (void)bo;
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

  parcial();
  grupos();
  ordemAddons();

  if (falhas) { printf("fonteauto: %d falha(s)\n", falhas); return 1; }
  printf("fonteauto: tudo ok\n");
  return 0;
}
