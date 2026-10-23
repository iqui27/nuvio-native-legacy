// Parser isolado: permite ASan sem carregar o inicializador de SDL do macOS.
#include "streams.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  char json[32000];size_t n=0;
  n+=snprintf(json+n,sizeof json-n,"{\"streams\":[");
  for(int i=0;i<100;i++) n+=snprintf(json+n,sizeof json-n,
    "%s{\"url\":\"https://example.invalid/%d\",\"behaviorHints\":{\"filename\":\"title.%s\"}}",
    i?",":"",i,i==99?"2160p.DV.Atmos.mp4":"1080p.DVDRip.mkv");
  snprintf(json+n,sizeof json-n,"]}");
  Stream *v=NULL;int count=stream_extrair(json,"fixture",&v);
  assert(count==100 && v[99].mp4 && v[99].dolbyVision && v[99].altura==2160);
  assert(!v[0].dolbyVision);free(v);
  count=stream_extrair("{\"streams\":[]}","fixture",&v);assert(count==0);free(v);
  count=stream_extrair("{\"streams\":["
    "{\"infoHash\":\"abc\",\"fileIdx\":1e99},"
    "{\"url\":\"https://example.invalid/a\",\"videoSize\":1e99},"
    "{\"url\":\"https://example.invalid/b\",\"title\":\"999999999999999999999999999999 GB\"},"
    "{\"infoHash\":\"def\",\"fileIdx\":2},"
    "{\"url\":\"https://example.invalid/c\",\"videoSize\":1073741824}]}","fixture",&v);
  assert(count==5 && v[0].fileIdx==-1 && v[1].tamanhoMB==0 && v[2].tamanhoMB==0);
  assert(v[3].fileIdx==2 && v[4].tamanhoMB==1024);free(v);
  count=stream_extrair("{\"streams\":["
    "{\"infoHash\":\"abc\",\"title\":\"Show S01E01 1080p\\n\xF0\x9F\x91\xA4 37 \xF0\x9F\x92\xBE 1.4 GB\"},"
    "{\"infoHash\":\"abd\",\"description\":\"Seeders: 0 | 2.1 GB\"},"
    "{\"infoHash\":\"abe\",\"title\":\"Show 720p 900 MB\"},"
    "{\"infoHash\":\"abf\",\"title\":\"\xF0\x9F\x91\xA4 99999999999999999999\"},"
    "{\"url\":\"https://example.invalid/d\",\"title\":\"\xF0\x9F\x91\xA4 12\"}]}","fixture",&v);
  assert(count==5 && v[0].temSemeadores && v[0].semeadores==37);
  assert(v[1].temSemeadores && v[1].semeadores==0);
  assert(!v[2].temSemeadores && v[3].temSemeadores && v[3].semeadores==999999999);
  assert(!v[4].temSemeadores);free(v);
  // Add-on text mode (#202-fmt): the line breaks of description/title survive the
  // parser (AIOStreams/Torrentio/Comet style), \r goes away, detection still works.
  count=stream_extrair("{\"streams\":["
    "{\"url\":\"https://example.invalid/e\",\"name\":\"[RD+] AIOStreams\\n4K\","
    "\"description\":\"\xF0\x9F\x92\xBE 12.3 GB \xF0\x9F\x91\xA4 45\\r\\n\xF0\x9F\x8E\x9E\xEF\xB8\x8F HEVC \xE2\x80\xA2 HDR10 \xE2\x80\xA2 DV\\n"
    "\xF0\x9F\x94\x8A DDP 5.1 Atmos\\n\xF0\x9F\x8C\x90 EN | PT\\n\xE2\x9A\x99\xEF\xB8\x8F RD+\"},"
    "{\"url\":\"https://example.invalid/f\",\"title\":\"Show 1080p\\n1.4 GB\"}]}","fixture",&v);
  assert(count==2);
  { int nl=0; for(const char *q=v[0].descricao;*q;q++) if(*q=='\n') nl++;
    assert(nl==4); assert(!strchr(v[0].descricao,'\r')); }
  assert(strstr(v[0].descricao,"\n") && v[0].dolbyVision && v[0].dolbyAtmos);
  assert(!strcmp(v[1].descricao,"Show 1080p\n1.4 GB") && v[1].altura==1080);
  free(v);
  // Plugin bridge fields are siblings; nested metadata must not name a stream.
  // Official Nuvio keeps title and description separately (toPluginStream).
  count=stream_extrair("{\"streams\":["
    "{\"url\":\"https://example.invalid/bridge\","
    "\"meta\":{\"name\":\"Error getting meta. Please try again later.\"},"
    "\"name\":\"4K\",\"title\":\"CastleTVProvider HDR10 DDP 5.1\",\"description\":\"2.1 GB\"},"
    "{\"url\":\"https://example.invalid/nested\","
    "\"meta\":{\"name\":\"Error getting meta. Please try again later.\"},"
    "\"title\":\"HDHub4u 1080p\"}]}","CNCVerse Bridge",&v);
  assert(count==2);
  assert(!strcmp(v[0].rotulo,"4K"));
  assert(!strcmp(v[0].descricao,"CastleTVProvider HDR10 DDP 5.1\n2.1 GB"));
  assert(v[0].altura==2160 && v[0].tamanhoMB>2000);
  assert(!strcmp(v[1].rotulo,"CNCVerse Bridge"));
  assert(!strcmp(v[1].descricao,"HDHub4u 1080p"));
  free(v);
  // #499: only explicit add-on signals claim cache; negatives win conflicts.
  count=stream_extrair("{\"streams\":["
    "{\"url\":\"https://example.invalid/1\",\"name\":\"⚡️ AIOStreams FHD\"},"
    "{\"url\":\"https://example.invalid/2\",\"name\":\"[TB+] Torrentio\"},"
    "{\"url\":\"https://example.invalid/3\",\"name\":\"[RD+] Torrentio\"},"
    "{\"url\":\"https://example.invalid/4\",\"name\":\"[AD+] Torrentio\"},"
    "{\"url\":\"https://example.invalid/5\",\"name\":\"[PM+] Torrentio\"},"
    "{\"url\":\"https://example.invalid/6\",\"clientResolve\":{\"isCached\":true}},"
    "{\"infoHash\":\"abc\",\"clientResolve\":{\"isCached\":true}},"
    "{\"url\":\"https://example.invalid/8\"},"
    "{\"url\":\"https://example.invalid/9\",\"name\":\"[TB download] Torrentio\"},"
    "{\"infoHash\":\"def\",\"name\":\"⚡ AIOStreams\",\"clientResolve\":{\"isCached\":false}},"
    "{\"url\":\"https://example.invalid/11\",\"name\":\"[RD+] uncached\"},"
    "{\"url\":\"https://example.invalid/12\",\"clientResolve\":{\"isCached\":\"true\"}},"
    "{\"url\":\"https://example.invalid/13\",\"other\":{\"clientResolve\":{\"isCached\":true}}},"
    "{\"url\":\"https://example.invalid/14\",\"clientResolve\":{\"other\":{\"isCached\":true}}},"
    "{\"infoHash\":\"ghi\"},"
    "{\"url\":\"https://example.invalid/16\",\"name\":\"⚡ AIOStreams\",\"clientResolve\":{\"isCached\":false}},"
    "{\"url\":\"https://example.invalid/17\",\"description\":\"[TB+] 1080p\"}]}","fixture",&v);
  assert(count==17);
  for (int i=0;i<7;i++) assert(v[i].emCache && !v[i].foraCache);
  assert(!strcmp(v[0].rotulo,"⚡️ AIOStreams FHD")); // raw name survives for details
  for (int i=7;i<16;i++) assert(!v[i].emCache);
  assert(v[16].emCache && !v[16].foraCache);
  assert(!v[7].foraCache && v[8].foraCache && v[9].foraCache && v[10].foraCache);
  for (int i=11;i<count;i++) assert(!v[i].foraCache);
  free(v);
  FILE *f = fopen("tests/fixtures/streams_notwebready.json", "r");
  assert(f);
  n = fread(json, 1, sizeof json - 1, f);
  assert(!ferror(f) && feof(f));
  fclose(f); json[n] = 0;
  count = stream_extrair(json, "fixture", &v);
  assert(count == 10);
  assert(!v[0].notWebReady && v[1].notWebReady && v[2].notWebReady && v[9].notWebReady);
  assert(!v[2].url[0] && !strcmp(v[2].infoHash, "abc"));
  for (int i = 3; i < 9; i++) assert(!v[i].notWebReady);
  assert(!strcmp(v[8].url, "https://example.invalid/media.mp4"));
  free(v);
  puts("PASS: parser isolado, cache explicito, notWebReady ancorado e externalUrl sem midia oculto.");
}
