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


// --- Explorar 2.0: vizinhanca e climas ------------------------------------------

static const char *DISCOVER =
  "{\"page\":1,\"results\":["
  "{\"id\":11,\"name\":\"Dark\",\"poster_path\":\"/d.jpg\",\"first_air_date\":\"2017-12-01\",\"vote_average\":8.4},"
  "{\"id\":12,\"name\":\"Sem cartaz\",\"poster_path\":null},"
  "{\"id\":13,\"name\":\"1899\",\"poster_path\":\"/n.jpg\",\"first_air_date\":\"2022-11-17\"}"
  "],\"total_pages\":9,\"total_results\":170}";

static const char *KEYWORDS =
  "{\"page\":1,\"results\":[{\"id\":99,\"name\":\"time travel machine\"},"
  "{\"id\":4379,\"name\":\"Time Travel\"}],\"total_results\":2}";

static MapaVizCand cand(const char *titulo, int ano, const char *g1, const char *g2,
                        const char *p1, int idx) {
  MapaVizCand c;
  memset(&c, 0, sizeof c);
  snprintf(c.obra.titulo, sizeof c.obra.titulo, "%s", titulo);
  snprintf(c.obra.tipo, sizeof c.obra.tipo, "movie");
  snprintf(c.obra.poster, sizeof c.obra.poster, "p/%s.jpg", titulo);
  c.obra.ano = ano;
  c.obra.nota = 70;
  c.obra.catIndice = idx;
  if (g1) c.gen[c.nGen++] = mapa_hash_titulo(g1);
  if (g2) c.gen[c.nGen++] = mapa_hash_titulo(g2);
  if (p1) c.gente[c.nGente++] = mapa_hash_titulo(p1);
  return c;
}

static int temTitulo(const MapaVizGrupo *g, const char *t) {
  int i;
  for (i = 0; i < g->n; i++) if (!strcmp(g->itens[i].obra.titulo, t)) return 1;
  return 0;
}

