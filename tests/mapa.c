// Cruzamento do mapa do gosto (src/mapa.c), sem rede e sem GL: as respostas
// sao recortes no formato real do TMDB, com as armadilhas que importam —
// "vote_average" de last_episode_to_air antes do da raiz, "name" de
// created_by[] antes do titulo, e "results" tanto em keywords quanto em
// recommendations da serie.
#include "mapa.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *FILME_A =
  "{\"adult\":false,\"backdrop_path\":\"/fundoA.jpg\","
  "\"belongs_to_collection\":{\"id\":9,\"name\":\"Colecao\",\"poster_path\":\"/col.jpg\"},"
  "\"genres\":[{\"id\":878,\"name\":\"Ficção científica\"},{\"id\":18,\"name\":\"Drama\"}],"
  "\"id\":157336,\"imdb_id\":\"tt0816692\",\"overview\":\"Uma equipe viaja\\tpelo espaço.\","
  "\"poster_path\":\"/posterA.jpg\",\"production_companies\":[{\"id\":1,\"name\":\"Estudio\"}],"
  "\"release_date\":\"2014-11-05\",\"title\":\"Interestelar\",\"vote_average\":8.4,\"vote_count\":35000,"
  "\"credits\":{\"cast\":[{\"id\":10297,\"name\":\"Matthew McConaughey\",\"profile_path\":\"/mm.jpg\"},"
  "{\"id\":83002,\"name\":\"Jessica Chastain\",\"profile_path\":null}],"
  "\"crew\":[{\"id\":525,\"name\":\"Christopher Nolan\",\"job\":\"Director\",\"profile_path\":\"/cn.jpg\"},"
  "{\"id\":999,\"name\":\"Hans Zimmer\",\"job\":\"Original Music Composer\"}]},"
  "\"keywords\":{\"keywords\":[{\"id\":4379,\"name\":\"time travel\"},{\"id\":3801,\"name\":\"space travel\"},"
  "{\"id\":7777,\"name\":\"wormhole\"}]},"
  "\"recommendations\":{\"page\":1,\"results\":["
  "{\"id\":27205,\"title\":\"A Origem\",\"media_type\":\"movie\",\"poster_path\":\"/inc.jpg\","
  "\"release_date\":\"2010-07-15\",\"vote_average\":8.4,\"vote_count\":36000,\"genre_ids\":[28,878],"
  "\"overview\":\"Sonhos dentro de sonhos.\"},"
  "{\"id\":286217,\"title\":\"Perdido em Marte\",\"media_type\":\"movie\",\"poster_path\":\"/mar.jpg\","
  "\"release_date\":\"2015-09-30\",\"vote_average\":7.7,\"vote_count\":20000,\"genre_ids\":[18,878]},"
  "{\"id\":1,\"title\":\"Sem poster\",\"media_type\":\"movie\",\"poster_path\":null}"
  "]}}";

static const char *FILME_B =
  "{\"genres\":[{\"id\":878,\"name\":\"Ficção científica\"},{\"id\":9648,\"name\":\"Mistério\"}],"
  "\"id\":77,\"imdb_id\":\"tt0209144\",\"poster_path\":\"/amn.jpg\",\"release_date\":\"2000-10-11\","
  "\"title\":\"Amnésia\",\"vote_average\":8.2,\"vote_count\":14000,"
  "\"credits\":{\"cast\":[{\"id\":529,\"name\":\"Guy Pearce\"}],"
  "\"crew\":[{\"id\":525,\"name\":\"Christopher Nolan\",\"job\":\"Director\"}]},"
  "\"keywords\":{\"keywords\":[{\"id\":4379,\"name\":\"time travel\"},{\"id\":1,\"name\":\"memory\"}]},"
  "\"recommendations\":{\"results\":["
  "{\"id\":27205,\"title\":\"A Origem\",\"media_type\":\"movie\",\"poster_path\":\"/inc.jpg\","
  "\"release_date\":\"2010-07-15\",\"vote_average\":8.4,\"vote_count\":36000,\"genre_ids\":[28,878]},"
  "{\"id\":1124,\"title\":\"O Grande Truque\",\"media_type\":\"movie\",\"poster_path\":\"/pre.jpg\","
  "\"release_date\":\"2006-10-17\",\"vote_average\":8.2,\"vote_count\":16000,\"genre_ids\":[18,9648]}"
  "]}}";

static const char *SERIE_C =
  "{\"backdrop_path\":\"/dk.jpg\",\"created_by\":[{\"id\":5001,\"name\":\"Baran bo Odar\",\"profile_path\":\"/bo.jpg\"}],"
  "\"first_air_date\":\"2017-12-01\",\"genres\":[{\"id\":18,\"name\":\"Drama\"},{\"id\":9648,\"name\":\"Mistério\"}],"
  "\"id\":70523,\"last_episode_to_air\":{\"id\":5,\"name\":\"O Paraíso\",\"vote_average\":3.1},"
  "\"name\":\"Dark\",\"overview\":\"Uma cidade pequena.\",\"poster_path\":\"/dark.jpg\",\"vote_average\":8.4,"
  "\"vote_count\":7000,"
  "\"credits\":{\"cast\":[{\"id\":6001,\"name\":\"Louis Hofmann\"}]},"
  "\"keywords\":{\"results\":[{\"id\":4379,\"name\":\"time travel\"},{\"id\":2,\"name\":\"small town\"}]},"
  "\"recommendations\":{\"results\":[{\"id\":66732,\"name\":\"Stranger Things\",\"media_type\":\"tv\","
  "\"poster_path\":\"/st.jpg\",\"first_air_date\":\"2016-07-15\",\"vote_average\":8.6,\"vote_count\":17000,"
  "\"genre_ids\":[18,9648]}]}}";

