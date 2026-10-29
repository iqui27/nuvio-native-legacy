// Exercise the real player checkpoint and disk writer; never close the player.
#define main retained_fixture_main
#include "player_retido.c"
#undef main
#include "dados.h"
#include "progresso.h"
#include "traktscrobble.h"
#include "trakt.h"
#include "simkl.h"
#include "rede.h"
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int nv_test_continuarLocal(CatItem *, int);
void nv_test_episodes(int index) { (void)index; }
int nv_test_enrich(CatItem *items, int count) { (void)items; return count; }
int nv_test_enrich_session(CatItem *items, int count, uint64_t session) {
  (void)session; return nv_test_enrich(items, count);
}
static int syncRequests, pointRequests;
void nv_test_sync(void) { syncRequests++; }
static pthread_mutex_t requestLock = PTHREAD_MUTEX_INITIALIZER;
static int requests[2];
static char remoteAction[2][16];
const char *nv_test_simkl_token(void) { return "fixture-token"; }
const char *nv_test_simkl_client(void) { return "fixture-client"; }
const char *nv_test_simkl_app(void) { return "fixture-app"; }
unsigned long long nv_test_simkl_cred(char *dst, size_t n) {
  if (dst && n) snprintf(dst, n, "%s", nv_test_simkl_token());
  return 1;
}
char *rede_postar_st(const char *url, int timeout, const char *const *headers,
                     const char *body, int *status) {
  (void)timeout; (void)headers;
  const char *action = strstr(url, "/scrobble/");
  assert(action && body && strstr(body, "\"progress\":"));
  int tracker = strstr(url, "api.simkl.com") != NULL;
  pthread_mutex_lock(&requestLock);
  snprintf(remoteAction[tracker], sizeof remoteAction[tracker], "%.*s",
           (int)strcspn(action + 10, "?"), action + 10);
  requests[tracker]++;
  pthread_mutex_unlock(&requestLock);
  *status = 200;
  return strdup("{}");
}
static int trackerRequest(int tracker, int event, const char *id, double p, double d,
                          double a, int watched) {
  pthread_mutex_lock(&requestLock);
  int before = requests[tracker];
  pthread_mutex_unlock(&requestLock);
  int action = tracker ? simkl_scrobble(event, id, p, d, a, watched)
                       : trakt_scrobble(event, id, p, d, a, watched);
  if (action != SCR_NADA) {
    int done = 0;
    for (int i = 0; i < 400 && !done; i++) {
      usleep(5000);
      pthread_mutex_lock(&requestLock);
      done = requests[tracker] > before;
      pthread_mutex_unlock(&requestLock);
    }
    assert(done);
  }
  if (event == SCR_EV_PONTO) {
    pointRequests++;
    pthread_mutex_lock(&requestLock);
    if (p / d >= .01 && p / d < .80) {
      if (strcmp(remoteAction[tracker], "start")) {
        fprintf(stderr, "%s checkpoint left remote %s while playing\n",
                tracker ? "Simkl" : "Trakt", remoteAction[tracker]);
        abort();
      }
      assert(requests[tracker] == 2);
    }
    pthread_mutex_unlock(&requestLock);
  }
  return action;
}
int nv_test_trakt(int event, const char *id, double p, double d, double a, int watched) {
  return trackerRequest(0, event, id, p, d, a, watched);
}
int nv_test_simkl(int event, const char *id, double p, double d, double a, int watched) {
  return trackerRequest(1, event, id, p, d, a, watched);
}
static int fileSynced, renamed, directorySynced;
int nv_test_fsync(int fd) {
  int rc = fsync(fd);
  if (!rc) { if (renamed) directorySynced++; else fileSynced++; }
  return rc;
}
int nv_test_rename(const char *from, const char *to) {
  int rc = rename(from, to);
  if (!rc && strstr(to, "progresso.txt")) { assert(fileSynced); renamed = 1; }
  return rc;
}
static double seconds = 100;
int nv_test_clock(clockid_t clock, struct timespec *ts) {
  (void)clock; ts->tv_sec = (time_t)seconds; ts->tv_nsec = 0; return 0;
}
int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "periodic";
  int cadence = !strcmp(mode, "cadence"), background = !strcmp(mode, "background");
  if (argc > 2) {
    assert(SDL_Init(SDL_INIT_TIMER) == 0);
    dados_iniciar(""); perfis_definir_ativo(1);
    assert(trakt_definir("fixture-token", "fixture-client"));
    CatItem c = {0};
    strcpy(c.imdb, "tt1375666"); strcpy(c.tipo, "movie"); strcpy(c.meta, "120 min");
    cat_definir(&c, 1); posicao = 0; duracao = 7200;
    player_abrir(0, "https://fixture.invalid/movie.mp4");
    player_atualizar(.01f, SDL_GetTicks());
    for (int i = 1; i <= (cadence ? 340 : 40); i++) {
      seconds++; posicao = i;
      player_atualizar(1.0f, SDL_GetTicks());
    }
    assert(syncRequests == (cadence ? 1 : 0));
    assert(pointRequests == (cadence ? 2 : 0));
    if (background) {
      player_ciclo_evento(SDL_APP_WILLENTERBACKGROUND);
      assert(syncRequests == 1 && pointRequests == 2);
    }
    assert(fileSynced && renamed && directorySynced);
    kill(getpid(), SIGKILL);
    _exit(99);
  }
  pid_t child = fork(); assert(child >= 0);
  if (!child) { execl(argv[0], argv[0], mode, "play", (char *)NULL); _exit(98); }
  int status; assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
  dados_iniciar(""); perfis_definir_ativo(1);
  CatItem cached = {0};
  strcpy(cached.imdb, "tt1375666"); strcpy(cached.tipo, "movie");
  strcpy(cached.titulo, "Offline movie");
  cat_definir(&cached, 1);
  prog_invalidar();
  ProgRegistro r;
  assert(prog_por_chave("tt1375666", &r));
  assert(r.posSeg == (cadence ? 330 : background ? 40 : 30) && r.pendente);
  assert(prog_continuar_de_perfil_min(1, 1, 90, &r));
  assert(!strcmp(r.contentId, "tt1375666") && r.posSeg >= 30);
  CatItem row[2];
  assert(nv_test_continuarLocal(row, 2) == 1);
  assert(!strcmp(row[0].imdb, "tt1375666") && row[0].progresso >= 1);
  printf("poweroff_resume: %s: SIGKILL, durable checkpoint %.0f s, Continue Watching restored\n", mode, r.posSeg);
  return 0;
}
