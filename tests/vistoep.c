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

// /shows/<id>/progress/watched, com o `completed` explicito por episodio.
static const char *PROG =
"{\"aired\":15,\"completed\":4,\"seasons\":["
 "{\"number\":1,\"aired\":10,\"completed\":3,\"episodes\":["
   "{\"number\":1,\"completed\":true,\"last_watched_at\":\"2026-01-01T00:00:00.000Z\"},"
   "{\"number\":2,\"completed\":true},"
   "{\"number\":3,\"completed\":false},"
   "{\"number\":4,\"completed\":true}]},"
 "{\"number\":2,\"aired\":5,\"completed\":1,\"episodes\":["
   "{\"number\":1,\"completed\":true},"
   "{\"number\":2,\"completed\":false}]}]}";

int main(void) {
  int n;

  // Antes de qualquer leitura NADA e conhecido, e isso nao e "nao visto".
  assert(vistoep_estado("tt14688458", 1, 1) == -1);
  assert(!vistoep_conhecido("tt14688458"));
  puts("ok  sem dado, o estado e -1 e nao 0");

  n = vistoep_ler_progresso("tt14688458", PROG);
  assert(n == 6);
  assert(vistoep_n() == 6);
  puts("ok  o progresso da serie entra inteiro");

  // AQUI O 0 E AFIRMACAO, e nao ausencia: a resposta enumera a serie inteira e
  // diz sim ou nao por episodio. E o que permite a tela escrever "nao
  // assistido" sem mentir.
  assert(vistoep_estado("tt14688458", 1, 3) == 0);
  assert(vistoep_estado("tt14688458", 2, 2) == 0);
  puts("ok  completed:false vira 0, e nao -1");


  assert(vistoep_estado("tt14688458", 1, 2) == 1);
  assert(vistoep_estado("tt14688458", 2, 1) == 1);
  puts("ok  temporadas e episodios casam");

  // SERIE NUNCA CONSULTADA continua -1. O 0 so existe onde a resposta afirmou.
  assert(vistoep_estado("tt14688458", 9, 9) == -1);
  assert(vistoep_estado("tt99999999", 1, 1) == -1);
  puts("ok  serie nao consultada continua -1");

  // A CHAVE COMPOSTA CASA COM A SIMPLES. "tt123:2:8" e o formato que
  // CatItem.imdb carrega num item de "Continuar assistindo", e quem chama nem
  // sempre sabe qual dos dois tem na mao.
  assert(vistoep_estado("tt14688458:2:1", 2, 1) == 1);
  assert(vistoep_conhecido("tt14688458:9:9"));
  puts("ok  id composto e id simples sao a mesma chave");

  assert(vistoep_contar("tt14688458") == 4);
  assert(vistoep_contar("tt99999999") == 0);
  puts("ok  a contagem por titulo conta so os vistos");

  // MARCAR E DESMARCAR na TV: efeito local imediato, sem esperar a rede.
  vistoep_definir("tt14688458", 1, 1, 0);
  assert(vistoep_estado("tt14688458", 1, 1) == 0);
  assert(vistoep_contar("tt14688458") == 3);
  vistoep_definir("tt14688458", 1, 1, 1);
  assert(vistoep_contar("tt14688458") == 4);
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

  assert(vistoep_ler_progresso("tt1", "{\"sem\":\"temporadas\"}") == -1);
  assert(vistoep_ler_progresso("tt1", NULL) == -1);
  assert(vistoep_ler_progresso(NULL, PROG) == -1);
  puts("ok  corpo invalido devolve -1 sem escrever nada");

  puts("vistoep: tudo ok");
  return 0;
}
