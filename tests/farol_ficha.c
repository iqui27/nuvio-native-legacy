// Ficha do titulo (src/farolficha.c), sem rede e sem GL: as respostas sao
// recortes no formato real do TMDB (mesma familia de tests/mapa.c), com as
// seis formas que a issue 36 pede: filme, serie, sem palavras-chave, sem
// creditos, anime e corpo truncado. Cobre tambem o caminho sem TMDB
// (CatItem -> ids negativos estaveis), o cache LRU de 600 e o arquivo.
//
//   bash tests/farol_ficha.sh                  (ASan+UBSan, o padrao)
//   SANITIZE=0 bash tests/farol_ficha.sh       (so os asserts)
#include "farolficha.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// 1. FILME — /movie/550 com append_to_response=keywords,credits,
//    recommendations,external_ids.
static const char *FILME =
  "{\"adult\":false,\"backdrop_path\":\"/fundo.jpg\",\"belongs_to_collection\":null,"
  "\"budget\":63000000,\"genres\":[{\"id\":18,\"name\":\"Drama\"}],"
  "\"homepage\":\"http://example.com\",\"id\":550,\"imdb_id\":\"tt0137523\","
  "\"origin_country\":[\"US\"],\"original_language\":\"en\","
  "\"original_title\":\"Fight Club\",\"overview\":\"Sinopse.\",\"popularity\":61.4,"
  "\"poster_path\":\"/poster.jpg\",\"release_date\":\"1999-10-15\","
  "\"revenue\":100853753,\"runtime\":139,\"status\":\"Released\",\"title\":\"Fight Club\","
  "\"vote_average\":8.439,\"vote_count\":28700,"
  "\"external_ids\":{\"imdb_id\":\"tt0137523\",\"wikidata_id\":\"Q190050\"},"
  "\"keywords\":{\"keywords\":[{\"id\":818,\"name\":\"based on novel or book\"},"
  "{\"id\":10349,\"name\":\"nihilism\"},{\"id\":1499,\"name\":\"split personality\"},"
  "{\"id\":310,\"name\":\"insomnia\"},{\"id\":41591,\"name\":\"alter ego\"},"
  "{\"id\":10683,\"name\":\"cult film\"},{\"id\":9717,\"name\":\"surrealism\"},"
  "{\"id\":209411,\"name\":\"underground fighting\"},{\"id\":207265,\"name\":\"soap\"}]},"
  "\"credits\":{\"cast\":["
  "{\"id\":819,\"name\":\"Edward Norton\",\"order\":0,\"profile_path\":\"/en.jpg\"},"
  "{\"id\":287,\"name\":\"Brad Pitt\",\"order\":1,\"profile_path\":\"/bp.jpg\"},"
  "{\"id\":1283,\"name\":\"Helena Bonham Carter\",\"order\":2},"
  "{\"id\":7470,\"name\":\"Meat Loaf\",\"order\":3},"
  "{\"id\":7471,\"name\":\"Jared Leto\",\"order\":4},"
  "{\"id\":7499,\"name\":\"Zach Grenier\",\"order\":5},"
  "{\"id\":7500,\"name\":\"Holt McCallany\",\"order\":6},"
  "{\"id\":7501,\"name\":\"Eion Bailey\",\"order\":7},"
  "{\"id\":7502,\"name\":\"Richmond Arquette\",\"order\":8}],"
  "\"crew\":[{\"id\":7467,\"name\":\"David Fincher\",\"job\":\"Director\"},"
  "{\"id\":7469,\"name\":\"Jim Uhls\",\"job\":\"Screenplay\"}]},"
  "\"recommendations\":{\"page\":1,\"results\":["
  "{\"id\":157336,\"title\":\"Interestelar\",\"media_type\":\"movie\","
  "\"poster_path\":\"/i.jpg\",\"release_date\":\"2014-11-05\","
  "\"vote_average\":8.4,\"vote_count\":35000,\"genre_ids\":[878,18,12]}]}}";

