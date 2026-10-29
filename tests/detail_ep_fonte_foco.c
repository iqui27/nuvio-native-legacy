// ISSUE #513, lado do pedido: com "Escolher a fonte ao reproduzir" ligado, o
// toque curto no card de episodio pede a folha de fontes E TRAVA o episodio
// do card. A folha rouba o foco do teclado — sem a trava, a escolha nela lia
// detail_ep_foco DEPOIS, com o foco ja fora da fileira de episodios, e o
// player montava o alvo do episodio EM ANDAMENTO (ci->temporada/episodio),
// nao do card que a pessoa tinha apertado.
//
//   1. toque curto no card do episodio 3 (progresso no 2): pede FOLHA e trava
//      T1E3 — nao reproducao, nao o episodio em andamento;
//   2. detail_pediu_fontes consome a trava UMA vez e limpa (sem vazar para a
//      folha seguinte, a do titulo);
//   3. com o ajuste desligado o mesmo toque pede REPRODUCAO e nao trava nada.
//
// Sem janela e sem rede — a mesma receita de tests/detail_epcard.c.
#include "nvtmp.h"
#include "dados.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/detail.c"

static char dir[NVT_MAX], caminho[700];
static void gravar(const char *txt) {
  FILE *f = fopen(caminho, "w");
  assert(f);
  fputs(txt, f);
  fclose(f);
  ajustes_dir(dir);
}

static void okCurto(void) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  detail_evento(&e);
  e.type = SDL_KEYUP;
  detail_evento(&e);   // solta no mesmo instante: toque curto
}

int main(void) {
  CatItem c;
  CatEp eps[4];
  int i, t, e;

  nvt_set(dir, sizeof dir, "/nuvio-detail-epfoco-XXXXXX");
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);

  // Serie em "Continuar assistindo": o progresso aponta T1E2; a pessoa
  // apertou no card do T1E3.
  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Série de Ensaio");
  snprintf(c.imdb, sizeof c.imdb, "tt0000513");
  c.progresso = 40; c.temporada = 1; c.episodio = 2;
  c.temporadas[0] = 1; c.nTemporadas = 1;
  cat_definir_tudo(&c, 1, NULL, 0); idx = 0;
  memset(eps, 0, sizeof eps);
  for (i = 0; i < 4; i++) {
    eps[i].temporada = 1;
    eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "Episódio %d", i + 1);
  }
  cat_definir_episodios(0, eps, 4);

  // "Escolher a fonte ao reproduzir" LIGADO (V_LIGA 0).
  gravar("escolherFonteManual 0\n");
  assert(ajustes_fonte_manual());

  aberto = 1; saindo = 0; nivel = 1;
  foco.fileira = SEC_EPISODIOS; foco.coluna = 2;   // card do T1E3
  for (i = 0; i < 30; i++) detail_atualizar(1.0f / 60.0f, 1000 + i * 16);
  assert(foco.nColunas[SEC_EPISODIOS] == 4);

  // 1. Toque curto no T1E3: folha, com o episodio do card travado.
  okCurto();
  assert(pedFontes && !pedReproduzir);
  assert(detail_pediu_fontes());
  foco.fileira = SEC_TEMPORADAS;
  assert(detail_ep_foco(&t, &e) && t == 1 && e == 2);
  assert(detail_pediu_fontes_ep(&t, &e));
  assert(t == 1 && e == 3);

  // 2. Consumida: a segunda leitura (a folha do titulo, sem card atras)
  //    nao herda a trava velha.
  assert(!detail_pediu_fontes_ep(&t, &e) || (t == 0 && e == 0));
  t = e = 0;
  assert(!detail_pediu_fontes_ep(&t, &e));
  assert(t == 0 && e == 0);
  puts("ok: sheet request latches the card's episode, once");

  // 3. Com o ajuste no automatico, o mesmo toque continua "toca sempre" e
  //    nao trava episodio nenhum.
  gravar("escolherFonteManual 1\n");
  assert(!ajustes_fonte_manual());
  foco.fileira = SEC_EPISODIOS; foco.coluna = 2;
  okCurto();
  assert(!pedFontes && pedReproduzir);
  assert(detail_pediu_reproduzir());
  t = e = -1;
  assert(!detail_pediu_fontes_ep(&t, &e));
  assert(t == 0 && e == 0);
  puts("ok: auto mode still plays, no latch");

  puts("detail_ep_fonte_foco: episode card latches T/E into the sheet request");
  return 0;
}
