// POSTER FLUIDO: a arte que ENTRA na janela durante a rolagem nao pode ser
// recusada nem esperar o repouso.
//
// POR QUE ESTE TESTE EXISTE. O relato do dono na TCL (Android TV):
// "os posters ainda tao carregando quando eu mexo". A recusa do item 4 marcava
// TODO card fora da tela como nivel 2 — inclusive a faixa de corte do lado em
// que a rolagem ENTRA — e o lookahead so disparava depois de dois quadros
// parados. Com o D-pad segurado nao ha repouso: cada card so pedia a arte ao
// aparecer, e a tela mostrava esqueleto. Este teste simula a rolagem mantida:
// a arte do lado que entra tem de estar no cache ou em voo ANTES de ficar
// visivel, e o lado oposto continua recusado.
//
// O cache e exercitado sem rede e sem GPU: so o caminho do pedido.
#include "../src/sdlcompat.h"
#include "../src/tex_cache.c"
#include <assert.h>

#ifdef NV_POSTER_BEFORE
// A arvore anterior nao tem a direcao da rolagem nem o lookahead em
// movimento: os stubs mantem o teste compilando para ele FALHAR nas
// afirmacoes de comportamento (o nivel 3 nao existe -> recusa; o pedido de
// especulacao exige cache ocioso -> nao entra).
void tex_grade_velocidade(float p) { (void)p; }
int tex_lookahead_mover(TexLookahead *s) { (void)s; return 0; }
#endif

static void prepararApi(void) {
  memset(itens, 0, sizeof itens);
  nMax = 16;   // emVoo < nMax/3 - 2 = 3: espaco para a pre-busca rolar
  filaIni = filaFim = 0;
  decIni = decFim = 0;
  mtx = SDL_CreateMutex();
  cond = SDL_CreateCond();
  condDec = SDL_CreateCond();
  condLivre = SDL_CreateCond();
  rodando = 1;
  orcamento = 512L * 1024 * 1024;
#ifndef NV_POSTER_BEFORE
  gradeRolDir = 0.0f;
#endif
  pedidoFora = 0;
  assert(mtx && cond && condDec && condLivre);
}

static int ocupados(void) {
  int n = 0;
  for (int i = 0; i < nMax; i++) if (itens[i].estado != VAZIO) n++;
  return n;
}

static int naFila(void) { return filaIni != filaFim; }

static int indice(const char *p) {
  return acharIndice(p, hashCaminho(p));
}

static int pedir(const char *p) {
  tex_n_recusa = 0;
  int r = tex_obter(p);
  assert(r == 0);
  return tex_n_recusa;
}

int main(void) {
  prepararApi();
  assert(ocupados() == 0 && !naFila());

  // Rolagem mantida para a direita: a grade diz que esta em movimento e a
  // velocidade diz qual lado recebe a arte nova.
  tex_grade_movendo(1);
#ifndef NV_POSTER_BEFORE
  tex_grade_velocidade(2.0f);
#endif

  // 1) FORA DA TELA NO LADO QUE ENTRA (nivel 3): NUNCA recusa — o pedido sai
  //    na hora e o item fica em voo antes de o card ser visivel.
  tex_pedido_fora(3);
  assert(pedir("https://x/entrando.jpg") == 0);
  assert(ocupados() == 1 && naFila());
#ifndef NV_POSTER_BEFORE
  { int i = indice("https://x/entrando.jpg");
    assert(i >= 0 && itens[i].passageiro == 1); }
#endif
  tex_pedido_fora(0);

  // 2) FORA DA TELA NO LADO QUE SAI (nivel 2), em movimento: recusado.
  tex_pedido_fora(2);
  assert(pedir("https://x/saindo.jpg") == 1);
  assert(ocupados() == 1);
  tex_pedido_fora(0);

  // 3) O MESMO pedido do nivel 3 quando o card JA ESTA na tela (nivel 0):
  //    arte conhecida nunca e recusada — nada aqui muda.
  assert(pedir("https://x/entrando.jpg") == 0);

  // 4) LOOKAHEAD EM MOVIMENTO: uma direcao por quadro por chamador, e o
  //    pedido passa MESMO com a fila ocupada — a demanda rolando nunca esvazia.
#ifdef NV_POSTER_BEFORE
  // Sem o conserto, nao ha nem a funcao nem o nivel: o teste para aqui.
  assert(0);
#endif
  { TexLookahead s; memset(&s, 0, sizeof s);
    assert(tex_lookahead_mover(&s) == 1);
    assert(tex_lookahead_mover(&s) == 0); }
  assert(tex_lookahead_pedir("https://x/proximo.jpg", 300) == 1);
  { int i = indice("https://x/proximo.jpg");
    assert(i >= 0 && itens[i].lookahead == 1); }
  // Pedir de novo no quadro seguinte (a cada 2 em movimento): antes disso,
  // negado.
  assert(tex_lookahead_pedir("https://x/cedo.jpg", 300) == 0);
  quadroAtual += 2;
  assert(tex_lookahead_pedir("https://x/cedo.jpg", 300) == 1);

  // 5) GRADE PARADA de novo: o lookahead volta a exigir cache ocioso — com
  //    itens em voo, a especulacao espera, como sempre valeu.
  tex_grade_movendo(0);
#ifndef NV_POSTER_BEFORE
  tex_grade_velocidade(0.0f);
#endif
  assert(tex_lookahead_pedir("https://x/parado.jpg", 300) == 0);
  // E a recusa de repouso continua: fora da tela parado (nivel 2) recusa.
  tex_pedido_fora(2);
  assert(pedir("https://x/fora-parado.jpg") == 1);
  tex_pedido_fora(0);

  puts("posterfluido: lado que entra nunca recusa; lookahead rola junto");
  puts("posterfluido: PASS");
  return 0;
}
