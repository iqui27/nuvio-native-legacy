// Upstream Matis94 2.0.8.5 (6d52159df, 70d8d67ab): local Continue Watching is
// visible while the remote tracker request is still blocked; release
// filenames and the "Episódio N" placeholder count as missing episode names;
// season tabs include season 0. Offline fixture over tests/cwordem_desc.c;
// trakt_continuar blocks on cwprov_wait() so the intermediate row is checked.
//
//   bash tests/cwprov.sh
#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#define NV_TEST_STUB_PERFISESSAO_H
static void cwprov_wait(void);
#define CWPROV_WAIT
#define main cwordem_regression_main
#include "cwordem_desc.c"
#undef main

static pthread_mutex_t sessionLock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t session;
uint64_t perfis_sessao_geracao(void) { return __atomic_load_n(&session, __ATOMIC_ACQUIRE); }
int perfis_sessao_atual(uint64_t s) { return s == perfis_sessao_geracao(); }
int perfis_sessao_travar(uint64_t s) {
  pthread_mutex_lock(&sessionLock);
  if (perfis_sessao_atual(s)) return 1;
  pthread_mutex_unlock(&sessionLock); return 0;
}
void perfis_sessao_destravar(void) { pthread_mutex_unlock(&sessionLock); }
static pthread_mutex_t remoteLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t remoteCond = PTHREAD_COND_INITIALIZER;
static int remoteEntered, remoteRelease;
static void cwprov_wait(void) {
  pthread_mutex_lock(&remoteLock);
  remoteEntered = 1; pthread_cond_broadcast(&remoteCond);
  while (!remoteRelease) pthread_cond_wait(&remoteCond, &remoteLock);
  pthread_mutex_unlock(&remoteLock);
}
static pthread_t startBlocked(void) {
  pthread_t thread;
  remoteEntered = remoteRelease = 0;
  uint64_t *arg = malloc(sizeof *arg); assert(arg); *arg = session;
  assert(!pthread_create(&thread, NULL, fioContinuar, arg));
  pthread_mutex_lock(&remoteLock);
  struct timespec deadline; clock_gettime(CLOCK_REALTIME, &deadline); deadline.tv_sec += 10;
  while (!remoteEntered) assert(!pthread_cond_timedwait(&remoteCond, &remoteLock, &deadline));
  pthread_mutex_unlock(&remoteLock);
  return thread;
}
static void finishBlocked(pthread_t thread) {
  pthread_mutex_lock(&remoteLock);
  remoteRelease = 1; pthread_cond_broadcast(&remoteCond);
  pthread_mutex_unlock(&remoteLock);
  assert(!pthread_join(thread, NULL));
}

static long long relogioProv(void) { return 2000000; }

// --- helpers ---------------------------------------------------------------
static void gravarLocal(const char *imdb, int t, int e, int feito, int total) {
  assert(prog_gravar_local(imdb, t, e, feito, total, -1));
}
static void rodarFio(uint64_t sessao) {
  uint64_t *arg = malloc(sizeof *arg);
  assert(arg); *arg = sessao;
  fioContinuar(arg);
}
static int fileiraCw(CatItem *out, int max) {
  return cat_copiar_fileira("continue_watching", out, max, NULL);
}

