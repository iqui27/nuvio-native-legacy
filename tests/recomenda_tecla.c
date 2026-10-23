// Uma recomendacao que CHEGA nao pode comer a tecla de quem esta navegando.
//
//   bash tests/recomenda_tecla.sh
//
// Antes: app.c chamava recomenda_mostrar_se_houver() a cada quadro com a home
// de pe e, ao chegar "[recomenda] N nova(s)", o cartao abria NA HORA por cima
// da home; app_evento entregava toda tecla a recomenda_evento, que so aceitava
// OK e Voltar — as setas de quem rolava a fileira eram descartadas.
//
// Este teste faz o que app.c faz: com uma recomendacao nova no cache, roda o
// gancho de todo quadro e depois entrega a tecla pela mesma porta de app_evento
// (recomenda_tecla). Se o cartao pegou a tecla, a home nao a recebeu.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_REC_URL "http://127.0.0.1:8799"
#include "../src/recomenda.c"

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHOU: " __VA_ARGS__); printf("\n"); } } while (0)

static SDL_Event tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return e;
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Event e;
  int i;
  if (!dir || !dir[0]) { printf("recomenda_tecla: NUVIO_DADOS ausente\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("recomenda_tecla: dados_dir diferente\n"); return 2; }
  if (!mtx) mtx = SDL_CreateMutex();

  // Uma recomendacao nova (nao vista) acabou de chegar.
  memset(itens, 0, sizeof itens);
  itens[0].id = 9001;
  itens[0].visto = 0;
  snprintf(itens[0].imdb, sizeof itens[0].imdb, "tt0111161");
  snprintf(itens[0].titulo, sizeof itens[0].titulo, "Um Sonho de Liberdade");
  snprintf(itens[0].deNome, sizeof itens[0].deNome, "Ana");
  nItens = 1;
  CONFERE(recomenda_n_novas() == 1, "a recomendacao devia contar como nova (%d)", recomenda_n_novas());

  // Varios quadros com a home de pe, e uma seta no meio: ela tem de chegar.
  for (i = 0; i < 5; i++) recomenda_mostrar_se_houver();
  e = tecla(SDLK_RIGHT);
  CONFERE(!recomenda_aberta(), "a chegada de uma recomendacao nao pode abrir modal sozinha");
  CONFERE(!recomenda_tecla(&e), "a seta DIREITA foi comida pelo cartao de recomendacao");
  e = tecla(SDLK_DOWN);
  CONFERE(!recomenda_tecla(&e), "a seta BAIXO foi comida pelo cartao de recomendacao");
  e = tecla(SDLK_RETURN);
  CONFERE(!recomenda_tecla(&e), "o OK da home foi comido pelo cartao de recomendacao");

  // O cartao continua existindo para quem o abre de proposito (a ilha / a aba
  // Social): ai sim ele e modal, e Voltar o fecha.
  itens[0].visto = 0;
  cartaoAberto = 0;
  recomenda_abrir_cartao();
  CONFERE(recomenda_aberta(), "recomenda_abrir_cartao devia abrir o cartao");
  e = tecla(SDLK_LEFT);
  CONFERE(recomenda_tecla(&e), "aberto de proposito, o cartao e modal");
  e = tecla(SDLK_ESCAPE);
  CONFERE(recomenda_tecla(&e) && !recomenda_aberta(), "Voltar devia fechar o cartao");
  e = tecla(SDLK_RIGHT);
  CONFERE(!recomenda_tecla(&e), "fechado, a tecla volta para a home");

  printf(falhas ? "recomenda_tecla: %d falhas\n" : "recomenda_tecla: ok\n", falhas);
  return falhas ? 1 : 0;
}
