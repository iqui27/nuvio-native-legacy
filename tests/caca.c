// A PARTE PURA DA CACA (cacadados.c): classe do filme, escopo, datas e os dois
// leitores (TMDB movie_credits e o formato do Worker). Sem rede, sem SDL.
//
// O corpo de movie_credits abaixo tem a FORMA do TMDB (cast/crew, genre_ids,
// job, character, adult) com os casos que sujam uma filmografia: credito de
// roteiro do diretor, documentario sobre ele ("Himself"), filme de TV, curta,
// filme ainda nao lancado e o mesmo filme duas vezes (diretor E produtor).
#include "caca.h"
#include "conquista.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *CREDITOS =
  "{\"cast\":["
  "{\"id\":900,\"title\":\"Making of Duna\",\"character\":\"Himself\",\"release_date\":\"2022-01-01\",\"genre_ids\":[99],\"adult\":false,\"poster_path\":\"/m.jpg\"},"
  "{\"id\":901,\"title\":\"Cameo\",\"character\":\"Man in Bar\",\"release_date\":\"2010-03-03\",\"genre_ids\":[18],\"adult\":false}"
  "],\"crew\":["
  "{\"id\":10,\"title\":\"Incendies\",\"job\":\"Director\",\"department\":\"Directing\",\"release_date\":\"2010-09-04\",\"genre_ids\":[18,9648],\"adult\":false,\"poster_path\":\"/inc.jpg\"},"
  "{\"id\":10,\"title\":\"Incendies\",\"job\":\"Screenplay\",\"department\":\"Writing\",\"release_date\":\"2010-09-04\",\"genre_ids\":[18],\"adult\":false},"
  "{\"id\":11,\"title\":\"Arrival\",\"job\":\"Director\",\"release_date\":\"2016-11-10\",\"genre_ids\":[878],\"adult\":false},"
  "{\"id\":12,\"title\":\"Dune: Part Three\",\"job\":\"Director\",\"release_date\":\"2099-12-18\",\"genre_ids\":[878],\"adult\":false},"
  "{\"id\":13,\"title\":\"Un 32 août sur terre\",\"job\":\"Director\",\"release_date\":\"1998-08-28\",\"genre_ids\":[18],\"adult\":false},"
  "{\"id\":14,\"title\":\"Next Floor\",\"job\":\"Director\",\"release_date\":\"2008-05-20\",\"genre_ids\":[],\"adult\":false},"
  "{\"id\":15,\"title\":\"TV Special\",\"job\":\"Director\",\"release_date\":\"2005-01-01\",\"genre_ids\":[10770],\"adult\":false},"
  "{\"id\":16,\"title\":\"Sem data\",\"job\":\"Director\",\"release_date\":\"\",\"genre_ids\":[18],\"adult\":false},"
  "{\"id\":17,\"title\":\"Producao\",\"job\":\"Producer\",\"release_date\":\"2019-01-01\",\"genre_ids\":[18],\"adult\":false}"
  "]}";

static void classes(void) {
  int doc[] = { 18, 99 }, tv[] = { 99, 10770 }, dr[] = { 18 };
  assert(caca_classe(doc, 2, 0) == CACA_DOC);
  assert(caca_classe(tv, 2, 0) == CACA_TV);       // TV ganha de doc
  assert(caca_classe(dr, 1, 12) == CACA_CURTA);
  assert(caca_classe(dr, 1, 40) == CACA_LONGA);   // 40 ja e longa
  assert(caca_classe(dr, 1, 0) == CACA_LONGA);    // sem duracao: longa ate saber
  assert(caca_classe(NULL, 0, 0) == CACA_LONGA);
  // Escopo padrao: so longas. Doc e curta por escolha; TV nunca.
  assert(caca_no_escopo(CACA_LONGA, 0));
  assert(!caca_no_escopo(CACA_DOC, 0) && caca_no_escopo(CACA_DOC, CACA_ESC_DOC));
  assert(!caca_no_escopo(CACA_CURTA, CACA_ESC_DOC) && caca_no_escopo(CACA_CURTA, CACA_ESC_CURTA));
  assert(!caca_no_escopo(CACA_TV, CACA_ESC_DOC | CACA_ESC_CURTA));
  assert(caca_data("2016-11-10") == 20161110);
  assert(caca_data("1998") == 19980101);
  assert(caca_data("") == 0 && caca_data(NULL) == 0 && caca_data("abc") == 0);
  printf("ok classes\n");
}

