/* Real HTTP reader: a progressing critical gap must retain its partial body. */
#include "rede.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
static volatile int cancelled;
static double now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void *cancel_soon(void *unused) {
  (void)unused;struct timespec t={0,150000000};nanosleep(&t,NULL);
  __atomic_store_n(&cancelled,1,__ATOMIC_RELAXED);return NULL;
}
int main(int argc,char **argv) {
  assert(argc==3);volatile int urgent=3;
  int stalled=0,status=0,retry=-1;long size=0;int64_t total=0;
  int stop=!strcmp(argv[2],"cancel"),idle=!strcmp(argv[2],"idle"),trickle=!strcmp(argv[2],"trickle");
  pthread_t t;if(stop)assert(!pthread_create(&t,NULL,cancel_soon,NULL));
  int64_t start=4*1048576LL-1,end=start+4*1048576LL-2;
  double began=now();
  char *body=rede_baixar_trecho64_relato(argv[1],"Authorization: Bearer engine-test\nX-Fixture: yes",start,end,
    &size,&total,&status,&cancelled,NULL,NULL,&urgent,2500,&stalled,&retry);
  double seconds=now()-began;
  if(stop) {pthread_join(t,NULL);assert(!body && seconds<1.5);}
  else if(idle || trickle) {assert(!body && stalled && seconds>=2 && seconds<7);}
  else {
    assert(body && status==206 && size==end-start+1 && !stalled && seconds>=5.5 && seconds<14.5);
    for(long i=0;i<size;i++)assert((unsigned char)body[i]==(unsigned char)((start+i)%251));
  }
  free(body);printf("PASS range %s: elapsed=%.3f status=%d stalled=%d bytes=%ld\n",argv[2],seconds,status,stalled,size);
}
