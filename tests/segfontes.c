// feat/mkv-chapter-segments: Simkl e Anime-Skip (segfontes.c), sem rede.
// As respostas abaixo seguem o formato que SimklIdResolver.kt e
// SkipIntroRepository.kt (Nuvio TV/PC) leem.
#include "segfontes.h"
#include "intro.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *nuvem_simkl_cliente(void) { return "cid-teste"; }
const char *nuvem_simkl_app(void) { return "nuvio"; }

static int pedidosSimkl, pedidosAs;
static char ultimoCid[64];

static int fakeGet(const char *url, char **corpo, int *status) {
  const char *r = NULL;
  *corpo = NULL;
  if (strstr(url, "api.simkl.com")) {
    pedidosSimkl++;
    assert(strstr(url, "client_id=cid-teste"));
    if (strstr(url, "/search/id?imdb=tt2560140"))
      r = "[{\"type\":\"anime\",\"title\":\"Attack on Titan\",\"ids\":{\"simkl\":38636,\"slug\":\"aot\"}}]";
    else if (strstr(url, "/anime/38636?extended=full_anime_seasons"))
      r = "{\"ids\":{\"simkl\":38636},\"mapped_tvdb_seasons\":[{\"simkl_id\":38636,\"tvdb_season\":1},"
          "{\"simkl_id\":40001,\"tvdb_season\":2}]}";
    else if (strstr(url, "/anime/38636?extended=full"))
      r = "{\"title\":\"AoT\",\"season\":1,\"ids\":{\"simkl\":38636,\"mal\":\"16498\",\"anilist\":\"16498\",\"imdb\":\"tt2560140\"}}";
    else if (strstr(url, "/anime/40001?extended=full"))
      r = "{\"title\":\"AoT 2\",\"season\":2,\"ids\":{\"simkl\":40001,\"mal\":25777,\"anilist\":\"20958\"}}";
    else if (strstr(url, "/anime/episodes/40001"))
      r = "[{\"episode\":1,\"title\":\"a\",\"tvdb\":{\"season\":2,\"episode\":1}},"
          "{\"tvdb\":{\"season\":2,\"episode\":2},\"episode\":2},"
          "{\"episode\":3,\"tvdb\":{\"season\":2,\"episode\":3}}]";
    else if (strstr(url, "/search/id?imdb=tt0903747"))
      r = "[{\"type\":\"show\",\"ids\":{\"simkl\":17465}}]";
    else if (strstr(url, "/tv/17465?extended=full"))
      r = "{\"title\":\"BB\",\"ids\":{\"simkl\":17465,\"imdb\":\"tt0903747\"}}";
  }
  if (!r) { *status = 404; return 404; }
  *corpo = strdup(r); *status = 200; return 200;
}

static int fakePost(const char *url, const char *corpo, const char *cid, char **resp, int *status) {
  const char *r = NULL;
  assert(strstr(url, "anime-skip.com/graphql"));
  pedidosAs++;
  snprintf(ultimoCid, sizeof ultimoCid, "%s", cid);
  if (strstr(corpo, "findShowsByExternalId") && strstr(corpo, "20958"))
    r = "{\"data\":{\"findShowsByExternalId\":[{\"id\":\"show-aot2\"}]}}";
  else if (strstr(corpo, "findEpisodesByShowId") && strstr(corpo, "show-aot2"))
    r = "{\"data\":{\"findEpisodesByShowId\":["
        "{\"season\":\"2\",\"number\":\"1\",\"timestamps\":[{\"at\":0,\"type\":{\"name\":\"Canon\"}}]},"
        "{\"season\":\"2\",\"number\":\"3\",\"timestamps\":["
        "{\"at\":1350.5,\"type\":{\"name\":\"Credits\"}},"
        "{\"at\":0,\"type\":{\"name\":\"Recap\"}},"
        "{\"at\":45,\"type\":{\"name\":\"Canon\"}},"
        "{\"at\":120,\"type\":{\"name\":\"Mixed Intro\"}},"
        "{\"at\":210,\"type\":{\"name\":\"Canon\"}},"
        "{\"at\":1440,\"type\":{\"name\":\"Preview\"}}]}]}}";
  else if (strstr(corpo, "findShowsByExternalId"))
    r = "{\"data\":{\"findShowsByExternalId\":[]}}";
  if (!r) { *status = 500; return 500; }
  *resp = strdup(r); *status = 200; return 200;
}