static const char *CREDITOS =
  "{\"cast\":[{\"id\":3,\"title\":\"Cameo\",\"media_type\":\"movie\",\"poster_path\":\"/c.jpg\",\"vote_count\":900}],"
  "\"crew\":[{\"id\":157336,\"title\":\"Interestelar\",\"media_type\":\"movie\",\"job\":\"Director\","
  "\"poster_path\":\"/posterA.jpg\",\"vote_count\":35000,\"release_date\":\"2014-11-05\"},"
  "{\"id\":155,\"title\":\"Batman: O Cavaleiro das Trevas\",\"media_type\":\"movie\",\"job\":\"Director\","
  "\"poster_path\":\"/tdk.jpg\",\"vote_count\":33000,\"vote_average\":8.5,\"release_date\":\"2008-07-16\"},"
  "{\"id\":156,\"title\":\"Produzido\",\"media_type\":\"movie\",\"job\":\"Producer\","
  "\"poster_path\":\"/p.jpg\",\"vote_count\":99000}]}";

int main(void) {
  static MapaSemente s[3];
  static MapaCreditos cred[1];
  static Mapa m;
  int i, achouOrigem = 0;

  memset(s, 0, sizeof s);
  assert(mapa_ler_detalhe(FILME_A, 0, &s[0]));
  assert(!strcmp(s[0].obra.titulo, "Interestelar"));
  assert(s[0].obra.tmdb == 157336 && s[0].obra.ano == 2014 && s[0].obra.nota == 84);
  assert(!strcmp(s[0].obra.imdb, "tt0816692"));
  assert(strchr(s[0].obra.sinopse, '\t') == NULL);
  assert(!strcmp(s[0].obra.poster, "https://image.tmdb.org/t/p/w185/posterA.jpg"));
  assert(s[0].nGen == 2 && s[0].gen[0].id == 878);
  assert(s[0].nKw == 3);
  assert(s[0].nGente == 3 && s[0].gente[0].direcao == 1 && !strcmp(s[0].gente[0].nome, "Christopher Nolan"));
  assert(s[0].nRec == 2);   // o sem poster fica de fora
  assert(s[0].rec[0].nGen == 2 && s[0].rec[0].generos[1] == 878);

  assert(mapa_ler_detalhe(FILME_B, 0, &s[1]));
  assert(mapa_ler_detalhe(SERIE_C, 1, &s[2]));
  // A armadilha de /tv: nome e nota sao os da RAIZ, nao do episodio.
  assert(!strcmp(s[2].obra.titulo, "Dark") && s[2].obra.nota == 84);
  assert(!strcmp(s[2].obra.tipo, "series") && s[2].obra.ano == 2017);
  assert(s[2].nKw == 2 && s[2].nRec == 1 && s[2].gente[0].direcao == 1);
  assert(!strcmp(s[2].rec[0].o.tipo, "series"));
  for (i = 0; i < 3; i++) s[i].quando = 1;

  assert(mapa_ler_creditos(CREDITOS, 1, &cred[0]) == 2);
  assert(cred[0].obras[0].votos >= cred[0].obras[1].votos);
  cred[0].pessoa = 525;

  mapa_cruzar(s, 3, cred, 1, NULL, 0, &m);
  assert(m.estado == MAPA_CRUZADO && m.nSem == 3);
  assert(m.nPontes >= 2);
  // A Origem e recomendada pelas duas de Nolan: e a ponte mais forte.
  assert(!strcmp(m.pontes[0].obra.titulo, "A Origem"));
  assert((m.pontes[0].a == 0 && m.pontes[0].b == 1));
  assert(m.pontes[0].elo == MAPA_ELO_TEMA);
  assert(!strcmp(m.pontes[0].motivo, "viagem no tempo"));
  for (i = 0; i < m.nPontes; i++) {
    int j;
    for (j = i + 1; j < m.nPontes; j++)
      assert(m.pontes[i].obra.tmdb != m.pontes[j].obra.tmdb);
    if (!strcmp(m.pontes[i].obra.titulo, "Interestelar")) achouOrigem = 1;
  }
  assert(!achouOrigem);    // semente nunca volta como sugestao
  assert(m.nFios == 1 && !strcmp(m.fios[0].nome, "Christopher Nolan"));
  assert(m.fios[0].n == 2 && m.fios[0].direcao == 1);
  // Interestelar ja foi visto: a proxima do fio e o Batman.
  assert(m.fios[0].temProxima && m.fios[0].proxima.tmdb == 155);
  assert(m.nTemas >= 1 && !strcmp(m.temas[0].nome, "viagem no tempo") && m.temas[0].n == 3);
  assert(m.anoMin == 2000 && m.anoMax >= 2017);
  assert(mapa_tema_nome("wormhole") == NULL);   // sem traducao: nao vira tema

  // Titulo ja comecado fora das sementes nao vira ponte.
  { unsigned v = mapa_hash_titulo("A Origem");
    mapa_cruzar(s, 3, cred, 1, &v, 1, &m);
    for (i = 0; i < m.nPontes; i++) assert(strcmp(m.pontes[i].obra.titulo, "A Origem"));
    for (i = 0; i < m.nSorte; i++) assert(strcmp(m.sorte[i].titulo, "A Origem")); }

  // Uma semente so: ainda ha o que sugerir (a == b).
  mapa_cruzar(s, 1, NULL, 0, NULL, 0, &m);
  assert(m.nPontes >= 1 && m.pontes[0].a == 0 && m.pontes[0].b == 0);

  // Nada: mapa vazio, sem estourar.
  mapa_cruzar(s, 0, NULL, 0, NULL, 0, &m);
  assert(m.estado == MAPA_VAZIO && m.nPontes == 0);

  puts("mapa: ok");
  return 0;
}
