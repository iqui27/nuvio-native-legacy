#include "../src/dts/dts_engine.c"
#include <assert.h>
static void bytes(DtsEngine *e,int source,int64_t at,int n) {
  unsigned char block[65536];
  while(n>0) {
    int want=n>(int)sizeof block?(int)sizeof block:n;
    int got;
    if(source)got=dts_engine_source_read(e,at,block,want,-1);
    else { assert(seek_range(e,at,SEEK_SET)==at);got=read_range(e,block,want); }
    assert(got>0 && got<=want);
    for(int i=0;i<got;i++)assert(block[i]==(unsigned char)((at+i)%251));
    at+=got;n-=got;
  }
}
static void session(const char *url,const char *mode,int workers,int phase) {
  int source=!strcmp(mode,"native");
  dts_engine_set_http_workers(workers);dts_engine_set_source_workers(workers);
  DtsEngine *e=source?dts_engine_source_create(url,"Authorization: Bearer fixture\nReferer: https://fixture.invalid/\nUser-Agent: FixturePlayer",64):dts_engine_create();assert(e);
  if(!source) {
    e->url=strdup(url);e->headers=strdup("Authorization: Bearer fixture\nReferer: https://fixture.invalid/\nUser-Agent: FixturePlayer");
    e->source_public_headers=rede_range_headers(e->headers,1);
    if(!strcmp(mode,"dv")){e->dv=1;e->range_bytes=RANGE_BYTES_DV;e->range_slots=8;}
  }
  assert(e->http_workers==workers);
  // Settings changed for future sessions must not alter an existing engine.
  dts_engine_set_http_workers(5);dts_engine_set_source_workers(0);assert(e->http_workers==workers);
  bytes(e,source,0,1048576);
  struct timespec pause={0,150000000};nanosleep(&pause,NULL);
  printf("STEADY %d %d\n",phase,workers);fflush(stdout);assert(getchar()=='\n');
  bytes(e,source,96*1048576,1048576);bytes(e,source,123,65536);
  DtsBufferInfo info;dts_engine_buffer_info(e,&info);
  assert(info.http_workers==workers); // Published session snapshot survives settings changes.
  assert(info.capacity_bytes<=64*1048576LL && info.ready_bytes>=0);
  uint64_t before=clock_ns(CLOCK_MONOTONIC);dts_engine_cancel(e);dts_engine_destroy(e);
  assert(clock_ns(CLOCK_MONOTONIC)-before<3000000000ULL);
  puts("PASS: byte continuity, seeks, fixed connection snapshot, RAM bound and cancellation");
}
int main(int argc,char **argv) {
  assert(argc==4);
  int phase=0;
  const char *next=argv[3];
  while(*next) {
    char *end;long workers=strtol(next,&end,10);
    assert(end!=next && workers>=1 && workers<=4 && (!*end || *end==','));
    char url[1024];int n=snprintf(url,sizeof url,"%s/%d",argv[1],phase);
    assert(n>0 && n<(int)sizeof url);
    session(url,argv[2],(int)workers,phase++);
    next=*end?end+1:end;
  }
  assert(phase>0);
}

