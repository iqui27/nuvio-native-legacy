#include "aiostreams.h"
#include "streams.h"
#include "addonstats.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  const char *mixed="{\"streams\":[{\"url\":\"https://media.test/v\"},{\"externalUrl\":\"https://project.test\",\"streamData\":{\"type\":\"statistic\"},\"name\":\"🟢 [Comet RD] Scrape Summary\",\"description\":\"✔ Status : SUCCESS\\n📦 Streams : 987\\n📋 Details : https://private.test/token\\n⏱️ Time : 117.00ms\\n\"},{\"url\":\"https://must-not-play.test/v\",\"externalUrl\":\"https://project.test\",\"streamData\":{\"type\":\"error\",\"addon\":\"Torrentio\",\"error\":{\"title\":\"Timeout\",\"description\":\"private-token\"}}}]}";
  AioSnapshot *s=calloc(1,sizeof *s); assert(s);
  aio_resultados(s,mixed);
  assert(s->resultados==1 && s->links==0);
  Stream *list=NULL;
  assert(stream_extrair(mixed,"AIOStreams",&list)==1); free(list);
  assert(s->estatisticas==1 && s->erros==1 && s->evidencias==2);
  assert(s->evidencia[0].kind==AIO_SCRAPE && s->evidencia[0].streams==987);
  assert(s->evidencia[0].status==1 && s->evidencia[0].elapsedMs==117);
  assert(!strcmp(s->evidencia[0].provider,"Comet") && !strcmp(s->evidencia[0].service,"RD"));
  assert(!strstr((char *)s->evidencia,"private"));
  const char *round2="{\"streams\":["
    "{\"streamData\":{\"type\":\"statistic\"},\"name\":\"⏱️ Pipeline Timing\",\"description\":\"📥 Fetch: 597ms\\n🔗 Service Wrap: 549ms\\n🎞️ Media Info: 220ms\\n🔍 Filter: 57ms\\n🔄 Dedup: 9ms\\n⚙️ Precompute: 3ms\\n📊 Sort: 34ms\\n✂️ Limit: 0ms\\n🎯 SEL: 0ms\\n⏱️ Total: ~1.210s\"},"
    "{\"streamData\":{\"type\":\"statistic\"},\"name\":\"🔗 Service Wrap Breakdown\",\"description\":\"⏱️ Total: 549ms (2 services)\\n☁️ AD (503 torrents | ✅ 8 cached | ⏳ 494 uncached)\\n    • Magnet check: 171ms\\n    • Processing: 3ms\\n    • Total: 174ms\\n\\n☁️ RD (503 torrents | ❌ Error)\\n    • Total: 200ms\"},"
    "{\"streamData\":{\"type\":\"statistic\"},\"name\":\"🔍 Removal Reasons\",\"description\":\"📌 Excluded Quality (6)\\n    • 2× SCR\\n    • 4× TS\"},"
    "{\"streamData\":{\"type\":\"statistic\"},\"name\":\"🔍 Removal Reasons\",\"description\":\"📌 Size (222)\\n    • 222× > 51.2 GB\"},"
    "{\"streamData\":{\"type\":\"statistic\"},\"name\":\"🔍 Filter Breakdown\",\"description\":\"⏱️ Total: 40ms (2 calls)\\nMetadata: 5ms\\nExpressions: 6ms\\nRegex compile: 7ms\\nRegex test: 8ms\\nFilter pass: 9ms\\n🔍 Per-stream phases:\\nTitle match: 2ms\\nSeason/Ep: 1ms\"},"
    "{\"streamData\":{\"type\":\"statistic\"},\"name\":\"⚙️ Precompute Breakdown\",\"description\":\"Fetcher: 2.5ms\\nPipeline: 1ms\\nPreferred regex: 1.2ms\"},"
    "{\"streamData\":{\"type\":\"statistic\"},\"name\":\"🟠 [private-token RD] Scrape Summary\",\"description\":\"Status : PARTIAL SUCCESS\\nStreams : 0\\nDetails : secrets\\n  multiline secrets\\nTime : -5ms\"},"
    "{\"streamData\":{\"type\":\"statistic\"},\"name\":\"new private-token format\",\"description\":\"Time: 500ms\"}]}";
  aio_resultados(s,round2);
  assert(s->resultados==0 && s->estatisticas==8 && s->erros==-1);
  assert(s->evidencia[0].elapsedMs==1210 && s->evidencia[0].approximate && s->evidencia[0].points==10);
  AioEvidencia *wrap=&s->evidencia[1];
  assert(wrap->elapsedMs==549 && wrap->point[1].cached==8 && wrap->point[1].uncached==494);
  assert(wrap->point[1].unclassified==1 && wrap->point[1].ms==174);
  assert(wrap->point[4].error && wrap->point[4].cached==-1);
  assert(s->evidencia[2].point[0].count==6 && s->evidencia[3].point[0].count==222);
  assert(s->evidencia[4].elapsedMs==40 && s->evidencia[4].points==8);
  assert(s->evidencia[5].elapsedMs==-1 && s->evidencia[5].point[0].ms==2.5);
  assert(s->evidencia[6].elapsedMs==-1 && s->evidencia[6].status==2 && s->evidencia[6].streams==0);
  assert(!strcmp(s->evidencia[6].provider,"Source") && s->evidencia[7].kind==AIO_NOTICE_UNKNOWN);
  assert(!strstr((char *)s->evidencia,"private-token"));
  const char *legacy="{\"streams\":[{\"name\":\"⏱️ Pipeline Timing\",\"description\":\"📥 Fetch: 2ms\\r\\n⏱️ Total: ~3ms\",\"externalUrl\":\"https://project.test\"},{\"name\":\"Pipeline Timing\",\"description\":\"Fetch: 2ms\",\"infoHash\":\"abcdef\"},{\"externalUrl\":\"https://external.test\"}]}";
  aio_resultados(s,legacy); assert(s->resultados==2 && s->links==1 && s->estatisticas==1);
  assert(stream_extrair(legacy,"AIOStreams",&list)==1); free(list);
  aio_resultados(s,"{\"streams\":[]}"); assert(s->responseValid && s->resultados==0 && s->erros==-1 && s->estatisticas==-1);
  aio_resultados(s,"{\"streams\":[{}"); assert(!s->responseValid);
  aio_search(s,"{\"success\":true,\"data\":{}}"); assert(s->filtrados==-1 && s->erros==-1 && s->estatisticas==-1);
  char *large=malloc(40000); assert(large);
  size_t used=sprintf(large,"{\"streams\":[");
  for(int i=0;i<70;i++) used+=sprintf(large+used,"%s{\"streamData\":{\"type\":\"statistic\"},\"name\":\"unknown\",\"description\":\"token\"}",i?",":"");
  strcpy(large+used,"]}"); aio_resultados(s,large);
  assert(s->estatisticas==70 && s->evidencias==AIO_EVIDENCE_MAX && s->evidenceOmitted==6); free(large);
  const char *hostile="{\"streams\":[{\"name\":\"notice private-token\",\"description\":\"error words do not prove failure\"},{\"name\":\"Pipeline Timing\",\"description\":\"Fetch: 1e2ms\\nTotal: nanms\",\"streamData\":{\"type\":\"statistic\"}}]}";
  aio_resultados(s,hostile); assert(s->resultados==0 && s->erros==-1 && s->evidencia[1].elapsedMs==-1);
  assert(s->evidencia[1].point[0].ms==-1 && !strstr((char *)s->evidencia,"private-token"));
  AddonAioRequest *history=calloc(10,sizeof *history); assert(history);
  for(int i=0;i<12;i++) addonstats_aio_complete("account-profile-install-variant","series",round2,200,0,100+i,0);
  assert(addonstats_aio_history("account-profile-install-variant","series",history,10)==10);
  assert(history[0].clientMs==102 && history[9].clientMs==111 && history[9].completed>0);
  assert(history[9].evidenceCount==8 && history[9].evidence[1].point[1].cached==8);
  assert(!addonstats_aio_history("other-profile","series",history,10));
  assert(!addonstats_aio_history("account-profile-install-variant","movie",history,10));
  int codes[]={401,429,503,0,0,200};
  for(int i=0;i<6;i++) addonstats_aio_complete("failures","movie",i==5?"broken":NULL,codes[i],i==3||i==4?REDE_TRANSPORTE:0,10,120);
  assert(addonstats_aio_history("failures","movie",history,10)==6);
  for(int i=0;i<6;i++) assert(!history[i].valid && history[i].playable==-1 && history[i].errors==-1);
  assert(addonstats_aio_wait("failures","movie")>0 && !addonstats_aio_wait("failures","series"));
  addonstats_zerar(); assert(!addonstats_aio_history("failures","movie",history,10));
  free(history);
  puts("AIO statistics: classification, typed round-two evidence, unknowns, scoped last-10 and backoff PASS");
  free(s);
}
