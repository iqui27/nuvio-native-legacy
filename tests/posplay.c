// O CARTAO DE PROXIMO EPISODIO, do OK ate o pedido chegar ao roteador.
//
// Issue #14: "o aviso de proximo episodio aparece, eu aperto OK, e o proximo
// nao comeca". O relato veio com video, e no video o cartao NEM SUME — fica na
// tela depois do OK. Este teste cobre o caminho inteiro que o app.c consome:
// posplay_evento -> posplay_pediu_episodio.
#include "posplay.h"
#include "catalogo.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int tecla(int k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return posplay_evento(&e);
}

// Deixa o painel no ar com a serie em T5E3, cujo proximo e T5E4.
static void abrirPainel(void) {
  posplay_fechar();
  posplay_atualizar(0.016f, 1000, 3000.0, 3600.0, 1, 0, 1);
  assert(posplay_visivel());
}

int main(void) {
  CatItem c = { 0 };
  CatEp eps[10] = { 0 };
  int i, t = 0, e = 0;

  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Game of Thrones");
  snprintf(c.imdb, sizeof c.imdb, "tt0944947");
  c.temporada = 5; c.episodio = 3;      // o que esta tocando
  cat_definir(&c, 1);
  for (i = 0; i < 10; i++) {
    eps[i].temporada = 5; eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "Episodio %d", i + 1);
  }
  cat_definir_episodios(0, eps, 10);
  assert(cat_n_episodios(0) == 10);

  // 1. OK ARMA O PEDIDO. E o issue #14: posplay_evento gravava pedT/pedE e
  //    chamava posplay_fechar em seguida, que zerava os dois.
  abrirPainel();
  assert(tecla(SDLK_RETURN) == 1);
  assert(!posplay_visivel());
  assert(posplay_pediu_episodio(&t, &e));
  assert(t == 5 && e == 4);
  assert(!posplay_pediu_episodio(&t, &e));   // consumido uma vez so
  puts("ok  OK arma o proximo episodio");

  // 2. E O CARTAO NAO VOLTA. Sem marcar dispensado, a condicao de aparecer
  //    continua verdadeira e o painel reabre no quadro seguinte — que e
  //    exatamente o que o video do relato mostra.
  posplay_atualizar(0.016f, 1016, 3016.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  puts("ok  o cartao nao reabre depois do OK");

  // 3. VOLTAR fica no episodio: dispensa sem pedir nada.
  posplay_fechar();
  abrirPainel();
  assert(tecla(SDLK_AC_BACK) == 1);
  assert(!posplay_visivel() && !posplay_pediu_episodio(&t, &e));
  posplay_atualizar(0.016f, 1016, 3016.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  puts("ok  Voltar dispensa sem pedir episodio");

  // 4. BAIXO devolve os controles (2) e tambem nao pede nada.
  posplay_fechar();
  abrirPainel();
  assert(tecla(SDLK_DOWN) == 2);
  assert(!posplay_visivel() && !posplay_pediu_episodio(&t, &e));
  puts("ok  Baixo devolve o player sem pedir episodio");

  // 5. CONTAGEM FINAL. O cabecalho "A seguir em %d s", a constante de 5 s e o
  //    bloco que consome `fecharEm` existiam desde o inicio; faltava a linha
  //    que arma o relogio, entao o painel ficava em "A seguir" para sempre e
  //    nada tocava sozinho. No video do relato e o que se ve.
  posplay_fechar();
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);   // faltam 3 s
  assert(posplay_visivel());
  posplay_atualizar(0.016f, 1000 + 3100, 3600.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  assert(posplay_pediu_episodio(&t, &e) && t == 5 && e == 4);
  puts("ok  a contagem final toca o proximo sozinha");

  // 6. FORA DA JANELA nao arma contagem nenhuma.
  posplay_fechar();
  posplay_atualizar(0.016f, 1000, 1800.0, 3600.0, 1, 0, 0);
  assert(!posplay_visivel());
  posplay_atualizar(0.016f, 1000 + 9000, 1809.0, 3600.0, 1, 0, 0);
  assert(!posplay_pediu_episodio(&t, &e));
  puts("ok  fora da janela nada e armado");

  // 7. ULTIMO EPISODIO: nao ha proximo, o painel nao sobe.
  posplay_fechar();
  { CatItem u = *cat_item(0);
    u.temporada = 5; u.episodio = 10;
    cat_definir(&u, 1);
    cat_definir_episodios(0, eps, 10); }
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  posplay_atualizar(0.016f, 1000 + 9000, 3600.0, 3600.0, 1, 0, 1);
  assert(!posplay_pediu_episodio(&t, &e));
  puts("ok  ultimo episodio nao oferece proximo");

  // 8. QUEM MANDA E O PLAYER, e nao o item do catalogo. Aparecido so na TV:
  //    tocando T2E8 com o item ainda apontando para T2E6 (foi assim que a
  //    serie entrou, por "Continuar assistindo"), o painel oferecia T2E7.
  posplay_fechar();
  { CatItem u = *cat_item(0);
    u.temporada = 5; u.episodio = 2;      // o item ficou para tras
    cat_definir(&u, 1);
    cat_definir_episodios(0, eps, 10); }
  player_abrir(0, NULL);
  player_definir_episodio(5, 8);          // o player esta em T5E8
  posplay_atualizar(0.016f, 1000, 3000.0, 3600.0, 1, 0, 1);
  assert(posplay_visivel());
  assert(tecla(SDLK_RETURN) == 1);
  assert(posplay_pediu_episodio(&t, &e));
  assert(t == 5 && e == 9);               // e nao 3, o "proximo" de E2
  puts("ok  o proximo sai do episodio que o player toca");

  puts("posplay: tudo ok");
  return 0;
}