static void testarVizinhanca(void) {
  static MapaSemente foco;
  static MapaVizinhos v;
  MapaVizCand pool[8];
  MapaVizAmigo amg[2];
  MapaVizEntrada e;
  MapaObra lista[4];
  unsigned visto;
  int total = 0, i, j, gi, gj;

  // Leituras de /discover e /search/keyword.
  assert(mapa_ler_lista(DISCOVER, 1, lista, 4, &total) == 2);   // o sem cartaz fica de fora
  assert(total == 170 && !strcmp(lista[0].tipo, "series") && lista[1].tmdb == 13);
  assert(mapa_ler_keyword_id(KEYWORDS, "time travel") == 4379);  // nome exato, sem caixa
  assert(mapa_ler_keyword_id(KEYWORDS, "wormhole") == 0);

  // Reserva local: so catalogo. Foco "Prisoners" (Crime/Drama, Villeneuve).
  memset(&foco, 0, sizeof foco);
  snprintf(foco.obra.titulo, sizeof foco.obra.titulo, "Prisoners");
  snprintf(foco.obra.tipo, sizeof foco.obra.tipo, "movie");
  snprintf(foco.obra.poster, sizeof foco.obra.poster, "p/pr.jpg");
  foco.obra.ano = 2013;
  foco.obra.catIndice = 0;
  snprintf(foco.gen[0].nome, sizeof foco.gen[0].nome, "Crime");
  snprintf(foco.gen[1].nome, sizeof foco.gen[1].nome, "Drama");
  foco.nGen = 2;
  snprintf(foco.gente[0].nome, sizeof foco.gente[0].nome, "Denis Villeneuve");
  foco.gente[0].id = -77; foco.gente[0].direcao = 1;
  snprintf(foco.gente[1].nome, sizeof foco.gente[1].nome, "Hugh Jackman");
  foco.gente[1].id = -78;
  foco.nGente = 2;
  pool[0] = cand("Sicario", 2015, "Crime", "Drama", "Denis Villeneuve", 1);
  pool[1] = cand("Arrival", 2016, "Drama", NULL, "Denis Villeneuve", 2);
  pool[2] = cand("Zodiac", 2007, "Crime", "Drama", NULL, 3);
  pool[3] = cand("Heat", 1995, "Crime", NULL, NULL, 4);
  pool[4] = cand("Logan", 2017, "Action", NULL, "Hugh Jackman", 5);
  pool[5] = cand("Prisoners", 2013, "Crime", NULL, NULL, 6);   // o proprio foco nunca volta
  amg[0].obra = pool[2].obra; snprintf(amg[0].quem, sizeof amg[0].quem, "Ana");   // ja usado acima
  amg[1].obra = pool[3].obra; snprintf(amg[1].quem, sizeof amg[1].quem, "Bia");
  visto = mapa_hash_titulo("Sicario");
  memset(&e, 0, sizeof e);
  e.foco = &foco;
  e.pool = pool; e.nPool = 6;
  e.amigos = amg; e.nAmigos = 2;
  e.vistos = &visto; e.nVistos = 1;
  v.revisao = 42;
  mapa_vizinhos_montar(&e, &v);
  assert(v.revisao == 42 && !v.remoto);
  assert(!strcmp(v.generos, "Crime  \xc2\xb7  Drama"));
  // Pessoa: Villeneuve, que aparece em mais titulos do catalogo; o motivo e o nome.
  assert(v.g[MAPA_VIZ_PESSOA].tipo == MAPA_GR_PESSOA);
  assert(!strcmp(v.g[MAPA_VIZ_PESSOA].sub, "Denis Villeneuve") && v.g[MAPA_VIZ_PESSOA].ref == -77);
  assert(v.g[MAPA_VIZ_PESSOA].n == 2 && temTitulo(&v.g[MAPA_VIZ_PESSOA], "Sicario"));
  assert(!strcmp(v.g[MAPA_VIZ_PESSOA].itens[0].motivo, "Denis Villeneuve"));
  assert(v.g[MAPA_VIZ_PESSOA].itens[0].visto == 1);    // Sicario ja comecado: marcado, nao escondido
  // Tema sem TMDB: o genero do catalogo.
  assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_GENERO && !strcmp(v.g[MAPA_VIZ_TEMA].sub, "Crime"));
  assert(temTitulo(&v.g[MAPA_VIZ_TEMA], "Zodiac"));    // divide os DOIS generos: primeiro
  assert(!strcmp(v.g[MAPA_VIZ_TEMA].itens[0].obra.titulo, "Zodiac"));
  assert(v.g[MAPA_VIZ_AMIGOS].n == 0 || !temTitulo(&v.g[MAPA_VIZ_AMIGOS], "Zodiac"));
  // Nenhum titulo em dois grupos, e o foco em nenhum.
  for (gi = 0; gi < MAPA_VIZ_GRUPOS; gi++)
    for (i = 0; i < v.g[gi].n; i++) {
      assert(strcmp(v.g[gi].itens[i].obra.titulo, "Prisoners"));
      for (gj = 0; gj < MAPA_VIZ_GRUPOS; gj++)
        for (j = 0; j < v.g[gj].n; j++)
          if (gi != gj || i != j)
            assert(strcmp(v.g[gi].itens[i].obra.titulo, v.g[gj].itens[j].obra.titulo));
    }

  // O fio: descer por Hugh Jackman mantem Hugh Jackman, mesmo com Villeneuve
  // rendendo mais; descer pelo Drama mantem o Drama.
  e.pessoaPref = mapa_hash_titulo("Hugh Jackman");
  e.generoPref = mapa_hash_titulo("Drama");
  mapa_vizinhos_montar(&e, &v);
  assert(!strcmp(v.g[MAPA_VIZ_PESSOA].sub, "Hugh Jackman") && temTitulo(&v.g[MAPA_VIZ_PESSOA], "Logan"));
  assert(!strcmp(v.g[MAPA_VIZ_TEMA].sub, "Drama"));

  // Ninguem em comum: a mesma decada.
  foco.nGente = 0;
  e.pessoaPref = e.generoPref = 0;
  mapa_vizinhos_montar(&e, &v);
  assert(v.g[MAPA_VIZ_PESSOA].tipo == MAPA_GR_EPOCA && !strcmp(v.g[MAPA_VIZ_PESSOA].sub, "2010"));
  assert(!strcmp(v.g[MAPA_VIZ_PESSOA].itens[0].obra.titulo, "Sicario"));   // o ano mais perto primeiro

  // Com TMDB: creditos e /discover viram os grupos; recomendados da semente.
  { MapaCreditos cr;
    MapaObra tema[2];
    memset(&cr, 0, sizeof cr);
    cr.pessoa = 137427; cr.n = 1;
    snprintf(cr.obras[0].titulo, sizeof cr.obras[0].titulo, "Dune");
    snprintf(cr.obras[0].poster, sizeof cr.obras[0].poster, "p/dune.jpg");
    cr.obras[0].tmdb = 438631; cr.obras[0].catIndice = -1;
    memset(tema, 0, sizeof tema);
    snprintf(tema[0].titulo, sizeof tema[0].titulo, "Gone Girl");
    snprintf(tema[0].poster, sizeof tema[0].poster, "p/gg.jpg");
    tema[0].tmdb = 210577; tema[0].catIndice = -1;
    tema[1] = cr.obras[0];                    // ja esta na pessoa: nao repete
    foco.quando = 1;
    foco.nRec = 1;
    snprintf(foco.rec[0].o.titulo, sizeof foco.rec[0].o.titulo, "Enemy");
    snprintf(foco.rec[0].o.poster, sizeof foco.rec[0].o.poster, "p/en.jpg");
    foco.rec[0].o.tmdb = 181886; foco.rec[0].o.catIndice = -1;
    e.cred = &cr; e.credNome = "Denis Villeneuve";
    e.tema = tema; e.nTema = 2; e.temaNome = "sequestro"; e.temaKw = "kidnapping"; e.temaId = 1930;
    mapa_vizinhos_montar(&e, &v);
    assert(v.remoto);
    assert(v.g[MAPA_VIZ_PESSOA].ref == 137427 && v.g[MAPA_VIZ_PESSOA].n == 1);
    assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_TEMA && v.g[MAPA_VIZ_TEMA].n == 1);
    assert(!strcmp(v.g[MAPA_VIZ_TEMA].kw, "kidnapping") && v.g[MAPA_VIZ_TEMA].ref == 1930);
    assert(!strcmp(v.g[MAPA_VIZ_TEMA].itens[0].motivo, "sequestro"));
    // Remoto: so os recomendados do TMDB, sem o enchimento do catalogo.
    assert(v.g[MAPA_VIZ_REC].n == 1 && temTitulo(&v.g[MAPA_VIZ_REC], "Enemy")); }

  // Genero canonico propagado por id TMDB (Animation id 16): casa com candidato
  // cuja genMasc veio do catalogo em ingles, sem depender do texto do selo.
  { MapaSemente sf;
    MapaVizCand animPool[2];
    MapaVizEntrada ea;
    memset(&sf, 0, sizeof sf);
    snprintf(sf.obra.titulo, sizeof sf.obra.titulo, "Spirited Away");
    snprintf(sf.obra.tipo, sizeof sf.obra.tipo, "movie");
    sf.gen[0].id = 16;
    snprintf(sf.gen[0].nome, sizeof sf.gen[0].nome, "Animation");
    sf.nGen = 1;
    animPool[0] = cand("Toy Story", 1995, "Animation", NULL, NULL, 10);
    animPool[0].genMasc = mapa_genero_mascara("Animation");
    animPool[1] = cand("Heat", 1995, "Crime", NULL, NULL, 11);
    animPool[1].genMasc = mapa_genero_mascara("Crime");
    memset(&ea, 0, sizeof ea);
    ea.foco = &sf;
    ea.pool = animPool; ea.nPool = 2;
    ea.generoPref = mapa_hash_titulo("Anima\xc3\xa7\xc3\xa3o");
    ea.generoPrefId = 16;
    ea.generoPrefMasc = 1u << MAPA_G_ANIMACAO;
    ea.generoPrefNome = "Anima\xc3\xa7\xc3\xa3o";
    mapa_vizinhos_montar(&ea, &v);
    assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_GENERO);
    assert(v.g[MAPA_VIZ_TEMA].ref == 16);
    assert(!strcmp(v.g[MAPA_VIZ_TEMA].sub, "Anima\xc3\xa7\xc3\xa3o"));
    assert(v.g[MAPA_VIZ_TEMA].n == 1 && temTitulo(&v.g[MAPA_VIZ_TEMA], "Toy Story"));

    // Mesma decada: selo explicito de genero (16) retem o unico candidato
    // elegivel mesmo quando ele tambem serve para MAPA_GR_EPOCA, preservando
    // os demais da mesma decada no grupo de epoca sem duplicacao; rota
    // generica sem preferencia mantem a prioridade original da epoca.
    { MapaVizCand decPool[3];
      MapaVizEntrada ed;
      sf.obra.ano = 2024;
      sf.nGente = 0;
      sf.nGen = 2;
      sf.gen[0].id = 18; snprintf(sf.gen[0].nome, sizeof sf.gen[0].nome, "Drama");
      sf.gen[1].id = 16; snprintf(sf.gen[1].nome, sizeof sf.gen[1].nome, "Animation");
      decPool[0] = cand("Drama Um", 2024, "Drama", NULL, NULL, 20);
      decPool[0].genMasc = mapa_genero_mascara("Drama");
      decPool[1] = cand("Unica Animacao", 2024, "Animation", NULL, NULL, 21);
      decPool[1].genMasc = mapa_genero_mascara("Animation");
      decPool[2] = cand("Drama Dois", 2024, "Drama", NULL, NULL, 22);
      decPool[2].genMasc = mapa_genero_mascara("Drama");
      memset(&ed, 0, sizeof ed);
      ed.foco = &sf;
      ed.pool = decPool; ed.nPool = 3;
      ed.generoPrefId = 16;
      ed.generoPrefMasc = 1u << MAPA_G_ANIMACAO;
      ed.generoPrefNome = "Anima\xc3\xa7\xc3\xa3o";
      mapa_vizinhos_montar(&ed, &v);
      assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_GENERO && v.g[MAPA_VIZ_TEMA].ref == 16);
      assert(v.g[MAPA_VIZ_TEMA].n == 1 && temTitulo(&v.g[MAPA_VIZ_TEMA], "Unica Animacao"));
      assert(v.g[MAPA_VIZ_PESSOA].tipo == MAPA_GR_EPOCA && v.g[MAPA_VIZ_PESSOA].n == 2);
      assert(!temTitulo(&v.g[MAPA_VIZ_PESSOA], "Unica Animacao"));
      assert(temTitulo(&v.g[MAPA_VIZ_PESSOA], "Drama Um") && temTitulo(&v.g[MAPA_VIZ_PESSOA], "Drama Dois"));

      // Rota generica (sem preferencia): preserva ordem e dedup originais.
      ed.generoPrefId = 0; ed.generoPrefMasc = 0; ed.generoPrefNome = NULL;
      mapa_vizinhos_montar(&ed, &v);
      assert(v.g[MAPA_VIZ_PESSOA].tipo == MAPA_GR_EPOCA && v.g[MAPA_VIZ_PESSOA].n == 3);
      assert(temTitulo(&v.g[MAPA_VIZ_PESSOA], "Unica Animacao"));
    }

    // ID conhecido sem rotulo + mascara do foco localizada (ex.: Komodie):
    // resolve localmente sem depender do texto em PT/EN/ES, e tambem quando
    // o foco nao traz a etiqueta (fallback por genTmdbNomePt).
    { MapaSemente sfDe;
      MapaVizCand dePool[2];
      MapaVizEntrada ede;
      memset(&sfDe, 0, sizeof sfDe);
      snprintf(sfDe.obra.titulo, sizeof sfDe.obra.titulo, "Foco DE");
      snprintf(sfDe.obra.tipo, sizeof sfDe.obra.tipo, "movie");
      sfDe.obra.ano = 2024;
      sfDe.nGen = 2;
      sfDe.gen[0].id = -1; snprintf(sfDe.gen[0].nome, sizeof sfDe.gen[0].nome, "Drama");
      sfDe.gen[1].id = -2; snprintf(sfDe.gen[1].nome, sizeof sfDe.gen[1].nome, "Kom\xc3\xb6" "die");
      dePool[0] = cand("Drama DE", 2024, "Drama", NULL, NULL, 30);
      dePool[0].genMasc = mapa_genero_mascara("Drama");
      dePool[1] = cand("Comedia DE", 2024, "Kom\xc3\xb6" "die", NULL, NULL, 31);
      dePool[1].genMasc = 1u << MAPA_G_COMEDIA;
      memset(&ede, 0, sizeof ede);
      ede.foco = &sfDe;
      ede.pool = dePool; ede.nPool = 2;
      ede.generoPrefId = 35;
      ede.generoPrefMasc = 1u << MAPA_G_COMEDIA;
      ede.focoGenMasc[0] = 1u << MAPA_G_DRAMA;
      ede.focoGenMasc[1] = 1u << MAPA_G_COMEDIA;
      mapa_vizinhos_montar(&ede, &v);
      assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_GENERO && v.g[MAPA_VIZ_TEMA].ref == 35);
      assert(v.g[MAPA_VIZ_TEMA].n == 1 && temTitulo(&v.g[MAPA_VIZ_TEMA], "Comedia DE"));

      sfDe.nGen = 0;
      mapa_vizinhos_montar(&ede, &v);
      assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_GENERO && v.g[MAPA_VIZ_TEMA].ref == 35);
      assert(v.g[MAPA_VIZ_TEMA].n == 1 && temTitulo(&v.g[MAPA_VIZ_TEMA], "Comedia DE"));
      assert(!strcmp(v.g[MAPA_VIZ_TEMA].sub, "Com\xc3\xa9" "dia"));
    }

    // Rotulo livre conflitante ("Drama") nao vence sobre ID conhecido (16).
    { MapaVizCand cfPool[2];
      MapaVizEntrada ec;
      sf.nGen = 2;
      sf.gen[0].id = 18; snprintf(sf.gen[0].nome, sizeof sf.gen[0].nome, "Drama");
      sf.gen[1].id = 16; snprintf(sf.gen[1].nome, sizeof sf.gen[1].nome, "Animation");
      cfPool[0] = cand("Drama A", 2004, "Drama", NULL, NULL, 40);
      cfPool[0].genMasc = mapa_genero_mascara("Drama");
      cfPool[1] = cand("Anim A", 2004, "Animation", NULL, NULL, 41);
      cfPool[1].genMasc = mapa_genero_mascara("Animation");
      memset(&ec, 0, sizeof ec);
      ec.foco = &sf;
      ec.pool = cfPool; ec.nPool = 2;
      ec.generoPref = mapa_hash_titulo("Drama");
      ec.generoPrefId = 16;
      ec.generoPrefMasc = 1u << MAPA_G_ANIMACAO;
      ec.generoPrefNome = "Drama";
      mapa_vizinhos_montar(&ec, &v);
      assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_GENERO && v.g[MAPA_VIZ_TEMA].ref == 16);
      assert(v.g[MAPA_VIZ_TEMA].n == 1 && temTitulo(&v.g[MAPA_VIZ_TEMA], "Anim A"));
      assert(!strcmp(v.g[MAPA_VIZ_TEMA].sub, "Animation"));
    }

    // Chamada legada sem ID preserva casamento textual; variantes oficiais
    // TV/filme (10768 <-> War, 10759 <-> Action) seguem casando.
    { MapaSemente sfLeg;
      MapaVizCand legPool[3];
      MapaVizEntrada el;
      memset(&sfLeg, 0, sizeof sfLeg);
      snprintf(sfLeg.obra.titulo, sizeof sfLeg.obra.titulo, "Foco Legado");
      snprintf(sfLeg.obra.tipo, sizeof sfLeg.obra.tipo, "series");
      sfLeg.nGen = 2;
      snprintf(sfLeg.gen[0].nome, sizeof sfLeg.gen[0].nome, "Cyberpunk");
      sfLeg.gen[1].id = 10768;
      snprintf(sfLeg.gen[1].nome, sizeof sfLeg.gen[1].nome, "War & Politics");
      legPool[0] = cand("Outro", 2004, "Drama", NULL, NULL, 50);
      legPool[1] = cand("Neon", 2004, "Cyberpunk", NULL, NULL, 51);
      legPool[2] = cand("Guerra Filme", 2004, "War", NULL, NULL, 52);
      legPool[2].genMasc = mapa_genero_mascara("War");
      memset(&el, 0, sizeof el);
      el.foco = &sfLeg;
      el.pool = legPool; el.nPool = 3;
      el.generoPref = mapa_hash_titulo("Cyberpunk");
      el.generoPrefNome = "Cyberpunk";
      mapa_vizinhos_montar(&el, &v);
      assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_GENERO && v.g[MAPA_VIZ_TEMA].n == 1);
      assert(temTitulo(&v.g[MAPA_VIZ_TEMA], "Neon") && !strcmp(v.g[MAPA_VIZ_TEMA].sub, "Cyberpunk"));

      memset(&el, 0, sizeof el);
      el.foco = &sfLeg;
      el.pool = legPool; el.nPool = 3;
      el.generoPrefId = 10768;
      el.generoPrefMasc = 1u << MAPA_G_GUERRA;
      el.generoPrefNome = "War";
      mapa_vizinhos_montar(&el, &v);
      assert(v.g[MAPA_VIZ_TEMA].tipo == MAPA_GR_GENERO && v.g[MAPA_VIZ_TEMA].ref == 10768);
      assert(v.g[MAPA_VIZ_TEMA].n == 1 && temTitulo(&v.g[MAPA_VIZ_TEMA], "Guerra Filme"));
    } }

  // Nada: sem foco, tudo vazio, sem estourar.
  e.foco = NULL;
  mapa_vizinhos_montar(&e, &v);
  for (gi = 0; gi < MAPA_VIZ_GRUPOS; gi++) assert(v.g[gi].n == 0);
}

