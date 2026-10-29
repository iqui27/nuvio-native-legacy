// Real warm-focus controller, fake network job: no sockets or decoder.
#include "retomamorna.h"
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
struct RedeGrupo { int unused; };
struct RedeJob { int refs, canceled; };
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int started, canceled, finished;
RedeGrupo *rede_grupo_criar(void) { return calloc(1, sizeof(RedeGrupo)); }
void rede_grupo_soltar(RedeGrupo *g) { free(g); }
RedeJob *rede_job_criar(RedeGrupo *g) { (void)g; RedeJob *j = calloc(1,sizeof *j); j->refs=1; return j; }
void rede_job_reter(RedeJob *j) { pthread_mutex_lock(&lock); j->refs++; pthread_mutex_unlock(&lock); }
void rede_job_soltar(RedeJob *j) { pthread_mutex_lock(&lock); int last=!--j->refs; pthread_mutex_unlock(&lock); if(last) free(j); }
void rede_job_cancelar(RedeJob *j) { pthread_mutex_lock(&lock); j->canceled=1; pthread_mutex_unlock(&lock); }
RedeErro rede_job_estado(RedeJob *j) { pthread_mutex_lock(&lock); int c=j->canceled; pthread_mutex_unlock(&lock); return c?REDE_CANCELADO:REDE_OK; }
int rede_aquecer_job(const char *const *u, int n, unsigned *ms, RedeJob *j) {
  assert(n==1 && !strcmp(*u,"https://fixture.invalid")); (void)ms;
  pthread_mutex_lock(&lock); started++; pthread_mutex_unlock(&lock);
  for (int i=0;i<1000 && rede_job_estado(j)==REDE_OK;i++) usleep(1000);
  pthread_mutex_lock(&lock); canceled += j->canceled; finished++; pthread_mutex_unlock(&lock);
  return 0;
}
static void waitCount(int *value,int want) {
  for(int i=0;i<2000;i++) { pthread_mutex_lock(&lock); int got=*value; pthread_mutex_unlock(&lock); if(got==want)return; usleep(1000); }
  assert(!"worker did not finish");
}
int main(void) {
  retomamorna_foco("https://fixture.invalid/movie.mkv"); waitCount(&started,1);
  retomamorna_foco("https://fixture.invalid/movie.mkv"); // same focus starts once
  retomamorna_foco(NULL); waitCount(&finished,1); assert(canceled==1);
  retomamorna_foco("https://fixture.invalid/movie.mkv"); waitCount(&started,2);
  retomamorna_cancelar(); waitCount(&finished,2); assert(canceled==2);
  puts("warm focus: one request, focus leave/action cancels idle I/O; no decoder: PASS");
}
