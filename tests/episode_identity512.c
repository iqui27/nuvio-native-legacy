#include "metaep.h"
#include "traktscrobble.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  MetaEpInfo ep;
  const char *split = "{\"videos\":["
    "{\"season\":1,\"episode\":13,\"tvdb_id\":1013},"
    "{\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":2001,\"trakt\":3001}}]}";
  assert(metaep_buscar_tvdb(split, 2001, &ep));
  assert(ep.temporada == 2 && ep.episodio == 1);
  assert(metaep_buscar(split, 2, 1, &ep));
  assert(!strcmp(ep.tvdb, "2001") && !strcmp(ep.trakt, "3001"));

  // Three duplicate rows for the same pair disagree, agree with the first
  // again. Once ambiguity is found, the third row must not revive the ID.
  const char *triplo = "{\"videos\":["
    "{\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":10,\"trakt\":20}},"
    "{\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":11,\"trakt\":21}},"
    "{\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":10,\"trakt\":20}}]}";
  assert(metaep_buscar(triplo, 2, 1, &ep));
  assert(!ep.tvdb[0] && !ep.trakt[0]);
  assert(!metaep_buscar_tvdb(triplo, 10, &ep));

  // One exact ID on different source pairs is not a usable translation.
  const char *amb = "{\"videos\":["
    "{\"season\":1,\"episode\":13,\"ids\":{\"tvdb\":99}},"
    "{\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":99}}]}";
  assert(!metaep_buscar_tvdb(amb, 99, &ep));

  // Current canonical episode handling includes specials (season zero).
  const char *special = "{\"videos\":["
    "{\"season\":0,\"episode\":1,\"ids\":{\"tvdb\":7001}}]}";
  assert(metaep_buscar(special, 0, 1, &ep) && !strcmp(ep.tvdb, "7001"));
  assert(metaep_buscar_tvdb(special, 7001, &ep));
  assert(ep.temporada == 0 && ep.episodio == 1);

  TraktScrobbleAlvo alvo = { "tt2861424:2:1", 2001 };
  char corpo[256];
  scrobble_corpo_alvo(corpo, sizeof corpo, &alvo, 42.5);
  assert(strstr(corpo, "\"episode\":{\"ids\":{\"tvdb\":2001}}"));
  assert(!strstr(corpo, "\"show\""));
  alvo.tvdbEp = 0;
  scrobble_corpo_alvo(corpo, sizeof corpo, &alvo, 42.5);
  assert(strstr(corpo, "\"show\":{\"ids\":{\"imdb\":\"tt2861424\"}}"));
  assert(strstr(corpo, "\"season\":2,\"number\":1"));

  // Player opened while only the provisional list exists: no numeric start is
  // emitted. The final list freezes TVDB once, and a later publication cannot
  // change pause/stop to another identity during the same playback.
  {
    TraktScrobbleAlvo frozen = {{0}, 0}; int ready = 0;
    ScrobbleEstado state = {{0}, 0};
    assert(!scrobble_alvo_congelar(&frozen, &ready, "tt21975436:1:14", 0, 0, 0));
    assert(!ready && !frozen.id[0]);
    // The playing event cannot queue a numeric start from provisional data.
    assert(!ready && scrobble_decidir(&state, SCR_EV_TOCANDO, NULL, 2.0) == SCR_NADA);
    assert(scrobble_alvo_congelar(&frozen, &ready, "tt21975436:2:2", 1, 1, 11209906));
    assert(ready && frozen.tvdbEp == 11209906 && !strcmp(frozen.id, "tt21975436:2:2"));
    // If final publication and pause cross, there was no numeric session to
    // pause. The next playing tick starts the exact session and its later
    // pause keeps that same frozen target.
    assert(scrobble_decidir(&state, SCR_EV_PAUSOU, frozen.id, 42.0) == SCR_NADA);
    assert(scrobble_decidir(&state, SCR_EV_TOCANDO, frozen.id, 2.0) == SCR_START);
    assert(scrobble_decidir(&state, SCR_EV_PAUSOU, frozen.id, 42.0) == SCR_PAUSE);
    assert(scrobble_alvo_congelar(&frozen, &ready, "tt21975436:9:9", 1, 1, 999));
    assert(frozen.tvdbEp == 11209906 && !strcmp(frozen.id, "tt21975436:2:2"));
    memset(&frozen, 0, sizeof frozen); ready = 0;
    assert(scrobble_alvo_congelar(&frozen, &ready, "tt21975436:2:3", 1, 1, 0));
    assert(scrobble_alvo_congelar(&frozen, &ready, "tt21975436:2:3", 1, 1, 123456));
    assert(frozen.tvdbEp == 0); // no worker-time metadata upgrade

    // A final publication removed the provisional S1E13 pair. Numeric
    // scrobbling must stay disabled, while a TVDB id captured before the
    // replacement remains a safe target even though that pair disappeared.
    memset(&frozen, 0, sizeof frozen); ready = 0;
    assert(!scrobble_alvo_congelar(&frozen, &ready, "tt21975436:1:13", 1, 0, 0));
    assert(!ready);
    assert(scrobble_alvo_congelar(&frozen, &ready, "tt21975436:1:13",
                                  1, 0, 10567809));
    assert(ready && frozen.tvdbEp == 10567809);
  }

  puts("episode_identity512: PASS");
  return 0;
}