int main(void) {
  prog_definir_relogio(relogioProv);
  prog_esquecer_tudo();
  agoraMs = (long long)time(NULL) * 1000LL;
  cat_definir(NULL, 0);
  CatItem lote[CONT_MAX];

  // 1. No snapshot: the local row is visible while the remote call blocks.
  gravarLocal("ttProv", 1, 2, 120, 300);
  fonteTeste = AJ_CWF_AMBAS;
  traktAtivoTeste = 1; simklAtivoTeste = 0; traktFalhouTeste = 1;
  pthread_t thread = startBlocked();
  int n = fileiraCw(lote, CONT_MAX);
  assert(n == 1 && !strcmp(lote[0].imdb, "ttProv:1:2"));
  assert(lote[0].progresso == 40);
  finishBlocked(thread);
  puts("ok  blocked remote: actual local row visible without snapshot");

  // 2. Stale snapshot on screen: local progress leads, unrelated cached entry
  //    stays (with its metadata) until the remote result reconciles.
  prog_esquecer_tudo(); gravarLocal("ttNovo", 2, 5, 60, 300);
  { CatItem velho;
    memset(&velho, 0, sizeof velho);
    snprintf(velho.imdb, sizeof velho.imdb, "ttVelho:1:1");
    snprintf(velho.tipo, sizeof velho.tipo, "series");
    snprintf(velho.titulo, sizeof velho.titulo, "Velho");
    velho.progresso = 10;
    cat_trocar_continuar(&velho, 1);
  }
  thread = startBlocked();
  n = fileiraCw(lote, CONT_MAX);
  assert(n == 2 && !strcmp(lote[0].imdb, "ttNovo:2:5"));
  assert(lote[0].progresso == 20 && !strcmp(lote[1].imdb, "ttVelho:1:1"));
  assert(!strcmp(lote[1].titulo, "Velho"));
  finishBlocked(thread);
  puts("ok  blocked remote: local progress visible beside stale snapshot");

  // A profile switch invalidates the in-flight result after provisional publication.
  thread = startBlocked();
  pthread_mutex_lock(&sessionLock);
  __atomic_store_n(&session, 1, __ATOMIC_RELEASE);
  cat_trocar_continuar(NULL, 0);
  pthread_mutex_unlock(&sessionLock);
  finishBlocked(thread);
  assert(fileiraCw(lote, CONT_MAX) == 0);
  __atomic_store_n(&session, 0, __ATOMIC_RELEASE);
  puts("ok  profile switched during blocked fetch: old row never republished");

  // 3. Account-only source (Trakt active): no local row, before or after.
  prog_esquecer_tudo(); gravarLocal("ttSolo", 1, 1, 30, 300);
  fonteTeste = AJ_CWF_TRAKT; traktAtivoTeste = 1; traktFalhouTeste = 1;
  cat_trocar_continuar(NULL, 0);
  rodarFio(0);
  n = fileiraCw(lote, CONT_MAX);
  assert(n == 0);
  puts("ok  account-only source: no local row");
  fonteTeste = AJ_CWF_AMBAS; traktAtivoTeste = 0; traktFalhouTeste = 0;

  // 4. Stale profile session generation: nothing is published.
  prog_esquecer_tudo(); gravarLocal("ttOutro", 3, 1, 40, 300);
  cat_trocar_continuar(NULL, 0);
  rodarFio(1);   // only generation 0 is current
  n = fileiraCw(lote, CONT_MAX);
  assert(n == 0);
  puts("ok  stale profile session: nothing published");

  // 5. Removed local entry never comes back through the provisional row.
  prog_esquecer_tudo();
  gravarLocal("ttRem", 1, 1, 40, 300);
  prog_remover("ttRem:1:1");
  prog_marcar_removido("ttRem:1:1");
  fonteTeste = AJ_CWF_AMBAS;
  cat_trocar_continuar(NULL, 0);
  rodarFio(0);
  n = fileiraCw(lote, CONT_MAX);
  assert(n == 0);
  prog_esquecer_tudo();
  puts("ok  removed local entry stays removed");

  // 6-10 need the new APIs; -DNV_CWPROV_OLD builds only the row part.
#ifndef NV_CWPROV_OLD
  // 6. nomeEpisodioArquivo/nomeEpisodioReserva (via API publica do modulo).
  assert(desc_nome_episodio_reserva("Show.S01E02.1080p.WEB-DL.x264.mkv", 2));
  assert(desc_nome_episodio_reserva("Show_S03E07_H265_2160p", 7));
  assert(desc_nome_episodio_reserva("Episódio 4", 4));
  assert(!desc_nome_episodio_reserva("Episódio 4", 5));
  assert(!desc_nome_episodio_reserva("The One With the Pilot", 1));
  assert(!desc_nome_episodio_reserva("", 1));
  puts("ok  placeholder names: extension, release pattern, localized label");

  // 7. TMDB substitui nome de arquivo mesmo com SO_VAZIO; nome de arquivo do
  //    TMDB nao entra; generico do TMDB nao entra.
  { CatEp eps[2];
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 1; eps[0].episodio = 1;
    snprintf(eps[0].nome, sizeof eps[0].nome, "Serie.S01E01.mkv");
    eps[1].temporada = 1; eps[1].episodio = 2;
    snprintf(eps[1].nome, sizeof eps[1].nome, "Nome Real");
    const char *json = "{\"episodes\":["
      "{\"episode_number\":1,\"name\":\"Pilot\",\"vote_average\":7.5},"
      "{\"episode_number\":2,\"name\":\"Serie.S01E02.720p.mkv\",\"vote_average\":8.0}]}";
    assert(desc_tmdb_notas_temporada_ex(json, eps, 2, 1, DESC_EPT_NOME|DESC_EPT_SO_VAZIO) >= 1);
    assert(!strcmp(eps[0].nome, "Pilot"));
    assert(!strcmp(eps[1].nome, "Nome Real"));
    puts("ok  tmdb: filename replaced; filename donor ignored"); }

  // 8. desc_mesclar_episodios: doador com nome de arquivo nao sobrescreve em
  //    TEXTO; em VAZIOS, preenche so o que falta (reserva conta como vazio).
  { CatEp base[1], outro[1];
    memset(base, 0, sizeof base); memset(outro, 0, sizeof outro);
    base[0].temporada = outro[0].temporada = 1;
    base[0].episodio = outro[0].episodio = 3;
    snprintf(base[0].nome, sizeof base[0].nome, "Nome Real");
    snprintf(outro[0].nome, sizeof outro[0].nome, "X.S01E03.mkv");
    desc_mesclar_episodios(base, 1, outro, 1, DESC_MESCLA_TEXTO);
    assert(!strcmp(base[0].nome, "Nome Real"));
    snprintf(base[0].nome, sizeof base[0].nome, "X.S01E03.mkv");
    desc_mesclar_episodios(base, 1, outro, 1, DESC_MESCLA_VAZIOS);
    assert(!strcmp(base[0].nome, "X.S01E03.mkv")); // doador e arquivo: reserva so cede a nome de verdade
    snprintf(outro[0].nome, sizeof outro[0].nome, "Titulo Bom");
    desc_mesclar_episodios(base, 1, outro, 1, DESC_MESCLA_VAZIOS);
    assert(!strcmp(base[0].nome, "Titulo Bom"));
    puts("ok  merge: filename never overwrites; placeholder yields to real name"); }

  // 9. temporadasDosEps: temporada 0 (especiais) vira aba.
  { CatEp eps[5];
    memset(eps, 0, sizeof eps);
    { static const int tmps[] = { 1, 2, 0, 0, 2 };
      static const int epsn[] = { 1, 1, 1, 2, 2 };
      for (int i = 0; i < 5; i++) { eps[i].temporada = tmps[i]; eps[i].episodio = epsn[i]; } }
    TempsPub tp;
    temporadasDosEps(eps, 5, &tp);
    assert(tp.n == 3 && tp.t[0] == 1 && tp.t[1] == 2 && tp.t[2] == 0);
    puts("ok  seasons: season 0 tab after regular seasons"); }

  // 10. parsearEpisodios: nome de arquivo vira "Episódio N" (title valido
  //     primeiro), vid/season/episode/sourceURL intactos.
  { static CatEp eps[4];
    const char *json = "{\"videos\":["
      "{\"id\":\"v1\",\"season\":1,\"episode\":1,\"name\":\"S.S01E01.1080p.mkv\",\"title\":\"Pilot Real\"},"
      "{\"id\":\"v2\",\"season\":1,\"episode\":2,\"name\":\"S.S01E02.720p.mkv\"},"
      "{\"id\":\"v3\",\"season\":0,\"episode\":1,\"name\":\"Special\"},"
      "{\"id\":\"v4\",\"season\":1,\"episode\":3,\"name\":\"Real Name\"}]}";
    int n = parsearEpisodios(json, eps, 4);
    assert(n == 4);
    // A lista sai ordenada por (temporada, episodio): o especial (T0) primeiro.
    assert(!strcmp(eps[0].nome, "Special") && eps[0].temporada == 0);
    assert(!strcmp(eps[1].nome, "Pilot Real") && !strcmp(eps[1].vid, "v1"));
    assert(!strcmp(eps[2].nome, "Episódio 2") && eps[2].temporada == 1 && eps[2].episodio == 2
           && !strcmp(eps[2].vid, "v2"));
    assert(!strcmp(eps[3].nome, "Real Name") && !strcmp(eps[3].vid, "v4"));
    puts("ok  parse: filename -> title or Episódio N, ids/seasons kept"); }
#endif

  puts("cwprov: ok");
  return 0;
}