// 2. SERIE — /tv/70523: "name" e nota na raiz (last_episode_to_air tenta
//    enganar), created_by antes do elenco, keywords.results, episode_run_time.
static const char *SERIE =
  "{\"backdrop_path\":\"/fundo.jpg\",\"created_by\":[{\"id\":5001,\"name\":\"Baran bo Odar\"},"
  "{\"id\":5002,\"name\":\"Jantje Friese\"}],"
  "\"episode_run_time\":[53],\"first_air_date\":\"2017-12-01\","
  "\"genres\":[{\"id\":9648,\"name\":\"Mystery\"},{\"id\":18,\"name\":\"Drama\"},"
  "{\"id\":878,\"name\":\"Sci-Fi & Fantasy\"}],"
  "\"id\":70523,\"in_production\":false,"
  "\"last_episode_to_air\":{\"id\":5,\"name\":\"O Paraiso\",\"vote_average\":3.1,"
  "\"runtime\":53},\"name\":\"Dark\",\"original_language\":\"de\","
  "\"overview\":\"Sinopse.\",\"poster_path\":\"/poster.jpg\","
  "\"vote_average\":8.427,\"vote_count\":4700,"
  "\"external_ids\":{\"imdb_id\":\"tt5753856\",\"instagram_id\":\"darknetflix\"},"
  "\"keywords\":{\"results\":[{\"id\":4379,\"name\":\"time travel\"},"
  "{\"id\":2,\"name\":\"small town\"},{\"id\":10364,\"name\":\"police\"}]},"
  "\"credits\":{\"cast\":[{\"id\":6001,\"name\":\"Louis Hofmann\",\"order\":0},"
  "{\"id\":6002,\"name\":\"Lisa Vicari\",\"order\":1}]},"
  "\"recommendations\":{\"results\":[]}}";

// 3. SEM PALAVRAS-CHAVE — filme com keywords vazias.
static const char *SEM_KW =
  "{\"genres\":[{\"id\":35,\"name\":\"Comedy\"}],\"id\":999001,\"imdb_id\":\"tt0000001\","
  "\"original_language\":\"en\",\"release_date\":\"2020-01-01\",\"runtime\":95,"
  "\"title\":\"Sem Palavras\",\"vote_average\":7.1,\"vote_count\":500,"
  "\"keywords\":{\"keywords\":[]},"
  "\"credits\":{\"cast\":[{\"id\":11,\"name\":\"Ator\",\"order\":0}],"
  "\"crew\":[{\"id\":22,\"name\":\"Diretor\",\"job\":\"Director\"}]}}";

// 4. SEM CREDITOS — filme com cast e crew vazios.
static const char *SEM_CRED =
  "{\"genres\":[{\"id\":28,\"name\":\"Action\"}],\"id\":999002,\"imdb_id\":\"tt0000002\","
  "\"original_language\":\"en\",\"release_date\":\"2021-05-20\",\"runtime\":110,"
  "\"title\":\"Sem Creditos\",\"vote_average\":6.4,\"vote_count\":800,"
  "\"keywords\":{\"keywords\":[{\"id\":500,\"name\":\"explosive\"}]},"
  "\"credits\":{\"cast\":[],\"crew\":[]}}";

// 5. ANIME — /movie/129: idioma "ja", genero Animation, diretor no crew.
static const char *ANIME =
  "{\"genres\":[{\"id\":16,\"name\":\"Animation\"},{\"id\":10751,\"name\":\"Family\"},"
  "{\"id\":14,\"name\":\"Fantasy\"}],\"id\":129,\"imdb_id\":\"tt0245429\","
  "\"original_language\":\"ja\",\"overview\":\"Sinopse.\",\"release_date\":\"2001-07-20\","
  "\"runtime\":125,\"title\":\"A Viagem de Chihiro\",\"vote_average\":8.537,"
  "\"vote_count\":16200,"
  "\"external_ids\":{\"imdb_id\":\"tt0245429\"},"
  "\"keywords\":{\"keywords\":[{\"id\":616,\"name\":\"witch\"},{\"id\":615,\"name\":\"train\"}]},"
  "\"credits\":{\"cast\":[{\"id\":7001,\"name\":\"Rumi Hiiragi\",\"order\":0}],"
  "\"crew\":[{\"id\":608,\"name\":\"Hayao Miyazaki\",\"job\":\"Director\"}]}}";

// 6. CORPO TRUNCADO — o mesmo detalhe, cortado no meio das palavras-chave
//    (append_to_response vai no fim). Id e generos chegam; o resto fica
//    tolerado como "nao veio".
static const char *TRUNCADO =
  "{\"genres\":[{\"id\":18,\"name\":\"Drama\"}],\"id\":550,\"imdb_id\":\"tt0137523\","
  "\"original_language\":\"en\",\"release_date\":\"1999-10-15\",\"runtime\":139,"
  "\"title\":\"Fight Club\",\"vote_average\":8.4,\"vote_count\":28700,"
  "\"keywords\":{\"keywords\":[{\"id\":818,\"name\":\"ba";

