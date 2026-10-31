#include "moviebuffer.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
int main(int argc,char **argv) {
  assert(argc==3 || argc==4);
  if(argc==4 && strcmp(argv[3],"pending"))rede_discord_ca(argv[3]);
  char url[512],line[64];setvbuf(stdout,NULL,_IOLBF,0);
  int ok=moviebuffer_url(argv[1],"Authorization: Bearer fixture\nReferer: https://fixture.invalid/\nUser-Agent: FixturePlayer",atoi(argv[2]),url,sizeof url);
  puts("STARTED");
  if(argc==4 && !strcmp(argv[3],"pending")) {
    /* Allow the test harness to cancel while preflight is blocked. */
    if(fgets(line,sizeof line,stdin)) assert(line[0]=='q');
    moviebuffer_stop();puts("STOPPED");return 0;
  }
  while(ok && !moviebuffer_ready()) {
    struct timespec tick={0,1000000};nanosleep(&tick,NULL);
  }
  if(ok && moviebuffer_ready()<0) { moviebuffer_stop();ok=0; }
  printf("URL %s\n",ok?url:"OFF");
  while(fgets(line,sizeof line,stdin)) {
    if(line[0]!='s')break;
    DtsBufferInfo b;int active=moviebuffer_info(&b);
    printf("STATS %d %lld %lld\n",active,(long long)b.ready_bytes,(long long)b.capacity_bytes);
  }
  moviebuffer_stop();puts("STOPPED");
}
