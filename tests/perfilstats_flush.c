// A blocked cache write must not block the UI, even on the next flush.
#define NV_ANDROID 1
#include "../src/corviva.c"
#include <assert.h>
#include <time.h>
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int started, releaseWrite, writes;
char *dados_ler(const char *name) { (void)name; return NULL; }
int dados_gravar_leve(const char *name, const char *value) {
  assert(!strcmp(name,"corviva.txt") && strstr(value,"cena "));
  pthread_mutex_lock(&gate); started=1; writes++; pthread_cond_signal(&changed);
  while(!releaseWrite) pthread_cond_wait(&changed,&gate);
  pthread_mutex_unlock(&gate); return 1;
}
int main(void) {
  sujo=1;corviva_gravar_se_preciso(0);
  pthread_mutex_lock(&gate);
  while(!started) pthread_cond_wait(&changed,&gate);
  pthread_mutex_unlock(&gate);
  sujo=1;relogio+=21000;
  corviva_gravar_se_preciso(0); // Must return while disk worker is blocked.
  assert(sujo && cacheJoinable);
  pthread_mutex_lock(&gate);releaseWrite=1;pthread_cond_signal(&changed);pthread_mutex_unlock(&gate);
  corviva_gravar_se_preciso(1); // Shutdown joins, then writes the newer snapshot.
  assert(writes==2 && !cacheJoinable && !sujo);
  puts("PASS: blocked cache I/O stays off UI; shutdown flush preserves latest snapshot");
}
