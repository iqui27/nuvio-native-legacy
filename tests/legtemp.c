// Legenda temporaria (mudo / voltar alguns segundos): a regra pura.
#include "legtemp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void rajada(void) {
  // Seis toques de 10 s seguidos (60 s no total): ao passar do teto a legenda
  // sai e os toques seguintes NAO religam; so depois de passar do ponto de
  // origem um novo recuo curto vale de novo.
  LegTemp t;
  int i;
  legtemp_zerar(&t);
  assert(legtemp_busca(&t, 100, 90) == LEGTEMP_LIGAR);
  for (i = 1; i < 4; i++) assert(legtemp_busca(&t, 100 - 10 * i, 90 - 10 * i) == LEGTEMP_NADA);
  assert(legtemp_busca(&t, 60, 50) == LEGTEMP_DESLIGAR);
  assert(legtemp_busca(&t, 50, 40) == LEGTEMP_NADA && !t.motivos);
  assert(legtemp_quadro(&t, 60) == LEGTEMP_NADA);
  assert(legtemp_busca(&t, 60, 55) == LEGTEMP_NADA && !t.motivos);
  assert(legtemp_quadro(&t, 103) == LEGTEMP_NADA);
  assert(legtemp_busca(&t, 103, 95) == LEGTEMP_LIGAR);
}

static void gestoConfirmado(void) {
  LegTemp t;
  legtemp_zerar(&t);
  assert(legtemp_busca(&t, 100, 100 - LEGTEMP_VOLTA_MAX - 1) == LEGTEMP_NADA);
  assert(legtemp_busca(&t, 100, 100) == LEGTEMP_NADA && !t.motivos);
  assert(legtemp_busca(&t, 100, 90) == LEGTEMP_LIGAR && t.motivos == LEGTEMP_VOLTA);
  assert(legtemp_busca(&t, 90, 99) == LEGTEMP_NADA && t.motivos == LEGTEMP_VOLTA);
  assert(legtemp_busca(&t, 99, 100) == LEGTEMP_DESLIGAR && !t.motivos);
}

int main(void) {
  rajada();
  gestoConfirmado();
  LegTemp t;
  const char *o[3];

  // MUDO: liga ao mutar, desliga ao voltar o som; repetir o estado nao e evento.
  legtemp_zerar(&t);
  assert(legtemp_mudo(&t, 0) == LEGTEMP_NADA);
  assert(legtemp_mudo(&t, -1) == LEGTEMP_NADA);   // plataforma nao sabe
  assert(legtemp_mudo(&t, 1) == LEGTEMP_LIGAR);
  assert(legtemp_mudo(&t, 1) == LEGTEMP_NADA);
  assert(legtemp_mudo(&t, -1) == LEGTEMP_NADA && t.motivos == LEGTEMP_MUDO);
  assert(legtemp_quadro(&t, 9999) == LEGTEMP_NADA);   // posicao nao encerra o mudo
  assert(legtemp_mudo(&t, 0) == LEGTEMP_DESLIGAR && !t.motivos);

  // VOLTA: 10 s para tras liga ate passar do ponto de origem + folga.
  legtemp_zerar(&t);
  assert(legtemp_busca(&t, 100, 90) == LEGTEMP_LIGAR);
  assert(legtemp_quadro(&t, 90) == LEGTEMP_NADA);
  assert(legtemp_quadro(&t, 100) == LEGTEMP_NADA);
  assert(legtemp_quadro(&t, 100 + LEGTEMP_VOLTA_FOLGA - 0.1) == LEGTEMP_NADA);
  assert(legtemp_quadro(&t, 100 + LEGTEMP_VOLTA_FOLGA) == LEGTEMP_DESLIGAR);
  assert(legtemp_quadro(&t, 200) == LEGTEMP_NADA);

  // Fora da faixa "alguns segundos": ajuste fino, salto longo e avanco nao ligam.
  legtemp_zerar(&t);
  assert(legtemp_busca(&t, 100, 99) == LEGTEMP_NADA);
  assert(legtemp_busca(&t, 100, 100 - LEGTEMP_VOLTA_MAX - 1) == LEGTEMP_NADA);
  assert(legtemp_busca(&t, 100, 130) == LEGTEMP_NADA);
  assert(legtemp_busca(&t, 0.0 / 0.0, 10) == LEGTEMP_NADA);
  assert(!t.motivos);

  // Recuos seguidos somam e o fim continua no ponto do PRIMEIRO.
  assert(legtemp_busca(&t, 100, 90) == LEGTEMP_LIGAR);
  assert(legtemp_busca(&t, 91, 81) == LEGTEMP_NADA);
  assert(legtemp_quadro(&t, 95) == LEGTEMP_NADA);
  assert(legtemp_quadro(&t, 102) == LEGTEMP_DESLIGAR);
  // ...mas passando do teto a pessoa esta navegando: desliga.
  assert(legtemp_busca(&t, 100, 90) == LEGTEMP_LIGAR);
  assert(legtemp_busca(&t, 90, 100 - LEGTEMP_VOLTA_MAX - 1) == LEGTEMP_DESLIGAR);
  assert(legtemp_quadro(&t, 102) == LEGTEMP_NADA);   // passou do ponto: vale de novo
  // Pular para a frente do ponto de origem tambem encerra.
  assert(legtemp_busca(&t, 100, 90) == LEGTEMP_LIGAR);
  assert(legtemp_busca(&t, 92, 300) == LEGTEMP_DESLIGAR);

  // Os dois motivos juntos: so desliga quando o ULTIMO cai.
  legtemp_zerar(&t);
  assert(legtemp_mudo(&t, 1) == LEGTEMP_LIGAR);
  assert(legtemp_busca(&t, 100, 90) == LEGTEMP_NADA);
  assert(legtemp_quadro(&t, 103) == LEGTEMP_NADA && t.motivos == LEGTEMP_MUDO);
  assert(legtemp_busca(&t, 103, 93) == LEGTEMP_NADA);
  assert(legtemp_mudo(&t, 0) == LEGTEMP_NADA && t.motivos == LEGTEMP_VOLTA);
  assert(legtemp_quadro(&t, 105) == LEGTEMP_DESLIGAR);

  // Idiomas: preferencia, app, audio; sem vazios, "none", "und" nem repetidos.
  assert(legtemp_idiomas("por", "pt", "eng", o) == 3 && !strcmp(o[0], "por") && !strcmp(o[2], "eng"));
  assert(legtemp_idiomas("none", "pt", "eng", o) == 2 && !strcmp(o[0], "pt") && !strcmp(o[1], "eng"));
  assert(legtemp_idiomas("", NULL, "und", o) == 0);
  assert(legtemp_idiomas("pt", "PT", "pt", o) == 1);
  assert(legtemp_idiomas(NULL, "", "jpn", o) == 1 && !strcmp(o[0], "jpn"));

  puts("legtemp: ok");
  return 0;
}
