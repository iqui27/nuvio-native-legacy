// Tela ambiente ao pausar: a regra de quando a arte do titulo entra por baixo
// do painel de pausa (src/ambiente.c).
#include "../src/ambiente.h"
#include <stdio.h>

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %s %s\n", ok ? "ok    " : "FALHOU", o_que);
  if (!ok) falhas++;
}

int main(void) {
  Ambiente s = { 0, 0 };
  float a, b;

  printf("-- pedido da textura\n");
  confere("nao pede antes da hora", !ambiente_pedir(AMB_PEDIR_MS - 1, 1, 1));
  confere("pede antes de a arte entrar", AMB_PEDIR_MS < AMB_ESPERA_MS && ambiente_pedir(AMB_PEDIR_MS, 1, 1));
  confere("desligado nunca pede", !ambiente_pedir(60000, 0, 1));
  confere("sem arte nunca pede", !ambiente_pedir(60000, 1, 0));

  printf("-- entrada\n");
  confere("pausa curta: nada", ambiente_passo(&s, 5000, AMB_ESPERA_MS - 1, 1, 1) == 0.0f);
  confere("no instante em que comeca: alfa 0", ambiente_passo(&s, 10000, AMB_ESPERA_MS, 1, 1) == 0.0f);
  a = ambiente_passo(&s, 10000 + AMB_ENTRA_MS / 2, AMB_ESPERA_MS + AMB_ENTRA_MS / 2, 1, 1);
  confere("meio da entrada: metade", a > 0.49f && a < 0.51f);
  b = ambiente_passo(&s, 10000 + AMB_ENTRA_MS * 3 / 4, AMB_ESPERA_MS + AMB_ENTRA_MS * 3 / 4, 1, 1);
  confere("so cresce", b > a && b < 1.0f);
  confere("fim da entrada: cheia", ambiente_passo(&s, 10000 + AMB_ENTRA_MS, AMB_ESPERA_MS + AMB_ENTRA_MS, 1, 1) == 1.0f);
  confere("fica cheia", ambiente_passo(&s, 900000, 890000, 1, 1) == 1.0f);

  printf("-- textura atrasada ou despejada\n");
  ambiente_zerar(&s);
  confere("sem textura: nada, por mais que demore", ambiente_passo(&s, 40000, 30000, 1, 0) == 0.0f);
  confere("textura chegou tarde: entra do zero, sem estalo", ambiente_passo(&s, 41000, 31000, 1, 1) == 0.0f);
  a = ambiente_passo(&s, 41000 + AMB_ENTRA_MS / 2, 31000 + AMB_ENTRA_MS / 2, 1, 1);
  confere("e sobe pela mesma rampa", a > 0.49f && a < 0.51f);
  confere("despejada no meio: some", ambiente_passo(&s, 50000, 40000, 1, 0) == 0.0f);
  confere("voltou: entra de novo do zero", ambiente_passo(&s, 51000, 41000, 1, 1) == 0.0f);

  printf("-- desligar e relogio\n");
  ambiente_passo(&s, 60000, 50000, 1, 1);
  confere("ajuste desligado com a arte no ar: some", ambiente_passo(&s, 60001, 50001, 0, 1) == 0.0f);
  ambiente_zerar(&s);
  ambiente_passo(&s, 0xFFFFFF00u, AMB_ESPERA_MS, 1, 1);
  a = ambiente_passo(&s, 0xFFFFFF00u + AMB_ENTRA_MS, AMB_ESPERA_MS + AMB_ENTRA_MS, 1, 1);
  confere("a virada do relogio de 32 bits nao quebra a rampa", a == 1.0f);

  return falhas != 0;
}
