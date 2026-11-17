// PASSO DO CH+/CH- DA LEGENDA (player.c, legSyncAtalho). Puro: sem SDL, sem
// globais, para tests/legsyncpasso.c simular o controle.
//
// Pedido do dono (17/11): um TOQUE anda 0,1 s; SEGURAR anda cada vez mais,
// "como na timeline principal". O firmware da TV entrega a tecla segurada como
// KEYDOWNs SEPARADOS com repeat=0 (o Smart Remote repete a cada ~100-117 ms,
// seekgesto.h), entao `repeat` do SDL nao basta: um KEYDOWN na MESMA direcao a
// ate LSP_SEGURA_GAP_MS do anterior conta como tecla segurada. A primeira
// repeticao do controle chega depois do atraso inicial (~400 ms), entao o
// segurar comeca como toque e so a partir dai acelera.
//
// A RAMPA fica em multiplos de 0,1 s: o total continua na grade do toque e da
// para voltar exatamente a 0 tocando. Trocar de direcao recomeca a rampa.
#ifndef NV_LEGSYNCPASSO_H
#define NV_LEGSYNCPASSO_H

#define LSP_TOQUE_MS       100
#define LSP_SEGURA_GAP_MS  170u

typedef struct {
  int ativo, dir, n;   // n = repeticoes seguidas da tecla segurada (0 = toque)
  unsigned ult;        // instante do ultimo KEYDOWN
} LegSyncPasso;

// Um KEYDOWN de CH+/CH- na direcao `dir` (+1/-1) no instante `agora` (ms).
// `repeticao` = a repeticao do proprio SDL (e->key.repeat), que tambem conta.
// Devolve o passo em ms (> 0), sem sinal.
static inline int legsyncpasso_tecla(LegSyncPasso *s, int dir, unsigned agora, int repeticao) {
  static const int rampa[] = { 100, 100, 200, 200, 300, 400, 500 };
  const int nr = (int)(sizeof rampa / sizeof rampa[0]);
  int segurando = s->ativo && s->dir == dir &&
                  (repeticao || agora - s->ult <= LSP_SEGURA_GAP_MS);
  s->n = segurando ? s->n + 1 : 0;
  s->ativo = 1; s->dir = dir; s->ult = agora;
  return rampa[s->n < nr ? s->n : nr - 1];
}

#endif
