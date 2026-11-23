// The Simkl library from the inside (src/simkl.c): /sync/all-items answers
// (one per type, or per status over max_items) joined into the one body it
// keeps, status lists built from it, and how often list readers check it
// with Simkl (SMK_BIB_FRESCA, the real value here). Built like tests/i610.c: simkl.c is included so its static helpers
// are reachable, and simkl_cw.c brings the stubs and the fake account.
#include "../src/simkl.c"
#define main simkl_cw_main
#define atividades fixture_activities
#include "simkl_cw.c"
#undef atividades
#undef main

static char *juntar(const char *const *respostas, int n) {
  SmkTexto tipos[3] = {{0}};
  char *r = NULL;
  int ok = 1;
  for (int i = 0; ok && i < n; i++) ok = bibliotecaJuntarEstado(tipos, respostas[i], NULL);
  if (ok) r = bibliotecaMontar(tipos);
  for (int i = 0; i < 3; i++) free(tipos[i].p);
  return r;
}

// Watched episodes in the library in memory (each one's "watched_at").
static int episodiosVistos(void) {
  int n = 0;
  pthread_mutex_lock(&trava);
  for (const char *p = bibCorpo; p && (p = strstr(p, "\"watched_at\"")); p++) n++;
  pthread_mutex_unlock(&trava);
  return n;
}

