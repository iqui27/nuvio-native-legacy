// #746: dois perfis da MESMA conta herdam a colecao — a troca de perfil nao
// pode perder a forma do cartao (tileShape) nem a capa que a conta mandou.
//
// O relato (11/11): subperfis herdavam a colecao "sem posters e com pastas
// virando listas". Este teste prova o caminho do codigo: o snapshot da conta
// e reaplicado depois de col_esquecer_perfil (a troca de perfil) e cada pasta
// volta com a MESMA forma e a MESMA capa — nada vira paisagem/lista por
// perder o tileShape, nada fica sem capa por perder o coverImageUrl.
//
//   bash tests/colecao746_perfil.sh
#include "../src/colecoes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

const char *addons_base_por_id(const char *id) { (void)id; return ""; }

static const char *conta =
  "{\"collections\":[{\"id\":\"c1\",\"title\":\"Streaming\",\"folders\":["
  "{\"id\":\"f1\",\"title\":\"Netflix\",\"tileShape\":\"POSTER\","
    "\"coverImageUrl\":\"https://img/nf.jpg\","
    "\"sources\":[{\"provider\":\"addon\",\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"nf\"}]},"
  "{\"id\":\"f2\",\"title\":\"Prime\",\"tileShape\":\"POSTER\","
    "\"coverImageUrl\":\"https://img/pr.jpg\","
    "\"sources\":[{\"provider\":\"addon\",\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"pr\"}]}]}]}";

static void conferir(const char *quando) {
  const ColFolder *a = col_folder(0), *b = col_folder(1);
  assert(a && b);
  assert(a->forma == COL_FORMA_POSTER);
  assert(b->forma == COL_FORMA_POSTER);
  assert(!strcmp(a->cover, "https://img/nf.jpg"));
  assert(!strcmp(b->cover, "https://img/pr.jpg"));
  assert(!strcmp(col_capa(a), "https://img/nf.jpg"));
  assert(!strcmp(col_capa(b), "https://img/pr.jpg"));
  printf("ok %s: forma POSTER e capa da conta nas duas pastas\n", quando);
}

int main(void) {
  // Perfil 1: a colecao da conta entra com forma e capa.
  assert(col_definir_json(conta) == 2);
  conferir("perfil 1");

  // Troca de perfil: as pastas do anterior saem da tela (col_esquecer_perfil,
  // chamado por app.c na troca) e o snapshot da MESMA conta e reaplicado —
  // e o que o sync faz ao entrar no perfil 2 (colfileiras_receber).
  col_esquecer_perfil();
  assert(col_n() == 0);
  assert(col_definir_json(conta) == 2);
  conferir("perfil 2 (depois da troca)");

  // Uma segunda troca, com o snapshot vindo de novo: idempotente.
  col_esquecer_perfil();
  assert(col_definir_json(conta) == 2);
  conferir("perfil 1 de novo");

  // tileShape AUSENTE na conta: vira QUADRADO (a regra do web), nunca some.
  assert(col_definir_json(
    "{\"collections\":[{\"id\":\"c2\",\"title\":\"C\",\"folders\":["
    "{\"id\":\"g\",\"title\":\"Sem forma\",\"coverImageUrl\":\"https://img/g.jpg\","
    "\"sources\":[{\"provider\":\"addon\",\"addonId\":\"x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}") == 1);
  assert(col_folder(0)->forma == COL_FORMA_QUADRADO);
  assert(!strcmp(col_folder(0)->cover, "https://img/g.jpg"));
  puts("ok sem tileShape a conta ainda traz a capa (forma quadrado, nao lista)");

  puts("colecao746_perfil: ok");
  return 0;
}
