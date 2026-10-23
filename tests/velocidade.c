// #202: velocidade de reproducao (src/velocidade.c): a lista, o passo sem dar
// a volta nas pontas, o rotulo e a conta do tempo de relogio que o "termina
// as" e a contagem do proximo episodio usam.
#include "../src/velocidade.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int perto(double a, double b) { return fabs(a - b) < 1e-9; }

// Compile the real Android menu gate with both audio output states.
static int audioEstado;
#define CB_GANHO_PASSTHROUGH 2
#define cacheboost_ganho_estado() audioEstado
#include "android_speed_gate.h"

static void android_speed(void) {
  audioEstado = 0;
  assert(!video_velocidade_bloqueada());
  audioEstado = CB_GANHO_PASSTHROUGH;
  assert(!video_velocidade_bloqueada());
}

// Pipeline simulado: a posicao anda `taxa` por segundo e so e publicada a cada
// 200 ms (o currentTime da LG; o Android e mais fino). Roda `seg` segundos a
// 60 Hz e devolve o ultimo evento diferente de VELMED_NADA.
static double relogio, posReal, posPub, proxPub;
static int rodar(VelMedidor *m, double taxa, double seg, int valido) {
  int r = VELMED_NADA, e; double fim = relogio + seg;
  for (; relogio < fim; relogio += 1.0 / 60.0) {
    if (valido) posReal += taxa / 60.0;
    if (relogio >= proxPub) { posPub = posReal; proxPub += 0.2; }
    e = velmed_passo(m, relogio, posPub, valido);
    if (e != VELMED_NADA) r = e;
  }
  return r;
}