static void testarClimas(void) {
  const char *kws[2] = { "Time Travel", "family" };
  long ids[4] = { 4379, 0, 1234, 0 };
  char q[160];
  unsigned drama = mapa_genero_mascara("Drama");
  unsigned misterio = mapa_genero_mascara(" Mistério ");
  unsigned scifi = mapa_genero_mascara("Ficção científica");
  int i;
  assert(drama == 1u << MAPA_G_DRAMA && misterio == 1u << MAPA_G_MISTERIO);
  assert(scifi == mapa_genero_mascara("Science Fiction"));
  assert(mapa_genero_mascara("Sci-Fi & Fantasy") == ((1u << MAPA_G_FICCAO) | (1u << MAPA_G_FANTASIA)));
  assert(mapa_genero_mascara("Animation") == (1u << MAPA_G_ANIMACAO) &&
         mapa_genero_mascara("Anima\xc3\xa7\xc3\xa3o") == (1u << MAPA_G_ANIMACAO) &&
         mapa_genero_mascara("Animaci\xc3\xb3n") == (1u << MAPA_G_ANIMACAO));
  assert(mapa_genero_mascara("Documentary") == (1u << MAPA_G_DOCUMENTARIO) &&
         mapa_genero_mascara("Document\xc3\xa1rio") == (1u << MAPA_G_DOCUMENTARIO) &&
         mapa_genero_mascara("Documental") == (1u << MAPA_G_DOCUMENTARIO));
  assert(mapa_genero_mascara("History") == (1u << MAPA_G_HISTORIA) &&
         mapa_genero_mascara("Hist\xc3\xb3ria") == (1u << MAPA_G_HISTORIA));
  assert(mapa_genero_mascara("Music") == (1u << MAPA_G_MUSICA) &&
         mapa_genero_mascara("M\xc3\xbasica") == (1u << MAPA_G_MUSICA));
  assert(mapa_genero_mascara("Romance") == (1u << MAPA_G_ROMANCE));
  assert(mapa_genero_mascara("War") == (1u << MAPA_G_GUERRA) &&
         mapa_genero_mascara("War & Politics") == (1u << MAPA_G_GUERRA) &&
         mapa_genero_mascara("Guerra") == (1u << MAPA_G_GUERRA) &&
         mapa_genero_mascara("B\xc3\xa9lica") == (1u << MAPA_G_GUERRA));
  assert(mapa_genero_mascara("Western") == (1u << MAPA_G_FAROESTE) &&
         mapa_genero_mascara("Faroeste") == (1u << MAPA_G_FAROESTE));
  assert(mapa_genero_mascara("Kids") == (1u << MAPA_G_INFANTIL) &&
         mapa_genero_mascara("Infantil") == (1u << MAPA_G_INFANTIL) &&
         mapa_genero_mascara("News") == (1u << MAPA_G_NOTICIAS) &&
         mapa_genero_mascara("Not\xc3\xad" "cias") == (1u << MAPA_G_NOTICIAS) &&
         mapa_genero_mascara("Reality") == (1u << MAPA_G_REALITY) &&
         mapa_genero_mascara("Soap") == (1u << MAPA_G_NOVELA) &&
         mapa_genero_mascara("Novela") == (1u << MAPA_G_NOVELA) &&
         mapa_genero_mascara("Talk show") == (1u << MAPA_G_TALK));
  assert(mapa_genero_mascara("Filme") == 0 && mapa_genero_mascara(NULL) == 0);
  // Todo clima tem nome e descricao (chaves de i18n) e nenhum id fora da tabela.
  for (i = 0; i < MAPA_CLIMA_N; i++) assert(mapa_clima_nome(i)[0] && mapa_clima_descricao(i)[0]);
  assert(!mapa_clima_nome(MAPA_CLIMA_N)[0] && !mapa_clima_casa(-1, ~0u, NULL, 0, 0));
  // "Tempo bagunçado" (1): pela palavra quando ha; no catalogo, Ficcao E Misterio.
  assert(mapa_clima_casa(1, 0, kws, 2, 0));
  assert(!mapa_clima_casa(1, scifi, kws + 1, 1, 0));
  assert(mapa_clima_casa(1, scifi | misterio, NULL, 0, 0));
  assert(!mapa_clima_casa(1, scifi, NULL, 0, 0));
  // "Mistério para maratonar" (6): so serie.
  assert(mapa_clima_casa(6, misterio, NULL, 0, 1) && !mapa_clima_casa(6, misterio, NULL, 0, 0));
  assert(!mapa_clima_consulta(6, 0, NULL, 0, q, sizeof q));
  assert(mapa_clima_consulta(6, 1, NULL, 0, q, sizeof q) && !strcmp(q, "with_genres=9648"));
  // Consulta por palavra: so as resolvidas, com o OU escapado.
  assert(mapa_clima_consulta(1, 0, ids, 4, q, sizeof q));
  assert(!strcmp(q, "with_keywords=4379%7C1234"));
  { long nada[4] = { 0, 0, 0, 0 };
    assert(!mapa_clima_consulta(1, 0, nada, 4, q, sizeof q) && !q[0]); }
  // "Terror sem exagero" (9): genero E palavra; a TV nao tem Terror, vai so pela palavra.
  assert(mapa_clima_consulta(9, 0, ids, 1, q, sizeof q) && !strcmp(q, "with_genres=27&with_keywords=4379"));
  assert(mapa_clima_consulta(9, 1, ids, 1, q, sizeof q) && !strcmp(q, "with_keywords=4379"));
  assert(mapa_clima_n_kw(9) == 3 && !strcmp(mapa_clima_kw(9, 0), "supernatural") && !mapa_clima_kw(9, 3));
}

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

  testarVizinhanca();
  testarClimas();

  puts("mapa: ok");
  return 0;
}
