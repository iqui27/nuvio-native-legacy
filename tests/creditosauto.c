#include "creditosauto.h"
#include <assert.h>
#include <stdio.h>

static int decidir(CreditosAuto *s, IntroTrecho *v, int n, double pos,
                   double dur, int filme, int ligado, int elegivel, double *fim) {
  return creditosauto_decidir(s, v, n, pos, dur, filme, ligado, elegivel, fim);
}

int main(void) {
  CreditosAuto s;
  IntroTrecho v[] = { { 1400.0, 1440.0, INTRO_CREDITOS }, { 1480.0, 1490.0, INTRO_CREDITOS } };
  double fim = -1.0;
  creditosauto_zerar(&s);
  assert(!decidir(&s, v, 2, 1401.0, 1500.0, 0, 0, 1, &fim)); // opcao desligada
  assert(!decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 0, &fim)); // pausa/carga/folha
  assert(s.nUsados == 0 && fim == -1.0);
  assert(!decidir(&s, v, 2, 1399.0, 1500.0, 0, 1, 1, &fim));
  assert(decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 1, &fim));
  assert(fim == 1440.0); // preserva os 40 s da cena depois dos primeiros creditos
  assert(!decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 1, &fim)); // seek atrasado/falhou
  assert(creditosauto_aguardar(&s, 1440.0, 1, 0)); // destino otimista nao prova seek concluido
  assert(creditosauto_aguardar(&s, 1450.0, 0, 0)); // pausa nao confirma progresso
  assert(creditosauto_aguardar(&s, NAN, 1, 0));
  assert(!creditosauto_aguardar(&s, 1450.0, 1, 0));
  assert(!decidir(&s, v, 2, 1450.0, 1500.0, 0, 1, 1, &fim)); // cena entre dois trechos
  assert(decidir(&s, v, 2, 1480.0, 1500.0, 0, 1, 1, &fim) && fim == 1490.0);
  creditosauto_recuar(&s, 1490.0, 1480.0);
  assert(creditosauto_aguardar(&s, 1480.0, 1, 0)); // destino otimista do recuo
  assert(creditosauto_aguardar(&s, 1490.0, 1, 1)); // tempo/fim velho nao reativa Next
  assert(!creditosauto_aguardar(&s, 1480.5, 1, 0)); // progresso depois do recuo

  IntroTrecho ateFim = { 1400.0, 1500.0, INTRO_CREDITOS };
  creditosauto_zerar(&s);
  assert(decidir(&s, &ateFim, 1, 1401.0, 1500.0, 0, 1, 1, &fim));
  assert(creditosauto_aguardar(&s, 1500.0, 1, 0)); // Next ainda nao pode avancar
  assert(!creditosauto_aguardar(&s, 1500.0, 0, 1)); // fim nativo libera Next

  creditosauto_zerar(&s);
  creditosauto_recuar(&s, 1430.0, 1390.0); // marcadores ainda nao chegaram
  assert(creditosauto_aguardar(&s, 1430.0, 1, 0));
  assert(!creditosauto_aguardar(&s, 1390.5, 1, 0));
  assert(!decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 1, &fim));
  assert(decidir(&s, v, 2, 1481.0, 1500.0, 0, 1, 1, &fim)); // outro trecho continua elegivel
  creditosauto_zerar(&s);
  creditosauto_recuar(&s, 1400.0, 1390.0); // recuo no instante em que os creditos comecam
  assert(!creditosauto_aguardar(&s, 1390.5, 1, 0));
  assert(!decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 1, &fim));
  creditosauto_zerar(&s);
  creditosauto_recuar(&s, 100.0, 80.0); // recuo longe dos creditos
  assert(!creditosauto_aguardar(&s, 80.5, 1, 0));
  assert(decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 1, &fim));
  creditosauto_recuar(&s, 1450.0, 1400.0);
  assert(!decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 1, &fim));

  IntroTrecho invalidos[] = {
    { 1400.0, 0.0, INTRO_CREDITOS }, { 1400.0, 1400.0, INTRO_CREDITOS },
    { 1400.0, 1501.0, INTRO_CREDITOS }, { -1.0, 1440.0, INTRO_CREDITOS },
    { 100.0, 1440.0, INTRO_CREDITOS }, { 1400.0, NAN, INTRO_CREDITOS },
    { NAN, 1440.0, INTRO_CREDITOS }, { 1400.0, INFINITY, INTRO_CREDITOS },
    { 1400.0, 1440.0, INTRO_ABERTURA }, { 1400.0, 1440.0, INTRO_RESUMO }
  };
  for (unsigned i = 0; i < sizeof invalidos / sizeof *invalidos; i++) {
    creditosauto_zerar(&s);
    assert(!decidir(&s, invalidos + i, 1, 1401.0, 1500.0, 0, 1, 1, &fim));
  }
  IntroTrecho cedo = { 200.0, 240.0, INTRO_CREDITOS };
  creditosauto_zerar(&s);
  assert(!decidir(&s, &cedo, 1, 201.0, 1500.0, 1, 1, 1, &fim)); // filme: nao antes da metade
  assert(!decidir(&s, v, 2, 1401.0, 0.0, 0, 1, 1, &fim));
  assert(!decidir(&s, v, 2, NAN, 1500.0, 0, 1, 1, &fim));
  assert(!decidir(&s, v, 2, 1401.0, INFINITY, 0, 1, 1, &fim));
  assert(!decidir(&s, v, 2, 1440.0, 1500.0, 0, 1, 1, &fim)); // fim nao faz parte do trecho
  assert(!decidir(&s, v, 2, 1401.0, 1600.0, 0, 1, 1, &fim)); // corte/duracao mudou
  creditosauto_zerar(&s);
  creditosauto_fonte(&s, "https://example.test/corte-a"); // opcao desligada/carregando
  assert(!decidir(&s, v, 2, 1401.0, 1500.0, 0, 0, 0, &fim));
  creditosauto_fonte(&s, "https://example.test/corte-b");
  assert(!decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 1, &fim));
  creditosauto_zerar(&s); // titulo novo permite outra tentativa
  creditosauto_fonte(&s, "https://example.test/corte-b");
  creditosauto_fonte(&s, "https://example.test/corte-b");
  assert(decidir(&s, v, 2, 1401.0, 1500.0, 0, 1, 1, &fim));
  puts("creditosauto: ok (limites, cenas, recuo, fonte e uma tentativa por trecho)");
  return 0;
}