static const IntroTrecho *acha(const IntroTrecho *v, int n, int tipo) {
  for (int i = 0; i < n; i++) if (v[i].tipo == tipo) return &v[i];
  return NULL;
}

int main(void) {
  SegIds ids;
  IntroTrecho v[8];
  int n, f;
  char buf[96];
  segfontes_definir_buscador(fakeGet);
  segfontes_definir_postador(fakePost);

  // Simkl: AoT T2E3 no IMDb -> entrada da 2a temporada, MAL 25777, AniList 20958, ep 3.
  assert(segfontes_simkl("imdb", "tt2560140", 2, 3, &ids));
  assert(ids.anime && ids.simkl == 40001 && ids.mal == 25777 && ids.anilist == 20958 && ids.ep == 3);
  puts("ok  Simkl: temporada TVDB -> entrada irma (MAL/AniList da temporada) e episodio do anime");
  // "episode" do item, e nao o de dentro de "tvdb" (a ordem das chaves varia).
  assert(segfontes_simkl_ep("[{\"tvdb\":{\"season\":2,\"episode\":7},\"episode\":4}]", 2, 7) == 4);
  puts("ok  Simkl: episodio do item fora do objeto tvdb");
  { int antes = pedidosSimkl;
    assert(segfontes_simkl("imdb", "tt2560140", 2, 3, &ids) && ids.mal == 25777);
    assert(pedidosSimkl == antes); }
  puts("ok  Simkl: cache por id/temporada/episodio");
  assert(segfontes_simkl("imdb", "tt0903747", 1, 1, &ids) && !ids.anime && !ids.mal);
  puts("ok  Simkl: serie que nao e anime nao ganha MAL");

  // Anime-Skip.
  assert(segfontes_animeskip(20958, 3, "", v, 8, &f) == 0 && pedidosAs == 0);
  puts("ok  Anime-Skip: sem Client ID nao vai a rede");
  n = segfontes_animeskip(20958, 3, "meu-id", v, 8, &f);
  assert(n == 3 && !f && !strcmp(ultimoCid, "meu-id"));
  assert(acha(v, n, INTRO_RESUMO)->inicio == 0 && acha(v, n, INTRO_RESUMO)->fim == 45);
  assert(acha(v, n, INTRO_ABERTURA)->inicio == 120 && acha(v, n, INTRO_ABERTURA)->fim == 210);
  assert(acha(v, n, INTRO_CREDITOS)->inicio == 1350.5 && acha(v, n, INTRO_CREDITOS)->fim == 1440);
  puts("ok  Anime-Skip: ordena por 'at', fim = proximo timestamp, Mixed Intro = abertura");
  { int antes = pedidosAs;
    assert(segfontes_animeskip(1, 3, "meu-id", v, 8, &f) == 0 && !f);
    assert(segfontes_animeskip(1, 3, "meu-id", v, 8, &f) == 0 && pedidosAs == antes + 1); }
  puts("ok  Anime-Skip: show desconhecido guardado (sem segundo pedido)");

  // Client ID: o digitado vence o da conta; vazio volta ao da conta.
  segfontes_animeskip_id_conta("da-conta");
  assert(!strcmp(segfontes_animeskip_id(buf, sizeof buf), "da-conta"));
  segfontes_animeskip_id_local("digitado");
  assert(!strcmp(segfontes_animeskip_id(buf, sizeof buf), "digitado"));
  segfontes_animeskip_id_local("");
  assert(!strcmp(segfontes_animeskip_id(buf, sizeof buf), "da-conta"));
  puts("ok  Client ID: ajuste digitado vence; vazio usa o da conta Nuvio");

  puts("segfontes: tudo ok");
  return 0;
}
