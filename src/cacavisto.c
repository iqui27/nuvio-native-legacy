// Adaptador do "visto" da Caca. Ver cacavisto.h: e o UNICO arquivo que muda
// quando as fontes de historico mudam.
#include "cacavisto.h"
#include "catalogo.h"
#include "diario.h"
#include "progresso.h"
#include <stdio.h>
#include <string.h>

int cacavisto_filme(const char *imdb, long tmdb, long long *quando) {
  ProgRegistro r;
  int hist;
  (void)tmdb;
  if (quando) *quando = 0;
  if (!imdb || strncmp(imdb, "tt", 2)) return 0;
  // O Diario (perfil ativo) responde o "visto" e a data da PRIMEIRA vez
  // (`primeiro`), que e o que "Em ordem" pede: reassistir nao muda a ordem.
  { DiarioTitulo d;
    if (diario_titulo(0, imdb, &d) && d.vezes > 0) {
      if (quando && d.primeiro > 0) *quando = d.primeiro;
      return 1;
    } }
  // Sem diario (ainda nao carregado): as fontes antigas, como antes.
  if (prog_por_chave(imdb, &r) && r.durSeg > 60.0 && r.posSeg >= r.durSeg * 0.9) {
    if (quando && r.lastWatchedMs > 0) *quando = r.lastWatchedMs / 1000;
    return 1;
  }
  hist = cat_historico_estado_id(imdb, "movie");
  return hist == 1;
}

unsigned long long cacavisto_geracao(void) {
  // A geracao do diario cobre visto novo e nota; a do catalogo, o resto.
  return ((unsigned long long)diario_geracao(0) << 33) ^
         (cat_historico_geracao() << 1) ^ (unsigned long long)cat_revisao_itens();
}
