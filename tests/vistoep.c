// O MAPA DE EPISODIOS VISTOS. Sem rede: alimenta o leitor com o corpo que o
// Trakt devolve, transcrito do contrato de /sync/watched/shows.
//
// O que este teste protege, e que a tela vai depender: a diferenca entre 0
// ("sabe-se que nao viu") e -1 ("nao se sabe"). Confundir os dois faz a lista
// de episodios afirmar "nao assistido" sobre uma serie que ela nunca consultou
// — a mesma familia de defeito que o heroi mostrando a arte do titulo anterior.
#include "vistoep.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *CORPO =
"[{\"plays\":14,\"show\":{\"title\":\"Silo\",\"ids\":{\"trakt\":1,\"imdb\":\"tt14688458\"}},"
  "\"seasons\":[{\"number\":1,\"episodes\":[{\"number\":1},{\"number\":2},{\"number\":3}]},"
                "{\"number\":2,\"episodes\":[{\"number\":1},{\"number\":2}]}]},"
 "{\"plays\":3,\"show\":{\"title\":\"Fallout\",\"ids\":{\"imdb\":\"tt12637874\"}},"
  "\"seasons\":[{\"number\":1,\"episodes\":[{\"number\":5}]}]}]";

int main(void) {
  int n;

  // Antes de qualquer leitura NADA e conhecido, e isso nao e "nao visto".
  assert(vistoep_estado("tt14688458", 1, 1) == -1);
  assert(!vistoep_conhecido("tt14688458"));
  puts("ok  sem dado, o estado e -1 e nao 0");

  n = vistoep_ler_trakt(CORPO);
  assert(n == 6);
  assert(vistoep_n() == 6);
  puts("ok  o corpo do Trakt entra inteiro");

  assert(vistoep_estado("tt14688458", 1, 2) == 1);
  assert(vistoep_estado("tt14688458", 2, 2) == 1);
  assert(vistoep_estado("tt12637874", 1, 5) == 1);
  puts("ok  temporadas e episodios casam");

  // O QUE NAO VEIO CONTINUA DESCONHECIDO. A resposta do Trakt e o conjunto do
  // que foi visto; ausencia nao prova o contrario.
  assert(vistoep_estado("tt14688458", 2, 3) == -1);
  assert(vistoep_estado("tt99999999", 1, 1) == -1);
  puts("ok  ausencia nao vira 0");

  // A CHAVE COMPOSTA CASA COM A SIMPLES. "tt123:2:8" e o formato que
  // CatItem.imdb carrega num item de "Continuar assistindo", e quem chama nem
  // sempre sabe qual dos dois tem na mao.
  assert(vistoep_estado("tt14688458:2:1", 2, 1) == 1);
  assert(vistoep_conhecido("tt14688458:9:9"));
  puts("ok  id composto e id simples sao a mesma chave");

  assert(vistoep_contar("tt14688458") == 5);
  assert(vistoep_contar("tt12637874") == 1);
  assert(vistoep_contar("tt99999999") == 0);
  puts("ok  a contagem por titulo");

  // MARCAR E DESMARCAR na TV: efeito local imediato, sem esperar a rede.
  vistoep_definir("tt14688458", 1, 1, 0);
  assert(vistoep_estado("tt14688458", 1, 1) == 0);
  assert(vistoep_contar("tt14688458") == 4);
  vistoep_definir("tt14688458", 1, 1, 1);
  assert(vistoep_contar("tt14688458") == 5);
  // Marcar de novo nao duplica a linha.
  assert(vistoep_n() == 6);
  puts("ok  marcar e desmarcar sem duplicar");

  // Episodio novo entra; temporada 0 (especiais) e valida, episodio 0 nao.
  vistoep_definir("tt14688458", 0, 1, 1);
  assert(vistoep_estado("tt14688458", 0, 1) == 1);
  vistoep_definir("tt14688458", 1, 0, 1);
  assert(vistoep_n() == 7);
  puts("ok  temporada 0 vale, episodio 0 nao");

  vistoep_esquecer();
  assert(vistoep_n() == 0 && vistoep_estado("tt14688458", 1, 1) == -1);
  puts("ok  logout esvazia o mapa");

  assert(vistoep_ler_trakt("{\"nao\":\"e array\"}") == -1);
  assert(vistoep_ler_trakt(NULL) == -1);
  puts("ok  corpo invalido devolve -1 sem escrever nada");

  puts("vistoep: tudo ok");
  return 0;
}