// Corte tao cedo que nem o id chegou: nao e detalhe, devolve 0.
static const char *TRUNCADO_CEDO = "{\"adult\":fals";

static FarolFichaTmp tmp;
static FarolFicha f, f2;

static int64_t chave(int tipo, long tmdb) { return ((int64_t)tipo << 32) | (uint32_t)tmdb; }

static void testarFilme(void) {
  assert(farolficha_ler_tmdb(FILME, 0, &tmp, &f) == 1);
  assert(f.tmdb == 550 && f.tipo == FF_FILME && f.ano == 1999);
  assert(f.idioma == (uint16_t)(('e' << 8) | 'n'));
  assert(f.minutos == 139 && f.nota == 84 && f.votos == 28700);
  assert(f.nGenero == 1 && f.genero[0] == 18);
  // Direcao primeiro, depois o elenco na ordem — 6 vagas, sem repetir.
  assert(f.nPessoa == 6 && f.pessoa[0] == 7467 && f.pessoa[1] == 819 &&
         f.pessoa[2] == 287 && f.pessoa[3] == 1283 && f.pessoa[4] == 7470 &&
         f.pessoa[5] == 7471);
  // 9 palavras, 8 vagas: a nona fica de fora.
  assert(tmp.nKw == 9 && f.nTema == 8 && f.tema[0] == 818 && f.tema[7] == 209411);
  assert(f.fonte == FF_DE_TMDB && f.reserva == 0);
  assert(farolficha_chave(&f) == chave(FF_FILME, 550));
  // Ids do TMDB sempre positivos.
  { int i; for (i = 0; i < f.nGenero + f.nPessoa + f.nTema; i++) {
      int32_t v = i < f.nGenero ? f.genero[i]
                : i < f.nGenero + f.nPessoa ? f.pessoa[i - f.nGenero]
                : f.tema[i - f.nGenero - f.nPessoa];
      assert(v > 0); } }
}

static void testarSerie(void) {
  assert(farolficha_ler_tmdb(SERIE, 1, &tmp, &f) == 1);
  assert(f.tmdb == 70523 && f.tipo == FF_SERIE && f.ano == 2017);
  assert(f.idioma == (uint16_t)(('d' << 8) | 'e'));
  assert(f.minutos == 53);            // episode_run_time[0]
  assert(f.nota == 84);               // a nota da RAIZ, nao a do episodio
  assert(f.nGenero == 3 && f.genero[0] == 9648);
  // Criadores primeiro e na ordem; o elenco vem depois.
  assert(f.nPessoa == 4 && f.pessoa[0] == 5001 && f.pessoa[1] == 5002 &&
         f.pessoa[2] == 6001 && f.pessoa[3] == 6002);
  // Serie le keywords.results (keywords.keywords nao existe em /tv).
  assert(f.nTema == 3 && f.tema[0] == 4379 && f.tema[2] == 10364);
  assert(farolficha_chave(&f) == chave(FF_SERIE, 70523));
  // Mesmo numero de id nao colide entre filme e serie.
  assert(chave(FF_FILME, 70523) != chave(FF_SERIE, 70523));
}

static void testarLacunas(void) {
  assert(farolficha_ler_tmdb(SEM_KW, 0, &tmp, &f) == 1);
  assert(f.nTema == 0 && f.nGenero == 1 && f.nPessoa == 2);
  assert(f.pessoa[0] == 22 && f.pessoa[1] == 11);
  assert(farolficha_ler_tmdb(SEM_CRED, 0, &tmp, &f) == 1);
  assert(f.nPessoa == 0 && f.nTema == 1 && f.tema[0] == 500);
  assert(f.nota == 64 && f.votos == 800);
}

static void testarAnime(void) {
  assert(farolficha_ler_tmdb(ANIME, 0, &tmp, &f) == 1);
  assert(f.tmdb == 129 && f.idioma == (uint16_t)(('j' << 8) | 'a'));
  assert(f.minutos == 125 && f.nGenero == 3 && f.genero[0] == 16);
  assert(f.nPessoa == 2 && f.pessoa[0] == 608 && f.pessoa[1] == 7001);
  assert(f.votos == 16200);
}

