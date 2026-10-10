// O GRAFICO DE TEMPORADAS (temporadas_grafico.h), so a conta, sem GL:
// exibido x total (agenda), especiais, o que nao estreou nao conta como visto,
// ordem dos amigos (frente primeiro, o mais adiantado antes) e as frases.
#include "temporadas_grafico.h"
#include "idioma.h"
#include "ajustes.h"
#include "dados.h"
#include "catalogo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int montagens;
void __cyg_profile_func_enter(void *funcao, void *chamador) {
  (void)chamador;
  if (funcao == (void *)tgraf_montar) montagens++;
}
void __cyg_profile_func_exit(void *funcao, void *chamador) {
  (void)funcao; (void)chamador;
}

static TgEp eps[64];
static int nEps;
static void ep(int t, int e, int visto) {
  eps[nEps].temporada = (short)t; eps[nEps].episodio = (short)e;
  eps[nEps].visto = (signed char)visto; nEps++;
}
static void amigo(AmigosTitulo *a, const char *id, const char *nome, int t, int e, int reacao) {
  AmigoTit *x = &a->a[a->n++];
  memset(x, 0, sizeof *x);
  snprintf(x->id, sizeof x->id, "%s", id);
  snprintf(x->nome, sizeof x->nome, "%s", nome);
  x->temporada = t; x->episodio = e; x->reacao = reacao; x->serie = 1;
  a->total = a->n;
}

