// O MAPA DE EPISODIOS VISTOS. Sem rede: alimenta o leitor com o corpo que o
// Trakt devolve, transcrito do contrato de /sync/watched/shows.
//
// O que este teste protege, e que a tela vai depender: a diferenca entre 0
// ("sabe-se que nao viu") e -1 ("nao se sabe"). Confundir os dois faz a lista
// de episodios afirmar "nao assistido" sobre uma serie que ela nunca consultou
// — a mesma familia de defeito que o heroi mostrando a arte do titulo anterior.
#include "vistoep.h"
#include <assert.h>
#include <limits.h>
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

static void booleanosENumerosInvalidos(void) {
  const char *json =
    "{\"seasons\":[{\"number\":1,\"episodes\":["
      "{\"number\":1,\"completed\":                    true},"
      "{\"number\":2,\"completed\": false,\"other\":true},"
      "{\"number\":3,\"completed\":\"true\"},"
      "{\"number\":4,\"completed\":null},"
      "{\"number\":5,\"last_watched_at\":\"true\"},"
      "{\"number\":1e100,\"completed\":true},"
      "{\"number\":32768,\"completed\":true},"
      "{\"number\":1.5,\"completed\":true},"
      "{\"number\":6,\"completed\":true                     }]},"
      "{\"number\":1e100,\"episodes\":[{\"number\":1,\"completed\":true}]},"
      "{\"number\":32768,\"episodes\":[{\"number\":1,\"completed\":true}]},"
      "{\"number\":2,\"episodes\":[{\"number\":1,\"completed\":true}]}]}";
  vistoep_esquecer();
  assert(vistoep_ler_progresso("tt1", json) == 4);
  assert(vistoep_estado("tt1", 1, 1) == 1);
  assert(vistoep_estado("tt1", 1, 2) == 0);
  assert(vistoep_estado("tt1", 1, 3) == -1);
  assert(vistoep_estado("tt1", 1, 4) == -1);
  assert(vistoep_estado("tt1", 1, 5) == -1);
  assert(vistoep_estado("tt1", 1, 6) == 1);
  assert(vistoep_estado("tt1", 2, 1) == 1);
  assert(vistoep_n() == 4);
  vistoep_definir("tt1", SHRT_MAX + 1, 1, 1);
  vistoep_definir("tt1", 1, SHRT_MAX + 1, 1);
  assert(vistoep_n() == 4);
  puts("ok  completed exige booleano e numeros invalidos nao corrompem o mapa");
}

static void loteSoContaMudancasEfetivas(void) {
  VistoPar pares[] = {{1, 1}, {1, 0}, {-1, 1}, {1, 1}};
  VistoPar saida[2];
  int i;
  vistoep_esquecer();
  assert(vistoep_marcar_lote("", pares, 4, 1) == 0);
  assert(vistoep_marcar_lote("tt1", pares, 4, 1) == 1);
  assert(vistoep_n() == 1);
  assert(vistoep_lote("tt1", 1, 1, 1, pares, 4, 0, 0, saida, -1) == 0);
  // Depois do teto uma tentativa recusada nao pode anunciar mudanca ou
  // disparar um envio ao servidor como se o estado local tivesse mudado.
  for (i = 2; i <= 8000; i++) vistoep_definir("tt1", 1, i, 1);
  assert(vistoep_n() == 8000);
  pares[0].episodio = 8001;
  assert(vistoep_marcar_lote("tt1", pares, 1, 1) == 0);
  assert(vistoep_estado("tt1", 1, 8001) == -1);
  vistoep_esquecer();
  puts("ok  lote so conta entradas validas aceitas e respeita o teto do mapa");
}