static void testarTruncado(void) {
  // O que chegou antes do corte vale; o que faltou fica zerado, sem travar.
  assert(farolficha_ler_tmdb(TRUNCADO, 0, &tmp, &f) == 1);
  assert(f.tmdb == 550 && f.nGenero == 1 && f.minutos == 139);
  assert(f.nTema == 0 && f.nPessoa == 0);     // o corte foi nas palavras
  assert(f.nota == 84 && f.votos == 28700);
  // Sem id nem lista nenhuma: nao e detalhe.
  assert(farolficha_ler_tmdb(TRUNCADO_CEDO, 0, &tmp, &f) == 0);
  assert(farolficha_ler_tmdb(NULL, 0, &tmp, &f) == 0);
  assert(farolficha_ler_tmdb("not-json", 0, &tmp, &f) == 0);
}

static void testarCatItem(void) {
  static CatItem ci;
  memset(&ci, 0, sizeof ci);
  snprintf(ci.titulo, sizeof ci.titulo, "Prisoners");
  snprintf(ci.tipo, sizeof ci.tipo, "movie");
  snprintf(ci.imdb, sizeof ci.imdb, "tt1392214");
  snprintf(ci.meta, sizeof ci.meta, "2013  \xc2\xb7  2h33");
  snprintf(ci.genero, sizeof ci.genero, "Filme  \xc2\xb7  Crime  \xc2\xb7  Drama");
  snprintf(ci.direcao, sizeof ci.direcao, "Denis Villeneuve");
  ci.nota = 81;
  snprintf(ci.elenco[0].nome, sizeof ci.elenco[0].nome, "Hugh Jackman");
  ci.elenco[0].tmdb = 6968;
  snprintf(ci.elenco[1].nome, sizeof ci.elenco[1].nome, "Jake Gyllenhaal");
  ci.elenco[1].tmdb = 131;
  snprintf(ci.elenco[2].nome, sizeof ci.elenco[2].nome, "Sem Id");
  ci.elenco[2].tmdb = 0;
  ci.nElenco = 3;

  assert(farolficha_ler_item(&ci, &f) == 1);
  assert(f.tmdb == 0 && f.tipo == FF_FILME && f.ano == 2013 && f.nota == 81);
  assert(f.minutos == 0 && f.idioma == 0 && f.votos == 0);
  assert(f.fonte == FF_DE_CAT && f.reserva > 0);
  // Genero/direcao/elenco sem TMDB viram ids NEGATIVOS e estaveis.
  assert(f.nGenero == 2 && f.genero[0] < 0 && f.genero[1] < 0);
  assert(f.genero[0] == (int32_t)mapa_hash_nome("Crime"));
  assert(f.genero[1] == (int32_t)mapa_hash_nome("Drama"));
  assert(f.nPessoa == 4);
  assert(f.pessoa[0] == (int32_t)mapa_hash_nome("Denis Villeneuve"));   // direcao primeiro
  assert(f.pessoa[1] == 6968 && f.pessoa[2] == 131);                    // elenco com id de verdade
  assert(f.pessoa[3] == (int32_t)mapa_hash_nome("Sem Id"));
  assert(f.pessoa[3] < 0);
  // Estavel: mesmo item, mesma ficha; nome diferente, id diferente.
  assert(farolficha_ler_item(&ci, &f2) == 1 && memcmp(&f, &f2, sizeof f) == 0);
  assert(mapa_hash_nome("Denis Villeneuve") != mapa_hash_nome("D. Villeneuve"));
  // A chave e negativa e identifica o item (hash do imdb, bit 30 ligado).
  assert(farolficha_chave(&f) < 0);
  assert(farolficha_chave(&f) == -(((int64_t)FF_FILME << 32) |
        (uint32_t)(0x40000000u | (mapa_hash_titulo("tt1392214") & 0x3fffffffu))));

  // Com id do TMDB resolvido no item, a chave volta a ser a positiva.
  ci.tmdb = 146233;
  assert(farolficha_ler_item(&ci, &f2) == 1 && f2.tmdb == 146233);
  assert(farolficha_chave(&f2) == chave(FF_FILME, 146233) && f2.reserva == 0);

  // Serie do catalogo muda o tipo — e o "tmdb:" no imdb tambem resolve.
  ci.tmdb = 0;
  snprintf(ci.imdb, sizeof ci.imdb, "tmdb:70523");
  snprintf(ci.tipo, sizeof ci.tipo, "series");
  assert(farolficha_ler_item(&ci, &f2) == 1 && f2.tmdb == 70523);
  assert(f2.tipo == FF_SERIE && farolficha_chave(&f2) == chave(FF_SERIE, 70523));
}

