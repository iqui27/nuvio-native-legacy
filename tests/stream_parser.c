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
  puts("PASS ASan/UBSan: parser isolado, 100 fontes, MP4/DV na última posição.");
}
