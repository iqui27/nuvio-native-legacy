// Relogio interpolado da legenda (#92). Simula o pipeline da C9: currentTime
// a cada ~200 ms, entregue com 0..47 ms de atraso (o intervalo medido no log
// foi 197..247 ms), lido pelo laco de quadro a 60 Hz.
#include "../src/relogio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

int main(void) {
  Relogio r; double t, proxAmostra = 0.0, amostraPos = 0.0, entrega = -1;
  double pior = 0, piorDegrau = 0, anterior = -1; int recuos = 0, n = 0;
  double base = 248.393;           // posicao do video quando o relogio nasce
  srand(92);
  relogio_zerar(&r);
  // [1] 60 s tocando
  for (t = 0.0; t < 60.0; t += 1.0 / 60.0) {
    double verdade = base + t, v;
    if (t >= proxAmostra) {
      // o pipeline carimba a posicao em proxAmostra e entrega com atraso
      amostraPos = base + proxAmostra;
      entrega = proxAmostra + (rand() % 48) / 1000.0;
      proxAmostra += 0.2;
    }
    if (entrega >= 0 && t >= entrega) { relogio_amostra(&r, amostraPos, t, 1); entrega = -1; }
    else relogio_amostra(&r, r.temAmostra ? r.ultPos : base, t, 1);
    v = relogio_ler(&r, t);
    if (t > 3.0) {
      double e = fabs(v - verdade);
      if (e > pior) pior = e;
      if (anterior >= 0 && v - anterior > piorDegrau) piorDegrau = v - anterior;
      if (anterior >= 0 && v < anterior) recuos++;
      n++;
    }
    anterior = v;
  }
  printf("    %d quadros: erro max %.1f ms, maior degrau %.1f ms, recuos %d\n",
         n, pior * 1000, piorDegrau * 1000, recuos);
  ok(pior < 0.020, "erro contra a posicao real < 20 ms (antes: ate 247 ms)");
  ok(piorDegrau < 0.040, "sem degrau de 200 ms: maior passo < 40 ms");
  ok(recuos == 0, "nunca anda para tras tocando");

  // [2] seek para frente: o relogio segue o salto na primeira amostra
  relogio_amostra(&r, 900.0, 60.1, 1);
  ok(fabs(relogio_ler(&r, 60.1) - 900.0) < 0.001, "seek para 900 s: relogio pula junto");
  relogio_amostra(&r, 900.2, 60.3, 1);
  ok(fabs(relogio_ler(&r, 60.4) - 900.3) < 0.01, "depois do seek volta a interpolar");
  // [3] seek para tras
  relogio_amostra(&r, 100.0, 60.5, 1);
  ok(fabs(relogio_ler(&r, 60.5) - 100.0) < 0.001, "seek para tras: relogio recua de fato");
  // [4] pausa: sem previsao
  relogio_amostra(&r, 100.2, 60.7, 0);
  ok(fabs(relogio_ler(&r, 65.0) - 100.2) < 0.001, "pausado: fica na posicao do pipeline");
  // [5] buffering: pipeline para de mandar; teto de 350 ms
  relogio_amostra(&r, 100.2, 70.0, 1);
  relogio_amostra(&r, 100.4, 70.2, 1);
  ok(relogio_ler(&r, 75.0) <= 100.4 + 0.35 + 1e-9, "sem amostra (buffering): para 350 ms depois");
  printf("\n%s\n", falhas ? "FALHOU" : "relogio: ok");
  return falhas != 0;
}