int main(void) {
  char *r;
  { const char *p[] = {
      "{\"shows\":[{\"a\":1},{\"a\":2}],\"anime\":[],\"movies\":[{\"m\":1}]}",
      " {\"shows\":[ {\"a\":3} ],\"movies\":[]}\n",
      "{\"anime\":[{\"x\":\"[,]\"}],\"shows\":[]}" };
    r = juntar(p, 3);
    assert(r && !strcmp(r, "{\"shows\":[{\"a\":1},{\"a\":2},{\"a\":3}],"
                           "\"anime\":[{\"x\":\"[,]\"}],\"movies\":[{\"m\":1}]}"));
    assert(perfilHistoricoValido(r));
    free(r); }
  printf("ok  answers join list by list, commas only between items, any key order\n");

  { const char *p[] = { "{}", "{\"movies\":[]}" };
    r = juntar(p, 2);
    assert(r && !strcmp(r, "{\"shows\":[],\"anime\":[],\"movies\":[]}"));
    assert(perfilHistoricoValido(r));
    free(r); }
  printf("ok  an empty library is three empty lists\n");

  { const char *p[] = { "{\"shows\":[{\"a\":1}]}", "<html>busy</html>" };
    assert(!juntar(p, 2)); }
  { const char *p[] = { "{\"error\":\"x\",\"shows\":[]}" };
    assert(!juntar(p, 1)); }
  printf("ok  an answer that is not all-items stops the join\n");

  // Status lists: the items with that status, without their episode history,
  // in the shape of /sync/all-items/<type>/<status>.
  { const char *lib =
      "{\"shows\":["
      "{\"seasons\":[{\"number\":1}],\"status\":\"watching\",\"show\":{\"title\":\"A\"}},"
      "{\"status\":\"completed\",\"show\":{\"title\":\"B\"},\"seasons\":[]},"
      "{\"status\":\"watching\",\"seasons\" : [{\"title\":\"ep\"}] ,\"show\":{\"title\":\"C\"}},"
      "{\"show\":{\"title\":\"no status\"}}],"
      "\"anime\":[],\"movies\":[{\"status\":\"watching\",\"movie\":{}}]}";
    r = bibLista(lib, "shows", "watching");
    assert(r && !strcmp(r, "{\"shows\":[{\"status\":\"watching\",\"show\":{\"title\":\"A\"}},"
                           "{\"status\":\"watching\" ,\"show\":{\"title\":\"C\"}}]}"));
    assert(perfilHistoricoValido(r));
    free(r);
    r = bibLista(lib, "shows", "completed");
    assert(r && !strcmp(r, "{\"shows\":[{\"status\":\"completed\",\"show\":{\"title\":\"B\"}}]}"));
    free(r);
    r = bibLista(lib, "anime", "watching");
    assert(r && !strcmp(r, "{\"anime\":[]}"));
    free(r);
    r = bibLista(lib, "shows", "hold");
    assert(r && !strcmp(r, "{\"shows\":[]}"));
    free(r);
    assert(!bibLista(NULL, "shows", "watching")); }
  printf("ok  status lists: by status, without the episode history, any key order\n");

  // The merge, by Simkl ID across the three lists.
  { const char *lib =
      "{\"shows\":[{\"status\":\"watching\",\"show\":{\"title\":\"X\",\"ids\":{\"simkl\":1}}},"
      "{\"status\":\"plantowatch\",\"show\":{\"title\":\"Y\",\"ids\":{\"simkl\":2}}}],"
      "\"anime\":[],\"movies\":[{\"status\":\"completed\",\"movie\":{\"ids\":{\"simkl\":3}}}]}";
    SmkIds atuais = {0};
    int trocados, removidos, ambiguo;
    // X turns out to be anime: it leaves shows, lands in anime once.
    r = bibliotecaFundir(lib, "{\"anime\":[{\"status\":\"watching\",\"show\":{\"title\":\"X2\","
                              "\"ids\":{\"simkl\":1}}}]}", NULL, &trocados, &removidos, &ambiguo);
    assert(r && trocados == 1 && removidos == 0);
    assert(!strstr(r, "\"X\"") && strstr(r, "\"anime\":[{\"status\":\"watching\",\"show\":{\"title\":\"X2\""));
    assert(strstr(r, "\"Y\"") && strstr(r, "\"simkl\":3") && perfilHistoricoValido(r));
    free(r);
    // An empty delta changes nothing.
    r = bibliotecaFundir(lib, "{\"shows\":[],\"anime\":[],\"movies\":[]}", NULL, &trocados, &removidos, &ambiguo);
    assert(r && trocados == 0 && removidos == 0 && strstr(r, "\"X\"") && strstr(r, "\"Y\""));
    free(r);
    // The IDs-only list drops what is gone (the movie, 3).
    assert(idsLer(&atuais, "{\"shows\":[{\"show\":{\"ids\":{\"simkl\":2}}},{\"ids\":{\"simkl\":1}}]}"));
    assert(atuais.n == 2 && idsTem(&atuais, 1) && !idsTem(&atuais, 3));
    r = bibliotecaFundir(lib, "{}", &atuais, &trocados, &removidos, &ambiguo);
    assert(r && removidos == 1 && !strstr(r, "\"simkl\":3") && strstr(r, "\"X\""));
    free(r);
    free(atuais.v);
    // Empty answers.
    assert(corpoVazio("null") && corpoVazio(" [ ] ") && corpoVazio("{}") && corpoVazio("\n{ }\n"));
    assert(!corpoVazio("{\"shows\":[]}") && !corpoVazio("nullx") && !corpoVazio(""));
  }
  printf("ok  merge: by Simkl ID across lists; empty delta; IDs-only list drops; empty answers\n");

  // Items without a Simkl ID: matched by their other IDs.
  { const char *lib =
      "{\"shows\":[{\"status\":\"watching\",\"show\":{\"title\":\"B\",\"ids\":{\"simkl\":2,\"imdb\":\"tt2\",\"tmdb\":\"7\"}}},"
      "{\"status\":\"watching\",\"show\":{\"title\":\"NoId\",\"ids\":{\"imdb\":\"tt9\"}}}],"
      "\"anime\":[{\"status\":\"watching\",\"show\":{\"title\":\"K1\",\"ids\":{\"simkl\":10,\"imdb\":\"tt8\",\"mal\":\"100\"}}},"
      "{\"status\":\"watching\",\"show\":{\"title\":\"K2\",\"ids\":{\"simkl\":11,\"imdb\":\"tt8\",\"mal\":\"200\"}}}],"
      "\"movies\":[]}";
    SmkIds atuais = {0};
    int trocados, removidos, ambiguo;
    // One match by IMDb; TMDB as a number equals TMDB as text.
    r = bibliotecaFundir(lib, "{\"shows\":[{\"status\":\"completed\",\"show\":{\"title\":\"B2\","
                              "\"ids\":{\"imdb\":\"tt2\",\"tmdb\":7}}}]}", NULL, &trocados, &removidos, &ambiguo);
    assert(r && trocados == 1 && !ambiguo && strstr(r, "\"B2\"") && !strstr(r, "\"B\"") && perfilHistoricoValido(r));
    free(r);
    // An ID that differs means another title: added, B stays.
    r = bibliotecaFundir(lib, "{\"shows\":[{\"show\":{\"title\":\"B3\",\"ids\":{\"imdb\":\"tt2\",\"tmdb\":\"8\"}}}]}",
                         NULL, &trocados, &removidos, &ambiguo);
    assert(r && trocados == 0 && strstr(r, "\"B3\"") && strstr(r, "\"B\""));
    free(r);
    // Kaiju: IMDb + MAL picks season 2 alone; IMDb alone fits both.
    r = bibliotecaFundir(lib, "{\"anime\":[{\"show\":{\"title\":\"K2b\",\"ids\":{\"imdb\":\"tt8\",\"mal\":\"200\"}}}]}",
                         NULL, &trocados, &removidos, &ambiguo);
    assert(r && trocados == 1 && strstr(r, "\"K1\"") && strstr(r, "\"K2b\"") && !strstr(r, "\"K2\""));
    free(r);
    r = bibliotecaFundir(lib, "{\"anime\":[{\"show\":{\"title\":\"K?\",\"ids\":{\"imdb\":\"tt8\"}}}]}",
                         NULL, &trocados, &removidos, &ambiguo);
    assert(!r && ambiguo);
    // A movie never matches a show: TMDB numbers them apart.
    r = bibliotecaFundir(lib, "{\"movies\":[{\"movie\":{\"title\":\"M\",\"ids\":{\"tmdb\":\"7\"}}}]}",
                         NULL, &trocados, &removidos, &ambiguo);
    assert(r && trocados == 0 && strstr(r, "\"B\"") && strstr(r, "\"M\""));
    free(r);
    // No IDs at all: added.
    r = bibliotecaFundir(lib, "{\"shows\":[{\"show\":{\"title\":\"Bare\"}}]}", NULL, &trocados, &removidos, &ambiguo);
    assert(r && trocados == 0 && strstr(r, "\"Bare\"") && strstr(r, "\"NoId\""));
    free(r);
    // The library's item without an ID is replaced once Simkl sends it with one.
    r = bibliotecaFundir(lib, "{\"shows\":[{\"show\":{\"title\":\"WithId\",\"ids\":{\"simkl\":9,\"imdb\":\"tt9\"}}}]}",
                         NULL, &trocados, &removidos, &ambiguo);
    assert(r && trocados == 1 && strstr(r, "\"WithId\"") && !strstr(r, "\"NoId\""));
    free(r);
    // The IDs-only list cannot vouch for it: it stays; K2 (11) is gone.
    assert(idsLer(&atuais, "{\"shows\":[{\"ids\":{\"simkl\":2}}],\"anime\":[{\"ids\":{\"simkl\":10}}]}"));
    r = bibliotecaFundir(lib, "{}", &atuais, &trocados, &removidos, &ambiguo);
    assert(r && removidos == 1 && strstr(r, "\"NoId\"") && !strstr(r, "\"K2\""));
    free(r);
    free(atuais.v);
  }
  printf("ok  merge without a Simkl ID: other IDs, one match only, movies apart, kept on removals\n");

  // Continue watching's playback copy: keyed on the three playback
  // timestamps; without any, on the whole activities as before.
  { const char *a = "{\"all\":\"1\",\"tv_shows\":{\"playback\":\"p\",\"watching\":\"w\"},"
                    "\"anime\":{\"playback\":null},\"movies\":{\"playback\":\"m\"}}";
    const char *b = "{\"all\":\"2\",\"tv_shows\":{\"playback\":\"p\",\"watching\":\"w2\"},"
                    "\"anime\":{\"playback\":null},\"movies\":{\"playback\":\"m\"}}";
    const char *c = "{\"all\":\"2\",\"tv_shows\":{\"playback\":\"p\",\"watching\":\"w2\"},"
                    "\"anime\":{\"playback\":\"x\"},\"movies\":{\"playback\":\"m\"}}";
    assert(assinarPlayback(a) && assinarPlayback(a) == assinarPlayback(b));
    assert(assinarPlayback(b) != assinarPlayback(c));
    assert(assinarPlayback("{\"all\":\"1\"}") != assinarPlayback("{\"all\":\"2\"}"));
    assert(!assinarPlayback(NULL)); }
  printf("ok  playback signature: its timestamps only; whole activities without them\n");

  // Which activities move the library.
  { int m, rm;
    const char *a = "{\"all\":\"1\",\"tv_shows\":{\"playback\":\"p\",\"watching\":\"w\",\"removed_from_list\":null},"
                    "\"anime\":{\"watching\":null},\"movies\":{\"plantowatch\":\"x\"}}";
    ativComparar(a, "{\"all\":\"2\",\"tv_shows\":{\"playback\":\"p2\",\"rated_at\":\"r\",\"watching\":\"w\","
                    "\"removed_from_list\":null},\"anime\":{\"watching\":null},\"movies\":{\"plantowatch\":\"x\"}}", &m, &rm);
    assert(!m && !rm);
    ativComparar(a, "{\"all\":\"2\",\"tv_shows\":{\"playback\":\"p\",\"watching\":\"w2\",\"removed_from_list\":null},"
                    "\"anime\":{\"watching\":null},\"movies\":{\"plantowatch\":\"x\"}}", &m, &rm);
    assert(m && !rm);
    ativComparar(a, "{\"all\":\"2\",\"tv_shows\":{\"playback\":\"p\",\"watching\":\"w\",\"removed_from_list\":\"z\"},"
                    "\"anime\":{\"watching\":null},\"movies\":{\"plantowatch\":\"x\"}}", &m, &rm);
    assert(!m && rm);
    ativComparar(a, "{\"all\":\"2\"}", &m, &rm);
    assert(m);
  }
  printf("ok  activities: playback/ratings move nothing; a bucket asks a delta; removals the IDs\n");

  // How often list readers ask Simkl: once per SMK_BIB_FRESCA, and again
  // right after a write from this app.
  { CatItem v[12];
    int ativ;
    tokenAtual = TOKEN;
    simkl_esquecer();
    memset(batidas, 0, sizeof batidas);
    assert(simkl_continuar(v, 12) > 0);
    assert(batidas[0] == 1 && batidas[9] == 1);
    assert(simkl_plantowatch(v, 12) > 0);
    r = simkl_lista_corpo("shows", "watching");
    assert(r && strstr(r, "Severance") && !strstr(r, "\"seasons\""));
    free(r);
    assert(batidas[0] == 1 && batidas[9] == 1 && batidas[2] == 0 && batidas[3] == 0);
    printf("ok  list readers within %d s share one check: no request\n", SMK_BIB_FRESCA);

    ativ = batidas[0];
    assert(simkl_lista_tipo("tt1375666", "movie", 1) == 1);
    assert(esperarEscrita() == SMK_OP_CONFIRMADA);
    assert(simkl_plantowatch(v, 12) > 0);
    assert(batidas[0] == ativ + 1);
    assert(simkl_plantowatch(v, 12) > 0);
    assert(batidas[0] == ativ + 1);
    printf("ok  a write from this app makes the next reader check again, once\n");
    bibEsperar(); }

  // A status list joined: an item without its own status gets the list's,
  // one with it keeps it.
  { SmkTexto tipos[3] = {{0}};
    assert(bibliotecaJuntarEstado(tipos,
             "{\"shows\":[{\"show\":{\"title\":\"A\"}}, {}, {\"status\":\"hold\",\"x\":1}]}",
             "completed"));
    r = bibliotecaMontar(tipos);
    assert(r && !strcmp(r, "{\"shows\":[{\"status\":\"completed\",\"show\":{\"title\":\"A\"}},"
                           "{\"status\":\"completed\"},{\"status\":\"hold\",\"x\":1}],"
                           "\"anime\":[],\"movies\":[]}"));
    free(r);
    for (int i = 0; i < 3; i++) free(tipos[i].p); }
  printf("ok  a status list's items carry its status\n");

  // Over Simkl's max_items: shows asked status by status, and its completed
  // list (over the episode cap) with only the recorded episodes. The library
  // holds the same lists and the same watched episodes as a pull in one
  // piece, and is saved. Every item has a status, as on Simkl (the default
  // fake library's watched history has none, so a status list leaves it out).
  { static const char *const estados[] = { "watching", "plantowatch", "hold", "completed",
                                           "dropped" };
    static const char *const BIB_DIVIDIR =
      "{\"shows\":["
      "{\"status\":\"watching\",\"show\":{\"title\":\"Watching\",\"ids\":{\"simkl\":901,"
        "\"imdb\":\"tt0000901\"}},\"seasons\":[{\"number\":1,\"episodes\":["
        "{\"number\":1,\"watched_at\":\"2026-10-01T12:00:00Z\"},"
        "{\"number\":2,\"watched_at\":\"2026-10-02T12:00:00Z\"}]}]},"
      "{\"status\":\"completed\",\"show\":{\"title\":\"Done\",\"ids\":{\"simkl\":902,"
        "\"imdb\":\"tt0000902\"}},\"seasons\":[{\"number\":1,\"episodes\":["
        "{\"number\":1,\"watched_at\":\"2026-09-01T12:00:00Z\"}]}]},"
      "{\"status\":\"plantowatch\",\"show\":{\"title\":\"Later\",\"ids\":{\"simkl\":903,"
        "\"imdb\":\"tt0000903\"}}}],"
      "\"anime\":[],\"movies\":[]}";
    char *inteira[5], *dividida;
    int vistosInteira, vistosDividida;
    tokenAtual = TOKEN;
    historicoCorpo = BIB_DIVIDIR;
    bibEsperar();
    simkl_esquecer();
    free(bibTexto); bibTexto = NULL;
    for (int i = 0; i < 5; i++) assert((inteira[i] = simkl_lista_corpo("shows", estados[i])));
    vistosInteira = episodiosVistos();
    bibEsperar();
    simkl_esquecer();
    free(bibTexto); bibTexto = NULL;
    grandeTipo = "shows"; grandeEstado = "completed";
    pedidosEstado = pedidosOriginal = 0;
    for (int i = 0; i < 5; i++) {
      assert((dividida = simkl_lista_corpo("shows", estados[i])));
      assert(!strcmp(dividida, inteira[i]));
      free(dividida);
      free(inteira[i]);
    }
    vistosDividida = episodiosVistos();
    assert(vistosInteira == 3 && vistosDividida == vistosInteira);
    assert(pedidosEstado == 6 && pedidosOriginal == 1 && bibTexto);
    grandeTipo = grandeEstado = NULL;
    historicoCorpo = NULL;
    bibEsperar(); }
  printf("ok  max_items: a type status by status, a status with its recorded episodes\n");

  // A status still over the cap with original: no library, nothing saved,
  // and no request after it (the other statuses, the other types).
  { bibEsperar();
    simkl_esquecer();
    free(bibTexto); bibTexto = NULL;
    grandeTipo = "shows"; grandeEstado = "plantowatch"; grandeSempre = 1;
    pedidosEstado = pedidosOriginal = 0;
    assert(!simkl_lista_corpo("shows", "watching"));
    assert(pedidosEstado == 3 && pedidosOriginal == 1 && !bibTexto);
    grandeTipo = grandeEstado = NULL; grandeSempre = 0;
    bibEsperar();
    simkl_esquecer(); }
  printf("ok  max_items even with original: no library, nothing saved\n");

  printf("simkl_biblioteca: tudo ok\n");
  return 0;
}
