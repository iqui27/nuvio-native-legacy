// #387: um quadro lento entre o KEYDOWN e o KEYUP do OK nao pode virar
// pressao longa. O segurar e medido no instante da ultima leitura de eventos.
#include "segurar.h"
#include "layout.h"
#include <stdio.h>

static int falhas;
static void caso(int obtido, int quer, const char *nome) {
  if (obtido != quer) { printf("FAIL %s: %d (quer %d)\n", nome, obtido, quer); falhas++; }
  else printf("ok   %s\n", nome);
}

int main(void) {
  Uint32 t, disparo = 0;
  // Segurando de verdade: quadros de 16 ms ate passar o limiar.
  for (t = 1000; t <= 1000 + NV_HOLD_MS + 32 && !disparo; t += 16) {
    segurar_quadro(t);
    if (segurar_venceu(t, 1000, "teste")) disparo = t;
  }
  caso(disparo >= 1000 + NV_HOLD_MS && disparo < 1000 + NV_HOLD_MS + 16, 1, "segurou com quadros normais dispara no limiar");
  // Pausa DENTRO do quadro (revisao do Codex): leitura em 2016, KEYDOWN lido em
  // 2017, a subida de textura leva 900 ms e a tela pergunta com o relogio em
  // 2916. O KEYUP de um toque curto esta na fila e ainda nao foi lido.
  segurar_quadro(2016);
  caso(segurar_venceu(2916, 2017, "teste"), 0, "pausa dentro do quadro nao dispara");
  // Pausa ENTRE quadros: a proxima leitura e em 2930; se o OK ainda esta
  // afundado depois dela, e segurar de verdade.
  segurar_quadro(2930);
  caso(segurar_venceu(2930, 2017, "teste"), 1, "segurando depois da proxima leitura dispara");
  // Duas telas no mesmo quadro veem a mesma medida.
  caso(segurar_venceu(3500, 2017, "teste"), 1, "segunda tela no mesmo quadro concorda");
  // Quadros sempre lentos (200 ms): o segurar de verdade ainda dispara.
  disparo = 0;
  for (t = 5000; t <= 6200 && !disparo; t += 200) {
    segurar_quadro(t);
    if (segurar_venceu(t, 5000, "teste")) disparo = t;
  }
  caso(disparo && disparo <= 5000 + NV_HOLD_MS + 200, 1, "quadros de 200 ms nao impedem o segurar");
  // Nao armado nunca dispara.
  caso(segurar_venceu(9000, 0, "teste"), 0, "sem armar nao dispara");
  if (falhas) { printf("FAIL #387: %d caso(s)\n", falhas); return 1; }
  puts("PASS #387: o segurar e medido na ultima leitura de eventos.");
  return 0;
}