// O campo `genero` do CatItem tem dois formatos (farolficha.c:ehRotuloTipo):
// com o rotulo do tipo na frente (pacote/Trakt) e a lista crua, que e o que
// Jellyfin, Emby e Plex gravam. Nenhum dos dois pode perder genero.
static void testarGeneroDoisFormatos(void) {
  static CatItem ci;
  memset(&ci, 0, sizeof ci);
  snprintf(ci.titulo, sizeof ci.titulo, "Item");
  snprintf(ci.tipo, sizeof ci.tipo, "movie");
  snprintf(ci.imdb, sizeof ci.imdb, "tt0000001");

  // Lista crua, um genero so: "Drama" inteiro entra (era o 1o bug da r2).
  snprintf(ci.genero, sizeof ci.genero, "Drama");
  assert(farolficha_ler_item(&ci, &f) == 1);
  assert(f.nGenero == 1 && f.genero[0] == (int32_t)mapa_hash_nome("Drama"));

  // Lista crua com dois, no separador de espaco simples do Jellyfin/Plex.
  snprintf(ci.genero, sizeof ci.genero, "Drama \xc2\xb7 Crime");
  assert(farolficha_ler_item(&ci, &f) == 1);
  assert(f.nGenero == 2 && f.genero[0] == (int32_t)mapa_hash_nome("Drama") &&
         f.genero[1] == (int32_t)mapa_hash_nome("Crime"));

  // Com rotulo, inclusive localizado ("TV Show", "Serie"): o tipo sai e os
  // generos ficam — o formato que o pacote grava continua igual.
  snprintf(ci.genero, sizeof ci.genero, "Filme  \xc2\xb7  Crime  \xc2\xb7  Drama");
  assert(farolficha_ler_item(&ci, &f) == 1);
  assert(f.nGenero == 2 && f.genero[0] == (int32_t)mapa_hash_nome("Crime"));
  snprintf(ci.genero, sizeof ci.genero, "TV Show  \xc2\xb7  Drama");
  assert(farolficha_ler_item(&ci, &f) == 1);
  assert(f.nGenero == 1 && f.genero[0] == (int32_t)mapa_hash_nome("Drama"));

  // Rotulo sozinho (o Trakt grava assim na watchlist): nenhum genero.
  snprintf(ci.genero, sizeof ci.genero, "Programa de TV");
  assert(farolficha_ler_item(&ci, &f) == 1);
  assert(f.nGenero == 0);
}

// Ids distintos de servidor (jf:/em:/px:) geram chaves distintas, mesmo com
// titulo e ano iguais — e nao colidem com o espaco titulo+ano nem com TMDB.
static void testarChaveServidor(void) {
  static CatItem a, b;
  const char *ids[][2] = { { "jf:01234567.abcd", "jf:01234567.abce" },
                           { "em:01234567.abcd", "em:01234567.abce" },
                           { "px:01234567.20",   "px:01234567.21" } };
  int i;
  for (i = 0; i < 3; i++) {
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    snprintf(a.titulo, sizeof a.titulo, "Mesmo Nome");
    snprintf(b.titulo, sizeof b.titulo, "Mesmo Nome");
    snprintf(a.tipo, sizeof a.tipo, "movie");
    snprintf(b.tipo, sizeof b.tipo, "movie");
    snprintf(a.meta, sizeof a.meta, "2020");
    snprintf(b.meta, sizeof b.meta, "2020");
    snprintf(a.imdb, sizeof a.imdb, "%s", ids[i][0]);
    snprintf(b.imdb, sizeof b.imdb, "%s", ids[i][1]);
    assert(farolficha_ler_item(&a, &f) == 1);
    assert(farolficha_ler_item(&b, &f2) == 1);
    assert(f.reserva > 0 && f2.reserva > 0 && f.reserva != f2.reserva);
    assert(farolficha_chave(&f) != farolficha_chave(&f2));
    // Id estavel: bit 30 ligado; titulo+ano usa o outro espaco.
    assert((f.reserva & 0x40000000) != 0 && (f2.reserva & 0x40000000) != 0);
    b.imdb[0] = 0;
    assert(farolficha_ler_item(&b, &f2) == 1);
    assert(!(f2.reserva & 0x40000000));
    assert(farolficha_chave(&f) != farolficha_chave(&f2));
  }
  // O mesmo imdb nos dois caminhos (item e detalhe sem tmdb) reconcilia.
  assert(farolficha_ler_tmdb(
      "{\"imdb_id\":\"tt7654321\",\"original_language\":\"en\","
      "\"genres\":[{\"id\":18,\"name\":\"Drama\"}]}", 0, &tmp, &f) == 1);
  assert(f.tmdb == 0 && f.reserva == (int32_t)(0x40000000u |
         (mapa_hash_titulo("tt7654321") & 0x3fffffffu)));
  memset(&a, 0, sizeof a);
  snprintf(a.titulo, sizeof a.titulo, "Qualquer");
  snprintf(a.imdb, sizeof a.imdb, "tt7654321");
  assert(farolficha_ler_item(&a, &f2) == 1);
  assert(f.reserva == f2.reserva);
}