int main(void) {
  char b[16];
  int i;
  android_speed();
  // A lista: curta, crescente, com o 1x dentro.
  assert(VEL_N == 6);
  for (i = 1; i < VEL_N; i++) assert(VEL_LISTA[i] > VEL_LISTA[i - 1]);
  assert(VEL_LISTA[vel_indice(VEL_NORMAL)] == VEL_NORMAL);
  assert(VEL_LISTA[0] == 75 && VEL_LISTA[VEL_N - 1] == 200);

  // Passo: um toque = um valor; nas pontas fica parado (sem volta).
  assert(vel_passo(100, 1) == 125);
  assert(vel_passo(125, 1) == 150);
  assert(vel_passo(150, -1) == 125);
  assert(vel_passo(100, -1) == 75);
  assert(vel_passo(75, -1) == 75);
  assert(vel_passo(200, 1) == 200);
  assert(vel_passo(150, 0) == 150);
  // Fora da lista anda a partir do mais proximo.
  assert(vel_indice(130) == vel_indice(125));
  assert(vel_passo(130, 1) == 150);
  assert(vel_passo(0, 1) == 100);
  { int v = 75, n = 0; while (v != 200) { v = vel_passo(v, 1); n++; } assert(n == VEL_N - 1); }

  // Rotulo com ponto (plrui_decimal troca pela virgula onde a lingua pede).
  vel_rotulo(b, sizeof b, 100); assert(!strcmp(b, "1x"));
  vel_rotulo(b, sizeof b, 75);  assert(!strcmp(b, "0.75x"));
  vel_rotulo(b, sizeof b, 125); assert(!strcmp(b, "1.25x"));
  vel_rotulo(b, sizeof b, 150); assert(!strcmp(b, "1.5x"));
  vel_rotulo(b, sizeof b, 175); assert(!strcmp(b, "1.75x"));
  vel_rotulo(b, sizeof b, 200); assert(!strcmp(b, "2x"));
  vel_rotulo(b, sizeof b, 130); assert(!strcmp(b, "1.3x"));
  vel_rotulo(b, 3, 125); assert(strlen(b) == 2);   // corta sem estourar

  // Tempo de relogio: 60 min de arquivo a 1,5x sao 40 min; a 0,75x, 80 min.
  assert(perto(vel_tempo_real(3600.0, 100), 3600.0));
  assert(perto(vel_tempo_real(3600.0, 150), 2400.0));
  assert(perto(vel_tempo_real(3600.0, 200), 1800.0));
  assert(perto(vel_tempo_real(3600.0, 75), 4800.0));
  assert(perto(vel_tempo_real(5.0, 125), 4.0));      // contagem final do proximo episodio
  assert(perto(vel_tempo_real(3600.0, 0), 3600.0));  // valor invalido = 1x
  assert(perto(vel_tempo_real(0.0, 150), 0.0));

  printf("velocidade: lista, passo, rotulo e tempo de relogio ok\n");

  // --- VelMedidor (#202, TCL 06/10) -----------------------------------------
  { VelMedidor m; int r;
    relogio = 0; posReal = posPub = 49.759; proxPub = 0;
    velmed_zerar(&m);
    assert(m.efetiva == 100 && m.pedida == 100);
    // 1x pedido e 1x tocando: nada a dizer.
    assert(rodar(&m, 1.0, 12.0, 1) == VELMED_NADA && m.efetiva == 100);

    // O CASO DA TCL: 1,5x pedido, "plataforma ok", o pipeline segue a 1x
    // (passthrough). Nunca vira efetiva; duas janelas a 1x desfazem o pedido.
    velmed_pedir(&m, 150);
    assert(m.efetiva == 100);                      // nao confia no pedido
    r = rodar(&m, 1.0, 6.0, 1);
    assert(r == VELMED_NADA && m.efetiva == 100);  // uma janela so nao basta
    r = rodar(&m, 1.0, 6.0, 1);
    assert(r == VELMED_NAO_ANDOU && m.efetiva == 100);

    // Plataforma que acelera de verdade: confirma na primeira janela.
    velmed_zerar(&m); relogio = 100; proxPub = 100;
    velmed_pedir(&m, 150);
    r = rodar(&m, 1.5, 6.0, 1);
    assert(r == VELMED_CONFIRMOU && m.efetiva == 150);
    assert(rodar(&m, 1.5, 12.0, 1) == VELMED_NADA && m.efetiva == 150);   // segue confirmada
    // 0,75x e 2x tambem se distinguem de 1x.
    velmed_pedir(&m, 75);  assert(rodar(&m, 0.75, 6.0, 1) == VELMED_CONFIRMOU && m.efetiva == 75);
    velmed_pedir(&m, 200); assert(rodar(&m, 2.0, 6.0, 1) == VELMED_CONFIRMOU && m.efetiva == 200);
    velmed_pedir(&m, 125); assert(rodar(&m, 1.25, 6.0, 1) == VELMED_CONFIRMOU && m.efetiva == 125);
    velmed_pedir(&m, 175); assert(rodar(&m, 1.75, 6.0, 1) == VELMED_CONFIRMOU && m.efetiva == 175);

    // Confirmada e depois o pipeline volta sozinho a 1x (uma busca que zera a
    // taxa na TV, por exemplo): duas janelas a 1x e cai para 100.
    velmed_pedir(&m, 150); rodar(&m, 1.5, 6.0, 1); assert(m.efetiva == 150);
    r = rodar(&m, 1.0, 16.0, 1);   // a janela que pega a troca e mista e nao conta
    assert(r == VELMED_NAO_ANDOU && m.efetiva == 100);

    // Pausa e buffering fecham a janela: 3 s tocando + pausa + 3 s nao medem
    // nada (nenhuma janela de 5 s inteira), e a pausa nao vira "nao andou".
    velmed_zerar(&m); velmed_pedir(&m, 150);
    assert(rodar(&m, 1.5, 3.0, 1) == VELMED_NADA);
    assert(rodar(&m, 0.0, 20.0, 0) == VELMED_NADA);
    assert(rodar(&m, 1.5, 3.0, 1) == VELMED_NADA && m.efetiva == 100);
    assert(rodar(&m, 1.5, 4.0, 1) == VELMED_CONFIRMOU);

    // Travada no meio da janela sem aviso de buffering (medida nem 1,5x nem
    // 1x): nao conta para nenhum lado.
    velmed_zerar(&m); velmed_pedir(&m, 200);
    r = rodar(&m, 2.0, 2.0, 1); r = rodar(&m, 0.0, 1.6, 1); r = rodar(&m, 2.0, 1.6, 1);
    assert(r == VELMED_NADA && m.efetiva == 100 && m.uns == 0);

    // Salto de posicao (busca) recomeca a janela em vez de medir 60x.
    velmed_zerar(&m); velmed_pedir(&m, 150);
    rodar(&m, 1.5, 2.0, 1);
    posReal += 300.0;
    assert(rodar(&m, 1.5, 4.0, 1) == VELMED_NADA && m.efetiva == 100);
    assert(rodar(&m, 1.5, 2.0, 1) == VELMED_CONFIRMOU);

    // Voltar a 1x vale na hora (e o estado em que o pipeline nasce).
    velmed_pedir(&m, 100); assert(m.efetiva == 100);
    assert(rodar(&m, 1.0, 12.0, 1) == VELMED_NADA); }
  printf("velocidade: medidor da velocidade efetiva ok\n");
  return 0;
}