int main(void) {
  TgDados d;
  AmigosTitulo at;
  char frase[160], cam[600];
  int i;
  FILE *f;

  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
  f = fopen(cam, "w"); assert(f); fputs("idioma 0\n", f); fclose(f);
  ajustes_dir(dados_dir());

  // A serie: T3 chega ANTES na lista (ordem do addon), especiais com um visto,
  // T1 inteira vista, T2 pela metade, T3 com o E5 em diante ainda por estrear
  // (agenda: proximo = T3E5) e o E6 marcado como visto por engano no Trakt.
  memset(&d, 0, sizeof d);
  nEps = 0;
  for (i = 1; i <= 8; i++) ep(3, i, i <= 2 || i == 6 ? 1 : 0);
  ep(0, 1, 1);
  for (i = 1; i <= 10; i++) ep(1, i, 1);
  for (i = 1; i <= 10; i++) ep(2, i, i <= 5 ? 1 : 0);
  assert(tgraf_montar(&d, eps, nEps, 1, 3, 5, 0, NULL) == 3);
  assert(d.t[0].numero == 1 && d.t[1].numero == 2 && d.t[2].numero == 3);   // ordenado
  assert(d.t[0].total == 10 && d.t[0].exibidos == 10 && d.t[0].vistos == 10 && d.t[0].completa);
  assert(d.t[1].vistos == 5 && !d.t[1].completa && d.t[1].ultVisto == 5);
  assert(d.t[2].total == 8 && d.t[2].exibidos == 4);          // E5..E8 nao estrearam
  assert(d.t[2].vistos == 2 && d.t[2].ultVisto == 2);         // o E6 "visto" nao conta
  assert(d.meuT == 3 && d.meuE == 2);
  assert(d.total == 28 && d.exibidos == 24 && d.vistos == 17 && d.completas == 1);
  assert(tgraf_existe(&d));
  assert(tgraf_coluna(&d, 2) == 1 && tgraf_coluna(&d, 0) == -1);
  tgraf_frase_amigos(&d, frase, sizeof frase);
  assert(frase[0] == 0);                                      // sem amigos, sem frase

  // Especiais so quando pedidos, e entram primeiro.
  assert(tgraf_montar(&d, eps, nEps, 1, 3, 5, 1, NULL) == 4);
  assert(d.t[0].numero == 0 && d.t[0].vistos == 1 && d.t[0].completa);

  // Sem agenda tudo conta como exibido (e o E6 volta a contar).
  tgraf_montar(&d, eps, nEps, 1, 0, 0, 0, NULL);
  assert(d.t[2].exibidos == 8 && d.t[2].vistos == 3 && d.meuT == 3 && d.meuE == 6);

  // AMIGOS: os da frente primeiro, o mais adiantado antes; quem esta no MESMO
  // episodio que voce nao esta na frente; sem posicao nao entra.
  memset(&at, 0, sizeof at);
  amigo(&at, "n:bia", "Bia Souza", 2, 8, SV_REAC_NADA);
  amigo(&at, "n:caio", "Caio", 3, 3, SV_REAC_GOSTOU);
  amigo(&at, "n:eva", "Eva", 0, 0, SV_REAC_GOSTOU);           // so reagiu
  amigo(&at, "n:ana", "Ana Lima", 3, 4, SV_REAC_NADA);
  amigo(&at, "n:davi", "Davi", 1, 2, SV_REAC_NAO);
  amigo(&at, "n:gil", "Gil", 3, 2, SV_REAC_NADA);              // junto com voce
  tgraf_montar(&d, eps, nEps, 1, 3, 5, 0, &at);
  assert(d.nAmg == 5 && d.nFrente == 2 && d.nAtras == 3 && d.totalAmg == 6);
  assert(!strcmp(d.amg[0].id, "n:ana") && d.amg[0].frente && d.amg[0].col == 2);
  assert(!strcmp(d.amg[1].id, "n:caio") && d.amg[1].frente && d.amg[1].reacao == SV_REAC_GOSTOU);
  assert(!strcmp(d.amg[2].id, "n:gil") && !d.amg[2].frente);
  assert(!strcmp(d.amg[3].id, "n:bia") && !d.amg[3].frente);
  assert(!strcmp(d.amg[4].id, "n:davi") && d.amg[4].col == 0);
  tgraf_frase_amigos(&d, frase, sizeof frase);
  assert(!strcmp(frase, "Ana e Caio estão na sua frente"));

  // Tres na frente: "Ana e mais 2".
  at.a[4].temporada = 3; at.a[4].episodio = 7;                 // Davi passa todo mundo
  tgraf_montar(&d, eps, nEps, 1, 3, 5, 0, &at);
  assert(d.nFrente == 3 && !strcmp(d.amg[0].id, "n:davi"));
  tgraf_frase_amigos(&d, frase, sizeof frase);
  assert(!strcmp(frase, "Davi e mais 2 estão na sua frente"));

  // Um so, e ninguem na frente.
  { AmigosTitulo um; memset(&um, 0, sizeof um);
    amigo(&um, "n:bia", "Bia Souza", 2, 8, SV_REAC_NADA);
    tgraf_montar(&d, eps, nEps, 1, 3, 5, 0, &um);
    tgraf_frase_amigos(&d, frase, sizeof frase);
    assert(!strcmp(frase, "Você está na frente de Bia"));
    um.a[0].temporada = 3; um.a[0].episodio = 3;
    tgraf_montar(&d, eps, nEps, 1, 3, 5, 0, &um);
    tgraf_frase_amigos(&d, frase, sizeof frase);
    assert(!strcmp(frase, "Bia está na sua frente")); }

  // SEM MAPA (nao sabemos o que voce viu): nada de "sua frente", nada visto,
  // e o bloco so existe pelos amigos.
  tgraf_montar(&d, eps, nEps, 0, 3, 5, 0, &at);
  assert(d.vistos == 0 && d.meuT == 0 && d.nFrente == 0 && tgraf_existe(&d));
  tgraf_frase_amigos(&d, frase, sizeof frase);
  assert(!strcmp(frase, "Onde seus amigos estão"));
  tgraf_montar(&d, eps, nEps, 0, 3, 5, 0, NULL);
  assert(!tgraf_existe(&d));

  // Com mapa mas nada visto e sem amigos: nao ha o que mostrar.
  nEps = 0;
  for (i = 1; i <= 6; i++) ep(1, i, 0);
  tgraf_montar(&d, eps, nEps, 1, 0, 0, 0, NULL);
  assert(d.n == 1 && d.vistos == 0 && !tgraf_existe(&d));

  // Id de Continuar assistindo: a segunda consulta reutiliza a montagem.
  { CatItem item = {0}; CatEp episodio = {0}; const TgDados *cache;
    snprintf(item.imdb, sizeof item.imdb, "tt123:2:6");
    snprintf(item.tipo, sizeof item.tipo, "series");
    cat_definir_tudo(&item, 1, NULL, 0);
    episodio.temporada = 2; episodio.episodio = 6;
    cat_definir_episodios(0, &episodio, 1);
    montagens = 0;
    cache = tgraf_dados(0);
    assert(!strcmp(cache->imdb, "tt123") && cache->total == 1);
    assert(montagens == 1);
    tgraf_dados(0);
    assert(montagens == 1); }

  puts("temporadas_grafico: ok");
  return 0;
}
