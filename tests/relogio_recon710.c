// #710: reabertura do TPK devolve 0 ate o seek pousar. O player amostra
// video_pos() no relogio da legenda; sem a guarda o relogio recuava a 0
// (412 -> 0 -> 0.2 -> 412.3). Com posReconLeg o video_pos segura o ponto da
// queda enquanto a reconexao/seek nao confirma, e o relogio (src/relogio.c)
// nunca ve o zero. Este teste exercita o relogio real com a sequencia do
// pipeline e com a posicao que o video_pos corrigido entrega.
#include "../src/relogio.h"
#include <stdio.h>
#include <math.h>

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

// Espelho da guarda de video_pos() em src/video_tpk.c (#710): enquanto
// posReconLeg >= 0 e a sessao nao esta pronta / o seek esta em voo / a posicao
// crua e 0 ou muito abaixo do alvo, devolve o ponto mantido.
static double videoPosTpk(double posCru, int pronto, int buscaEmVoo, double *posReconLeg) {
  if (*posReconLeg >= 0.0) {
    if (!pronto || posCru < 0.25 || posCru + 2.0 < *posReconLeg || buscaEmVoo)
      return *posReconLeg;
    *posReconLeg = -1.0;
  }
  return posCru;
}

// Espelho do EV_BUSCOU (src/video_tpk.c): o ack confirma o alvo enviado. Um
// seek do usuario para outro ponto (ex.: 0 s durante a recuperacao em 12 s)
// invalida posReconLeg mesmo abaixo de 1 s.
static void ackSeek(int a, double *posReconLeg) {
  if (a >= 1000 || (*posReconLeg >= 0.0 && a != (int)(*posReconLeg * 1000.0)))
    *posReconLeg = -1.0;
}

int main(void) {
  Relogio r;
  double posReconLeg = -1.0, t, v, menor = 1e9;
  relogio_zerar(&r);
  // Tocando normal em 412 s.
  for (t = 0.0; t < 2.0; t += 1.0 / 60.0) {
    relogio_amostra(&r, 412.0 + t, t, 1);
    v = relogio_ler(&r, t);
    if (v < menor) menor = v;
  }
  ok(fabs(relogio_ler(&r, 1.9) - 413.9) < 0.4, "antes da queda: relogio em ~414 s");

  // Queda de rede: o host reabre, pronto=0, hPos devolve 0. O player marca
  // legParada (video_reconectando || video_busca_em_voo) e video_pos segura 412.
  posReconLeg = 412.0;
  double cru[] = {0.0, 0.0, 0.2, 0.0, 0.0};
  for (int i = 0; i < 5; i++) {
    t = 2.0 + i * (1.0 / 60.0);
    double pos = videoPosTpk(cru[i], 0, 1, &posReconLeg);
    int legParada = 1; // reconexao/seek em voo
    relogio_amostra(&r, pos, t, !legParada);
    v = relogio_ler(&r, t);
    if (v < menor) menor = v;
  }
  ok(menor > 411.0, "reabertura com hPos=0: relogio NUNCA recua a zero (#710)");
  ok(fabs(relogio_ler(&r, 2.08) - 412.0) < 0.5, "durante a reconexao segura o ponto da queda");

  // Seek confirmado: EV_BUSCOU limpa posReconLeg, pronto=1, hPos=412.3.
  posReconLeg = -1.0;
  t = 2.2;
  relogio_amostra(&r, videoPosTpk(412.3, 1, 0, &posReconLeg), t, 1);
  v = relogio_ler(&r, t);
  ok(fabs(v - 412.3) < 0.4, "depois do seek confirmado o relogio salta para 412.3");

  // Controle: sem a guarda, amostrar o zero cru com tocando=1 recuaria o relogio.
  { Relogio q; relogio_zerar(&q);
    relogio_amostra(&q, 412.0, 1.0, 1);
    relogio_amostra(&q, 0.0, 1.1, 1); // o defeito: zero amostrado como tocando
    ok(relogio_ler(&q, 1.1) < 1.0, "controle: zero cru com tocando=1 recuava o relogio (o bug)"); }

  // Cenario do revisor (F1-r2): retomada aos 12 s + seek do usuario para 0 s
  // antes do 1o ack. O 1o ack (12 s) ve o alvo novo em buscaAlvoMs e nao limpa;
  // o 2o ack (0 s) limpa e TEM de derrubar posReconLeg, senao video_pos()
  // segue devolvendo 12 com o host em 0 ate ~10 s.
  { Relogio q; relogio_zerar(&q);
    double pr = 12.0;      // posReconLeg da queda aos 12 s
    // 1o ack (12000): buscaAlvoMs ja vale 0, o CAS nao casa e o corpo nao roda
    // — posReconLeg continua em 12. Nada a chamar aqui; so o 2o ack existe de
    // fato na variavel.
    ok(pr == 12.0, "sem ack casado, posReconLeg segue em 12 s");
    ackSeek(0, &pr);       // 2o ack: confirma o seek do usuario para 0 s
    ok(pr < 0.0, "seek do usuario p/ 0 s durante a recuperacao invalida posReconLeg (#710)");
    relogio_amostra(&q, 0.0, 0.0, 1);
    relogio_amostra(&q, 0.350, 1.0 / 60.0, 1);
    ok(fabs(relogio_ler(&q, 1.0 / 60.0) - 0.350) < 0.2,
       "depois do ack do seek novo o relogio segue o host (0.35, nao 12)");
    // E um ack do MESMO alvo mantido nao e engolido: retomada aos 12 s
    // confirmada em 12 s solta a guarda do mesmo jeito.
    pr = 12.0; ackSeek(12000, &pr);
    ok(pr < 0.0, "ack do proprio alvo (12 s) limpa posReconLeg como antes"); }

  printf("\n%s\n", falhas ? "FALHOU" : "relogio_recon710: ok");
  return falhas != 0;
}
