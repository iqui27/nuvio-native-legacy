// Real Trakt queue/serializers; only HTTP transport is replaced. Hold its first
// request so outgoing stop and next start coexist before their buffers mutate.
#include "trakt.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int calls, releaseFirst;
static char bodies[8][2000], urls[8][160], auths[8][200];
static pthread_t worker;
static int workerCount;

// Keep the production thread joinable for a deterministic fixture teardown.
int fixture_detach(pthread_t thread) { worker = thread; workerCount++; return 0; }
unsigned long long cat_historico_geracao(void) { return 1; }
void rede_avisar_401(void (*cb)(const char *)) { (void)cb; }

char *rede_postar_st(const char *url, int timeout, const char *const *cab,
                      const char *body, int *status) {
  (void)timeout;
  pthread_mutex_lock(&lock);
  int i = calls++;
  assert(i < 8);
  snprintf(urls[i], sizeof urls[i], "%s", url);
  snprintf(bodies[i], sizeof bodies[i], "%s", body);
  snprintf(auths[i], sizeof auths[i], "%s", cab[0]);
  pthread_cond_broadcast(&changed);
  while (!i && !releaseFirst) pthread_cond_wait(&changed, &lock);
  pthread_mutex_unlock(&lock);
  *status = 201;
  return strdup("{}");
}

static void awaitFirst(void) {
  struct timespec until;
  assert(!clock_gettime(CLOCK_REALTIME, &until)); until.tv_sec += 10;
  pthread_mutex_lock(&lock);
  while (!calls) assert(pthread_cond_timedwait(&changed, &lock, &until) != ETIMEDOUT);
  pthread_mutex_unlock(&lock);
}

static void queue(void) {
  TraktScrobbleAlvo a = {"tt21975436:2:1", 10567809};
  TraktScrobbleAlvo b = {"tt21975436:2:2", 11209906};
  assert(trakt_definir("fixture-A", "fixture-client"));
  assert(trakt_scrobble_alvo(SCR_EV_TOCANDO, &a, 20, 1000, 10, 0) == SCR_START);
  awaitFirst();
  assert(trakt_scrobble_alvo(SCR_EV_SAIU, &a, 900, 1000, 900, 1) == SCR_STOP);
  assert(trakt_scrobble_alvo(SCR_EV_TOCANDO, &b, 20, 1000, 10, 0) == SCR_START);
  memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
  assert(trakt_definir("fixture-B", "fixture-client"));
  pthread_mutex_lock(&lock); releaseFirst = 1;
  pthread_cond_broadcast(&changed); pthread_mutex_unlock(&lock);
  assert(workerCount == 1 && !pthread_join(worker, NULL));
  assert(calls == 3);
  assert(strstr(urls[0], "/scrobble/start") && strstr(urls[1], "/scrobble/stop") &&
         strstr(urls[2], "/scrobble/start"));
  assert(!strcmp(bodies[0], "{\"episode\":{\"ids\":{\"tvdb\":10567809}},\"progress\":2.00}"));
  assert(!strcmp(bodies[1], "{\"episode\":{\"ids\":{\"tvdb\":10567809}},\"progress\":90.00}"));
  assert(!strcmp(bodies[2], "{\"episode\":{\"ids\":{\"tvdb\":11209906}},\"progress\":2.00}"));
  for (int i = 0; i < 3; i++) assert(!strcmp(auths[i], "Authorization: Bearer fixture-A"));
}

static void manual(void) {
  TraktDestino d;
  TraktEpisodioAlvo alvos[] = {{{2, 1}, 10567809}, {{2, 1}, 10567809}, {{2, 2}, 0}};
  VistoPar legado = {2, 2};
  assert(trakt_destino_capturar(&d)); // B remains the owner after C is connected.
  assert(trakt_definir("fixture-C", "fixture-client"));
  assert(trakt_episodios_marcar_alvos_destino(&d, "tt21975436", alvos, 3, 1));
  assert(calls == 4 && !strcmp(auths[3], "Authorization: Bearer fixture-B"));
  assert(!strcmp(bodies[3], "{\"episodes\":[{\"ids\":{\"tvdb\":10567809}}],"
    "\"shows\":[{\"ids\":{\"imdb\":\"tt21975436\"},"
    "\"seasons\":[{\"number\":2,\"episodes\":[{\"number\":2}]}]}]}"));
  assert(trakt_episodios_marcar_alvos_destino(&d, "tt21975436", alvos, 1, 0));
  assert(calls == 5 && strstr(urls[4], "/sync/history/remove"));
  assert(!strcmp(bodies[4], "{\"episodes\":[{\"ids\":{\"tvdb\":10567809}}]}"));
  assert(trakt_episodios_marcar_destino(&d, "tt21975436", &legado, 1, 1));
  assert(calls == 6 && !strstr(bodies[5], "\"tvdb\""));
  assert(!strcmp(bodies[5], "{\"shows\":[{\"ids\":{\"imdb\":\"tt21975436\"},"
    "\"seasons\":[{\"number\":2,\"episodes\":[{\"number\":2}]}]}]}"));
}

int main(void) {
  queue(); manual();
  puts("trakt_alvos512: PASS");
  return 0;
}
