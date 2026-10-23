#include "m3u.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static M3uCanal c[8];
int main(void) {
  char epg[1100];int total;
  char body[]="\xef\xbb\xbf#EXTM3U x-tvg-url=\"/guide.xml,https://unused.invalid/epg\"\r\n"
    "#EXTVLCOPT:http-user-agent=Before EXTINF\n"
    "#EXTINF:-1 tvg-id=\"news.us\" tvg-name=\"Fallback\" tvg-logo=\"https://logo.invalid/a.png\" group-title=\"US, News\",Actual Channel\n"
    "#EXTVLCOPT:http-referrer=https://referer.invalid/\n"
    "https://cdn.invalid/live.m3u8?secret=token|Origin=https%3A%2F%2Forigin.invalid\n"
    "#EXTINF:-1 group-title=Unquoted tvg-id=second,Second\n"
    "#EXTGRP:Overridden group\n../second.ts\n"
    "#EXTINF:-1,Third\nhttps://cdn.invalid/third.ts|User-Agent=Player%20One&Referer=ok%0D%0AX-Leak%3A%20bad\n";
  assert(m3u_processar(body,"https://provider.invalid/dir/list.m3u?key=hidden",c,8,epg,sizeof epg,&total)==3);
  assert(total==3&&!strcmp(epg,"https://provider.invalid/guide.xml"));
  assert(!strcmp(c[0].nome,"Actual Channel")&&!strcmp(c[0].categoria,"US, News"));
  assert(!strcmp(c[0].epgId,"news.us")&&!strcmp(c[0].logo,"https://logo.invalid/a.png"));
  assert(strstr(c[0].cabecalhos,"User-Agent: Before EXTINF\n"));
  assert(strstr(c[0].cabecalhos,"Referer: https://referer.invalid/\n"));
  assert(strstr(c[0].cabecalhos,"Origin: https://origin.invalid\n"));
  assert(!strcmp(c[0].url,"https://cdn.invalid/live.m3u8?secret=token"));
  assert(!strcmp(c[1].categoria,"Overridden group")&&!c[1].cabecalhos[0]);
  assert(strstr(c[2].cabecalhos,"User-Agent: Player One\n")&&!strstr(c[2].cabecalhos,"X-Leak"));
  assert(!strncmp(c[0].id,"nvm3u:",6)&&strlen(c[0].id)==22&&!strstr(c[0].id,"token"));
  char hls[]="#EXTM3U\n#EXT-X-TARGETDURATION:4\n#EXTINF:4,\nseg.ts\n";
  assert(!m3u_processar(hls,"https://cdn.invalid/movie.m3u8",c,8,epg,sizeof epg,&total));
  char plain[]="#EXTM3U\n#EXTINF:-1,One\nfirst.ts\n#EXTINF:-1,Two\n//other.invalid/two.ts\n";
  assert(m3u_processar(plain,"https://provider.invalid",c,1,epg,sizeof epg,&total)==1&&total==2);
  assert(!strcmp(c[0].url,"https://provider.invalid/first.ts"));
  char huge[5000];memset(huge,'a',sizeof huge);memcpy(huge,"https://",8);huge[sizeof huge-1]=0;
  assert(!m3u_processar(huge,"https://provider.invalid/list",c,8,epg,sizeof epg,&total));
  char html[]="<html>Unauthorized</html>";
  assert(!m3u_processar(html,"https://provider.invalid/list",c,8,epg,sizeof epg,&total));
  char duplicates[]="#EXTM3U\n#EXTINF:-1 tvg-id=one,One\nhttps://cdn.invalid/a\n#EXTINF:-1 tvg-id=one,One\nhttps://cdn.invalid/a\n#EXTINF:-1 tvg-id=one,One\nhttps://cdn.invalid/b\n";
  assert(m3u_processar(duplicates,"https://provider.invalid/list",c,8,epg,sizeof epg,&total)==2&&total==3);
  assert(strcmp(c[0].id,c[1].id));
  char abandon[]="#EXTM3U\n#EXTINF:-1,Abandoned\n#EXTVLCOPT:http-referrer=https://old.invalid\n#EXTINF:-1,Actual\nhttps://cdn.invalid/actual\n";
  assert(m3u_processar(abandon,"https://provider.invalid/list",c,8,epg,sizeof epg,&total)==1&&!c[0].cabecalhos[0]);
  const int largeN=5000;size_t cap=largeN*160+32;
  char *large=malloc(cap);M3uCanal *many=calloc(largeN,sizeof *many);assert(large&&many);
  size_t used=(size_t)snprintf(large,cap,"#EXTM3U\n");
  for(int i=0;i<largeN;i++)used+=(size_t)snprintf(large+used,cap-used,"#EXTINF:-1 tvg-id=\"id%d\" group-title=\"Group %d\",Channel %d\nhttps://cdn.invalid/%d.ts\n",i,i%250,i,i);
  assert(used<cap);assert(m3u_processar(large,"https://provider.invalid/list",many,largeN,epg,sizeof epg,&total)==largeN&&total==largeN);
  assert(!strcmp(many[largeN-1].categoria,"Group 249"));free(large);free(many);
  puts("m3u parser: names, groups, EPG, headers, URLs, limits, HLS rejection and credential-free IDs PASS");
}