static FarolFichaCache c, c2;

static FarolFicha ficha(long tmdb) {
  FarolFicha x;
  memset(&x, 0, sizeof x);
  x.tmdb = (int32_t)tmdb;
  x.tipo = FF_FILME;
  x.ano = 2000;
  x.fonte = FF_DE_TMDB;
  x.genero[0] = 18; x.nGenero = 1;
  return x;
}

static void testarCache(const char *dir) {
  char caminho[512], ruim[512];
  long i;
  FILE *fp;

  snprintf(caminho, sizeof caminho, "%s/farol-fichas.bin", dir);
  snprintf(ruim, sizeof ruim, "%s/farol-fichas-ruim.bin", dir);
  unlink(caminho);
  unlink(ruim);

  farolficha_cache_iniciar(&c);
  assert(sizeof(FarolFichaCache) <= 60 * 1024);

  // Miss, hit e promocao.
  assert(farolficha_obter(&c, chave(FF_FILME, 1), NULL) == 0);
  f = ficha(1); farolficha_guardar(&c, &f);
  f = ficha(2); farolficha_guardar(&c, &f);
  f = ficha(3); farolficha_guardar(&c, &f);
  assert(c.n == 3);
  assert(farolficha_obter(&c, chave(FF_FILME, 1), &f2) == 1 && f2.tmdb == 1);
  assert(farolficha_chave(&c.f[0]) == chave(FF_FILME, 1));   // promovida

  // Atualizar nao duplica: mesma chave, valor novo, vai para a frente.
  f = ficha(3); f.nota = 99;
  farolficha_guardar(&c, &f);
  assert(c.n == 3 && c.f[0].tmdb == 3 && c.f[0].nota == 99);

  // Cheio: entra a 601a, sai a mais velha — menos a que foi tocada.
  farolficha_cache_iniciar(&c);
  for (i = 1; i <= FAROL_FICHA_MAX; i++) { f = ficha(i); farolficha_guardar(&c, &f); }
  assert(c.n == FAROL_FICHA_MAX);
  assert(farolficha_obter(&c, chave(FF_FILME, 1), NULL) == 1);    // toca a mais velha
  f = ficha(601); farolficha_guardar(&c, &f);
  assert(c.n == FAROL_FICHA_MAX);
  assert(farolficha_obter(&c, chave(FF_FILME, 601), NULL) == 1);
  assert(farolficha_obter(&c, chave(FF_FILME, 1), NULL) == 1);    // promovida: ficou
  assert(farolficha_obter(&c, chave(FF_FILME, 2), NULL) == 0);    // a nova mais velha saiu

  // Ficha sem identidade nao entra.
  memset(&f2, 0, sizeof f2); f2.tipo = FF_FILME;
  farolficha_guardar(&c, &f2);
  assert(c.n == FAROL_FICHA_MAX);

  // Ida e volta do arquivo: mesma ordem e mesmo conteudo.
  farolficha_cache_iniciar(&c);
  for (i = 1; i <= 7; i++) { f = ficha(i); f.nota = (uint8_t)i; farolficha_guardar(&c, &f); }
  assert(c.sujo);
  assert(farolficha_gravar(&c, caminho) == 1 && !c.sujo);
  assert(farolficha_ler_arquivo(&c2, caminho) == 1 && c2.n == 7);
  for (i = 0; i < 7; i++)
    assert(c2.f[i].tmdb == c.f[i].tmdb && c2.f[i].nota == c.f[i].nota);

  // Arquivo inexistente: 0, cache vazio, nada apagado.
  farolficha_cache_iniciar(&c);
  assert(farolficha_ler_arquivo(&c, ruim) == 0 && c.n == 0);

  // Corrompidos, um a um: todos descartados e apagados, sem travar.
  { unsigned char hdr[12];
    // Texto que nem e o formato.
    fp = fopen(ruim, "wb"); assert(fp);
    fwrite("lixo de verdade, nao e nem binario", 1, 32, fp);
    fclose(fp);
    assert(farolficha_ler_arquivo(&c, ruim) == 0 && c.n == 0);
    assert(access(ruim, F_OK) != 0);
    // Versao futura do formato.
    fp = fopen(ruim, "wb"); assert(fp);
    memcpy(hdr, "FFICHA\x00\x09", 8);
    hdr[8] = 1; hdr[9] = 0; hdr[10] = 96; hdr[11] = 0;
    fwrite(hdr, 1, 12, fp);
    f = ficha(1); fwrite(&f, 1, sizeof f, fp);
    fclose(fp);
    assert(farolficha_ler_arquivo(&c, ruim) == 0 && access(ruim, F_OK) != 0);
    // n alem do teto.
    fp = fopen(ruim, "wb"); assert(fp);
    memcpy(hdr, "FFICHA\x00\x01", 8);
    hdr[8] = 0xff; hdr[9] = 0xff; hdr[10] = 96; hdr[11] = 0;
    fwrite(hdr, 1, 12, fp);
    fclose(fp);
    assert(farolficha_ler_arquivo(&c, ruim) == 0 && access(ruim, F_OK) != 0);
    // Truncado no meio da 3a ficha.
    fp = fopen(ruim, "wb"); assert(fp);
    memcpy(hdr, "FFICHA\x00\x01", 8);
    hdr[8] = 3; hdr[9] = 0; hdr[10] = 96; hdr[11] = 0;
    fwrite(hdr, 1, 12, fp);
    f = ficha(1); fwrite(&f, 1, sizeof f, fp);
    fwrite(&f, 1, 40, fp);                       // meio registro
    fclose(fp);
    assert(farolficha_ler_arquivo(&c, ruim) == 0 && access(ruim, F_OK) != 0);
    // Lixo no fim.
    fp = fopen(ruim, "wb"); assert(fp);
    hdr[8] = 1; hdr[9] = 0;
    fwrite(hdr, 1, 12, fp);
    fwrite(&f, 1, sizeof f, fp);
    fputc(0xAA, fp);
    fclose(fp);
    assert(farolficha_ler_arquivo(&c, ruim) == 0 && access(ruim, F_OK) != 0);
    // Duas fichas com a mesma chave: o gravador nao produz; corrompido.
    fp = fopen(ruim, "wb"); assert(fp);
    memcpy(hdr, "FFICHA\x00\x01", 8);
    hdr[8] = 2; hdr[9] = 0; hdr[10] = 96; hdr[11] = 0;
    fwrite(hdr, 1, 12, fp);
    f = ficha(1); f.nota = 80; fwrite(&f, 1, sizeof f, fp);
    f.nota = 10;                 fwrite(&f, 1, sizeof f, fp);
    fclose(fp);
    assert(farolficha_ler_arquivo(&c, ruim) == 0 && c.n == 0);
    assert(access(ruim, F_OK) != 0);
  }

  // Cache vazio grava um cabecalho valido que volta com n == 0.
  farolficha_cache_iniciar(&c);
  assert(farolficha_gravar(&c, caminho) == 1);
  assert(farolficha_ler_arquivo(&c2, caminho) == 1 && c2.n == 0);
  unlink(caminho);
}

int main(void) {
  char dir[512];
  const char *td = getenv("TMPDIR");
  if (!td || !td[0]) td = "/tmp";
  snprintf(dir, sizeof dir, "%s/farol-ficha-teste.%d", td, (int)getpid());
  assert(mkdir(dir, 0700) == 0);

  testarFilme();
  testarSerie();
  testarLacunas();
  testarAnime();
  testarTruncado();
  testarCatItem();
  testarGeneroDoisFormatos();
  testarChaveServidor();
  testarCache(dir);

  rmdir(dir);
  puts("farol_ficha: ok");
  return 0;
}