// "Ate aqui" em T2E5 com S1 inteira e T2E1-4 vistos: so T2E5 muda. Antes mandava
// os 15 e o Trakt duplicava os plays (Silo, TCL 08/10 11:46:23).
static void ateAquiSoEnviaMudancas(void) {
  VistoPar cat[20], lote[32], envio[32];
  int i, n, ja = 0, k;
  for (i = 0; i < 10; i++) { cat[i].temporada = 1; cat[i].episodio = (short)(i + 1);
                             cat[10 + i].temporada = 2; cat[10 + i].episodio = (short)(i + 1); }
  vistoep_esquecer();
  for (i = 1; i <= 10; i++) vistoep_definir("tt14688458", 1, i, 1);
  for (i = 1; i <= 10; i++) vistoep_definir("tt14688458", 2, i, i <= 4);
  n = vistoep_lote("tt14688458", 1, 2, 5, cat, 20, 0, 0, lote, 32);
  assert(n == 15);
  k = vistoep_aplicar("tt14688458", lote, n, 1, envio, &ja);
  assert(k == 1 && ja == 14);
  assert(envio[0].temporada == 2 && envio[0].episodio == 5);
  assert(vistoep_contar("tt14688458") == 15);          // local correto: 15 vistos
  // de novo: nada a mandar
  assert(vistoep_aplicar("tt14688458", lote, n, 1, envio, &ja) == 0 && ja == 15);
  // desmarcar: so sai quem esta visto (T2E1-5), nao a temporada toda
  n = vistoep_lote("tt14688458", 0, 2, 0, cat, 20, 0, 0, lote, 32);
  assert(n == 10);
  k = vistoep_aplicar("tt14688458", lote, n, 0, envio, &ja);
  assert(k == 5 && ja == 5);
  for (i = 0; i < k; i++) assert(envio[i].temporada == 2 && envio[i].episodio <= 5);
  assert(vistoep_contar("tt14688458") == 10);
  // titulo desconhecido: tudo conta como mudanca (repara o remoto)
  vistoep_esquecer();
  assert(vistoep_aplicar("tt14688458", lote, 3, 1, envio, &ja) == 3 && ja == 0);
  puts("ok  ate aqui / temporada enviam so o que mudou");
}