static void creditos(void) {
  CacaFilme v[16];
  int n = caca_ler_creditos(CREDITOS, CACA_DIRECAO, 20261006, v, 16), i, longas = 0;
  // Direcao: 10, 11, 13, 14, 15 (o futuro, o sem data, o roteiro duplicado e
  // o de produtor ficam fora). Em ordem de lancamento.
  assert(n == 5);
  assert(v[0].tmdb == 13 && v[0].data == 19980828);
  assert(v[n - 1].tmdb == 11);
  for (i = 1; i < n; i++) assert(v[i - 1].data <= v[i].data);
  for (i = 0; i < n; i++) {
    assert(v[i].tmdb != 12 && v[i].tmdb != 16 && v[i].tmdb != 17);
    if (v[i].tmdb == 15) assert(v[i].classe == CACA_TV);
    if (caca_no_escopo(v[i].classe, 0)) longas++;
  }
  // O filme de TV sai do escopo; o curta "Next Floor" so sai quando a duracao
  // chegar (a fila lenta), entao aqui ainda conta.
  assert(longas == 4);
  assert(!strcmp(v[0].titulo, "Un 32 ao\xc3\xbbt sur terre"));
  { int k; for (k = 0; k < n; k++) if (v[k].tmdb == 10) assert(strstr(v[k].poster, "/w185/inc.jpg")); }
  // Atuacao: o "Himself" do making of fica fora; o cameo entra.
  n = caca_ler_creditos(CREDITOS, CACA_ATUACAO, 20261006, v, 16);
  assert(n == 1 && v[0].tmdb == 901);
  // Teto respeitado.
  assert(caca_ler_creditos(CREDITOS, CACA_DIRECAO, 20261006, v, 2) == 2);
  assert(caca_ler_creditos(NULL, CACA_DIRECAO, 0, v, 16) == 0);
  printf("ok creditos\n");
}

static void filmes(void) {
  static const char *W =
    "{\"nome\":\"Denis Villeneuve\",\"filmes\":["
    "{\"tmdb\":13,\"imdb\":\"tt0156887\",\"titulo\":\"Un 32 août\",\"poster\":\"/a.jpg\",\"data\":19980828,\"classe\":0},"
    "{\"tmdb\":14,\"imdb\":\"\",\"titulo\":\"Next Floor\",\"data\":\"2008-05-20\",\"classe\":2},"
    "{\"tmdb\":14,\"imdb\":\"tt9\",\"titulo\":\"Repetido\",\"data\":1},"
    "{\"tmdb\":0,\"imdb\":\"lixo\",\"titulo\":\"Sem id\"},"
    "{\"tmdb\":11,\"imdb\":\"tt2543164\",\"titulo\":\"Arrival\",\"data\":20161110,\"classe\":9}"
    "]}";
  CacaFilme v[8];
  int n = caca_ler_filmes(W, v, 8);
  assert(n == 3);
  assert(!strcmp(v[0].imdb, "tt0156887") && strstr(v[0].poster, "/w185/a.jpg"));
  assert(v[1].data == 20080520 && v[1].classe == CACA_CURTA && !v[1].imdb[0]);
  assert(v[2].classe == CACA_LONGA);   // classe invalida vira longa
  printf("ok filmes\n");
}

// "Voce viu 2 de 11 Gore Verbinski — faltam 9": o motor sobre uma filmografia
// lida, com o adaptador de visto simulado.
static void progresso(void) {
  CacaFilme v[16];
  ConqItem it[16];
  int n = caca_ler_creditos(CREDITOS, CACA_DIRECAO, 20261006, v, 16), i, k = 0;
  ConqProgresso p;
  for (i = 0; i < n; i++) {
    if (!caca_no_escopo(v[i].classe, 0)) continue;
    it[k].ordem = v[i].data;
    it[k].visto = v[i].tmdb == 10 || v[i].tmdb == 11;
    it[k].quando = it[k].visto ? 1000 + v[i].tmdb : 0;
    k++;
  }
  p = conq_calcular(it, k);
  assert(p.vistos == 2 && p.total == 4 && conq_faltam(&p) == 2);
  assert(p.degraus == (CONQ_PRIMEIRO | CONQ_METADE));
  printf("ok progresso\n");
}

int main(void) {
  classes();
  creditos();
  filmes();
  progresso();
  printf("caca: tudo ok\n");
  return 0;
}
