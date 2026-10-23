// A LINHA DE META DO DESTAQUE com os quatro ajustes do #205 (sinopse, notas,
// ano e duracao, cada um no seu lugar de Ajustes > Tela inicial > Estilo).
// hero_meta_linha junta o que sobrou com o MESMO separador, SEM buraco, em
// qualquer combinacao — e o catalogo manda "ANO  ·  DURACAO" num campo so
// (descoberta.c), entao o ano e a duracao tem de partir para cada um
// obedecer ao seu ajuste.
//
//   bash tests/herometa.sh
#include "../src/ajustes.c"
#include "../src/home.h"
#include <assert.h>
#include <string.h>

static char linha[288];

// g/a/d: 0 = Ligado (o padrao), 1 = Desligado (V_LIGA).
static void ajustes(int genero, int ano, int duracao) {
  valor[AJ_HERO_GENERO] = genero;
  valor[AJ_HERO_ANO] = ano;
  valor[AJ_HERO_DURACAO] = duracao;
}

int main(void) {
  CatItem ci;
  memset(&ci, 0, sizeof ci);
  strcpy(ci.tipo, "movie");
  strcpy(ci.genero, "Filme  \xc2\xb7  Terror");
  strcpy(ci.meta, "2022  \xc2\xb7  1 h 54 min");

  // Ligados de fabrica: o visual de sempre.
  ajustes(0, 0, 0); hero_meta_linha(&ci, linha, sizeof linha);
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror" "  \xe2\x80\xa2  " "2022" "  \xe2\x80\xa2  " "1 h 54 min"));

  // Cada ajuste esconde so o seu campo; o separador nao sobra.
  ajustes(0, 0, 1); hero_meta_linha(&ci, linha, sizeof linha);   // duracao off
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror" "  \xe2\x80\xa2  " "2022"));
  ajustes(0, 1, 0); hero_meta_linha(&ci, linha, sizeof linha);   // ano off
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror" "  \xe2\x80\xa2  " "1 h 54 min"));
  ajustes(0, 1, 1); hero_meta_linha(&ci, linha, sizeof linha);   // ano e duracao off
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror"));
  ajustes(1, 0, 0); hero_meta_linha(&ci, linha, sizeof linha);   // genero off (o ajuste que ja existia)
  assert(!strcmp(linha, "2022" "  \xe2\x80\xa2  " "1 h 54 min"));
  ajustes(1, 0, 1); hero_meta_linha(&ci, linha, sizeof linha);
  assert(!strcmp(linha, "2022"));
  ajustes(1, 1, 0); hero_meta_linha(&ci, linha, sizeof linha);
  assert(!strcmp(linha, "1 h 54 min"));
  ajustes(1, 1, 1); hero_meta_linha(&ci, linha, sizeof linha);   // tudo off
  assert(linha[0] == 0);

  // Episodio em andamento ("S1 E2 nome") continua na frente, com o mesmo
  // ponto cheio.
  strcpy(ci.nomeEpisodio, "O Rato");
  ci.temporada = 1; ci.episodio = 2;
  ci.progresso = 50; ci.restanteMin = 12;
  ajustes(0, 0, 0); hero_meta_linha(&ci, linha, sizeof linha);
  assert(!strcmp(linha, "S1 E2" "  \xe2\x80\xa2  " "O Rato" "  \xe2\x80\xa2  " "Filme" "  \xe2\x80\xa2  " "Terror" "  \xe2\x80\xa2  " "2022" "  \xe2\x80\xa2  " "1 h 54 min"));
  ajustes(1, 1, 1); hero_meta_linha(&ci, linha, sizeof linha);
  // O episodio em andamento nao tem ajuste: fica, como sempre.
  assert(!strcmp(linha, "S1 E2" "  \xe2\x80\xa2  " "O Rato"));

  // Catalogo sem duracao ("2022") e sem ano (so "1 h 54 min"): o campo que
  // existe obedece ao ajuste certo.
  strcpy(ci.nomeEpisodio, ""); ci.temporada = 0; ci.episodio = 0;
  ci.progresso = 0; ci.restanteMin = 0;
  strcpy(ci.meta, "2022");
  ajustes(0, 0, 0); hero_meta_linha(&ci, linha, sizeof linha);
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror" "  \xe2\x80\xa2  " "2022"));
  ajustes(0, 1, 0); hero_meta_linha(&ci, linha, sizeof linha);
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror"));
  strcpy(ci.meta, "1 h 54 min");
  ajustes(0, 0, 0); hero_meta_linha(&ci, linha, sizeof linha);
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror" "  \xe2\x80\xa2  " "1 h 54 min"));
  ajustes(0, 1, 0); hero_meta_linha(&ci, linha, sizeof linha);   // ano off: a duracao sem ano obedece ao ajuste DELA
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror" "  \xe2\x80\xa2  " "1 h 54 min"));
  ajustes(0, 0, 1); hero_meta_linha(&ci, linha, sizeof linha);   // duracao off: so o genero
  assert(!strcmp(linha, "Filme" "  \xe2\x80\xa2  " "Terror"));

  // Sem item: linha vazia.
  ajustes(0, 0, 0); hero_meta_linha(NULL, linha, sizeof linha);
  assert(linha[0] == 0);
  // O separador e o MESMO ponto cheio de sempre: nada de separador novo.
  strcpy(ci.meta, "2022  \xc2\xb7  1 h 54 min");
  ajustes(0, 0, 0); hero_meta_linha(&ci, linha, sizeof linha);
  assert(strstr(linha, "  \xe2\x80\xa2  ") && !strstr(linha, "\xc2\xb7"));
  puts("herometa: ok");
  return 0;
}
