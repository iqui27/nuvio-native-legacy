// PEDIDO DE AMIZADE NA ILHA (06/10, relato do dono: "nao ta mostrando na
// ilha do relogio"). Estado real de ilha.c + ilhasinais.c; a caixa de pedidos
// (recomenda.c) e o "ja dito" em disco (avisodisp.c) dublados em memoria.
// Sem janela, GL, rede ou disco.
//
// O que se prova:
//   1. um pedido novo vai para a pilula e so vira "dito" DEPOIS de aparecer;
//   2. debaixo do painel (ilha coberta) o prazo nao corre e nada e "dito";
//   3. descartado pela fila cheia de erros, ele volta na sondagem seguinte
//      (antes: marcado no disco na hora do envio e perdido para sempre);
//   4. dito uma vez, nao volta — nem depois de reiniciar;
//   5. respondido na aba Amigos antes de aparecer, sai da pilula;
//   6. varios juntos: um "N pedidos de amizade", e os N viram ditos.
//
//   bash tests/ilha_pedidos.sh
#include "../src/ilha.c"
#include "../src/ilhasinais.c"
#include <assert.h>

static Uint32 agora = 1000;
Uint32 SDL_GetTicks(void) { return agora; }
const char *i18n(const char *s) { return s; }
const char *rec_genero_rotulo(int g) { (void)g; return "Drama"; }
float gfx_tex_aspect_atual;
float gfx_card_forcar_cover_atual;
int txt_pendentes;
int anim_politica_reduzida;
// Citados como ponteiro em ilhasinais_iniciar (que nao roda aqui).
void rede_saude_nota(int c, const char *u) { (void)c; (void)u; }
void rede_hosts_nota(const char *u, int c, int h, unsigned ms) { (void)u; (void)c; (void)h; (void)ms; }

static RecPessoa caixa[REC_PEDIDOS_MAX];
static int nCaixa;
int recomenda_n_pedidos(void) { return nCaixa; }
int recomenda_pedido(int i, RecPessoa *s) {
  if (i < 0 || i >= nCaixa) return 0;
  *s = caixa[i]; return 1;
}
int recomenda_aceitar(const char *p) { (void)p; return 1; }
int recomenda_recusar(const char *p) { (void)p; return 1; }
static char ditas[32][80];
static int nDitas;
int avisodisp_tem(const char *c) {
  int i;
  for (i = 0; i < nDitas; i++) if (!strcmp(ditas[i], c)) return 1;
  return 0;
}
void avisodisp_por(const char *c) { if (!avisodisp_tem(c) && nDitas < 32) snprintf(ditas[nDitas++], 80, "%s", c); }

static void chegaPedido(const char *pub, const char *apelido) {
  memset(&caixa[nCaixa], 0, sizeof caixa[0]);
  snprintf(caixa[nCaixa].pub, sizeof caixa[0].pub, "%s", pub);
  snprintf(caixa[nCaixa].apelido, sizeof caixa[0].apelido, "%s", apelido);
  nCaixa++;
}
// Um quadro da ilha (o comeco de ilha_desenharCorpo_), `cob` = painel por cima.
static Uint32 ultimo;
static void quadro(Uint32 ms, int cob) {
  Uint32 antes = ultimo;
  agora += ms;
  ultimo = agora;
  coberta = cob;
  vezNaTela(agora, antes);
  if (temCur && curAte && !coberta && (Sint32)(agora - curAte) >= 0) { proximo(); vezNaTela(agora, antes); }
  coberta = 0;
}
static void segundos(int s, int cob) { int i; for (i = 0; i < s * 10; i++) quadro(100, cob); }
static void limpaIlha(void) { while (temCur) proximo(); nFila = 0; nMostrados = 0; }

int main(void) {
  // 1. Chega um pedido: vai para a pilula, mas ainda nao e "dito".
  chegaPedido("pubA", "cine-ana");
  pedidosDeAmizade();
  assert(ilha_tem("pedido:pubA"));
  assert(!avisodisp_tem("pedido:pubA"));
  // A sondagem seguinte, antes de aparecer, nao duplica nem marca.
  pedidosDeAmizade();
  assert(!avisodisp_tem("pedido:pubA"));

  // 2. Painel de Salvos por cima por 20 s: o prazo (6 s) nao corre.
  segundos(20, 1);
  assert(ilha_tem("pedido:pubA"));
  pedidosDeAmizade();
  assert(!avisodisp_tem("pedido:pubA"));
  // O painel fecha: o aviso aparece, e a sondagem seguinte grava "dito".
  quadro(16, 0);
  pedidosDeAmizade();
  assert(avisodisp_tem("pedido:pubA"));
  segundos(7, 0);
  assert(!ilha_tem("pedido:pubA"));
  printf("ok: dito so depois de aparecer, nada vence debaixo do painel\n");

  // 3. Fila cheia de erros: o pedido e descartado sem aparecer e volta.
  limpaIlha();
  { int k; char c[16];
    for (k = 0; k < 7; k++) {
      snprintf(c, sizeof c, "erro%d", k);
      ilha_avisar(c, ILHA_ERRO, "x", c, 4000u, 0);
    } }
  chegaPedido("pubB", "rafa");
  pedidosDeAmizade();
  assert(!ilha_tem("pedido:pubB"));          // a fila nao tinha lugar
  assert(!avisodisp_tem("pedido:pubB"));     // e nada foi marcado
  limpaIlha();
  pedidosDeAmizade();                        // sondagem seguinte (2 s)
  assert(ilha_tem("pedido:pubB"));
  quadro(16, 0);
  pedidosDeAmizade();
  assert(avisodisp_tem("pedido:pubB"));
  printf("ok: descartado pela fila volta na sondagem seguinte\n");

  // 4. Reiniciar o app (RAM zerada, disco fica): nada volta.
  limpaIlha();
  nPend = 0;
  pedidosDeAmizade();
  assert(!ilha_tem("pedido:pubA") && !ilha_tem("pedido:pubB") && !temCur);
  printf("ok: dito nao volta depois de reiniciar\n");

  // 5. Respondido na aba Amigos antes de aparecer: sai da pilula.
  ilha_avisar("erro-x", ILHA_ERRO, "x", "erro na frente", 4000u, 0);
  quadro(16, 0);                              // o erro esta na tela
  chegaPedido("pubC", "lu");
  pedidosDeAmizade();
  assert(ilha_tem("pedido:pubC"));
  nCaixa--;                                   // aceito na aba
  pedidosDeAmizade();
  assert(!ilha_tem("pedido:pubC"));
  assert(!avisodisp_tem("pedido:pubC"));
  printf("ok: respondido antes de aparecer sai da pilula\n");

  // 6. Varios juntos: um aviso so, e os dois viram ditos quando ele aparece.
  limpaIlha();
  chegaPedido("pubD", "dani");
  chegaPedido("pubE", "edu");
  pedidosDeAmizade();
  assert(ilha_tem("pedidos") && !ilha_tem("pedido:pubD"));
  assert(!avisodisp_tem("pedido:pubD") && !avisodisp_tem("pedido:pubE"));
  quadro(16, 0);
  pedidosDeAmizade();
  assert(avisodisp_tem("pedido:pubD") && avisodisp_tem("pedido:pubE"));
  printf("ok: varios juntos viram um aviso e todos ficam ditos\n");

  printf("ilha_pedidos: OK\n");
  return 0;
}