// Modo 2 = daqui em diante; compila tambem antes da implementacao.
static void daquiEmDiante(void) {
  VistoPar cat[30], lote[32], envio[32];
  int i, n, ja;
  vistoep_esquecer();
  for (i = 0; i < 30; i++) {
    cat[i] = (VistoPar){i / 10 + 1, i % 10 + 1};
    vistoep_definir("tt14688458", cat[i].temporada, cat[i].episodio, i < 29);
  }
  n = vistoep_lote("tt14688458", 2, 2, 6, cat, 30, 0, 0, lote, 32);
  assert(n == 15);
  assert(vistoep_aplicar("tt14688458", lote, n, 0, envio, &ja) == 14 && ja == 1);
  for (i = 0; i < 30; i++)
    assert(vistoep_estado("tt14688458", cat[i].temporada, cat[i].episodio) == (i < 15));
  // Agenda filtra mapa E catalogo, inclusive o episodio ainda nao lancado.
  n = vistoep_lote("tt14688458", 2, 2, 6, cat, 30, 3, 5, lote, 32);
  assert(n == 9);
  for (i = 0; i < n; i++)
    assert(lote[i].temporada == 2 ? lote[i].episodio >= 6 :
           lote[i].temporada == 3 && lote[i].episodio < 5);
  assert(vistoep_lote("tt14688458", 2, 3, 5, cat, 30, 3, 5, lote, 32) == 0);
  assert(vistoep_lote("tt14688458", 2, 2, 6, cat, 30, 3, 5, NULL, 0) == 9);
  assert(vistoep_lote("tt14688458", 2, 2, 6, cat, 30, 3, 5, lote, 3) == 3);
  // Conta sem mapa Trakt completo: catalogo completa o intervalo sem duplicar.
  vistoep_esquecer();
  vistoep_definir("tt14688458", 3, 1, 1);
  assert(vistoep_lote("tt14688458", 2, 2, 6, cat, 30, 3, 5, lote, 32) == 9);
  // Anteriores nao gastam o teto do lote antes de chegar ao alvo.
  for (i = 1; i <= 300; i++) vistoep_definir("tt14688458", 1, i, 1);
  assert(vistoep_lote("tt14688458", 2, 2, 6, cat, 30, 3, 5, lote, 32) == 9);
  puts("ok  daqui em diante preserva anteriores, filtra nao lancados e une mapa/catalogo");
}

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

  // --- OS TRES GESTOS DA TELA, que sao o mesmo lote em tamanhos diferentes ---
  vistoep_esquecer();
  vistoep_ler_progresso("tt14688458", PROG);
  { VistoPar lote[32];
    int k;

    // ATE AQUI. A ordem e (temporada, numero) NESTA ordem: parar no T2E1 leva
    // a temporada 1 inteira mais o primeiro da 2 — e nao "todo episodio de
    // numero <= 1", que pegaria o T1E1 e deixaria o T1E4 para tras.
    k = vistoep_ate_aqui("tt14688458", 2, 1, lote, 32);
    assert(k == 5);
    { int i, achouT1E4 = 0, achouT2E2 = 0;
      for (i = 0; i < k; i++) {
        if (lote[i].temporada == 1 && lote[i].episodio == 4) achouT1E4 = 1;
        if (lote[i].temporada == 2 && lote[i].episodio == 2) achouT2E2 = 1;
      }
      assert(achouT1E4);    // temporada anterior inteira entra
      assert(!achouT2E2); } // o que vem depois na mesma temporada, nao
    puts("ok  \"ate aqui\" respeita (temporada, numero) e nao so o numero");

    // TEMPORADA INTEIRA.
    k = vistoep_temporada("tt14688458", 1, lote, 32);
    assert(k == 4);
    k = vistoep_temporada("tt14688458", 2, lote, 32);
    assert(k == 2);
    k = vistoep_temporada("tt14688458", 9, lote, 32);
    assert(k == 0);
    puts("ok  a temporada inteira, e temporada inexistente devolve zero");

    // O LOTE SO CONTA QUEM MUDOU. Marcar o que ja estava visto devolve 0, e e
    // isso que impede a tela de mandar um POST ao Trakt para nada.
    k = vistoep_temporada("tt14688458", 1, lote, 32);
    assert(vistoep_marcar_lote("tt14688458", lote, k, 1) == 1);  // so o T1E3
    assert(vistoep_marcar_lote("tt14688458", lote, k, 1) == 0);
    assert(vistoep_contar("tt14688458") == 5);
    assert(vistoep_marcar_lote("tt14688458", lote, k, 0) == 4);
    assert(vistoep_contar("tt14688458") == 1);
    puts("ok  o lote conta so quem mudou de estado");

    // Teto respeitado, sem escrever fora.
    k = vistoep_ate_aqui("tt14688458", 2, 2, lote, 3);
    assert(k == 3);
    puts("ok  o teto do vetor e respeitado");

    // MODO CONTAGEM. A tela precisa do numero para escrever "Ate aqui (7
    // episodios)" ANTES de decidir agir, e a primeira versao passava max=0
    // com um vetor de verdade — o laco parava em `k < max` e o rotulo dizia
    // zero em toda linha.
    assert(vistoep_ate_aqui("tt14688458", 2, 2, NULL, 0) == 6);
    assert(vistoep_temporada("tt14688458", 1, NULL, 0) == 4);
    assert(vistoep_temporada("tt14688458", 9, NULL, 0) == 0);
    puts("ok  saida nula conta sem truncar em max");
  }

  daquiEmDiante();
  ateAquiSoEnviaMudancas();
  booleanosENumerosInvalidos();
  loteSoContaMudancasEfetivas();
  puts("vistoep: tudo ok");
  return 0;
}
