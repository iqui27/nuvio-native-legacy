#ifndef NV_LTVKIND_H
#define NV_LTVKIND_H
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
/* Explicit provider type and VOD paths win. EPG/live transports protect real
 * movie channels (Telecine, HBO) from broad genre labels. */
static int ltv_kind(const char *type,const char *url,const char *name,
                    const char *group,const char *epg,double duration){
  if(type&&(!strcasecmp(type,"movie")||!strcasecmp(type,"vod")))return 1;
  if(type&&!strcasecmp(type,"series"))return 2;
  if(type&&!strcasecmp(type,"episode"))return 3;
  if(url&&(strcasestr(url,"/movie/")||strcasestr(url,"/vod/")))return 1;
  if(url&&strcasestr(url,"/series/"))return 3;
  if(url){const char *exts[]={".mp4",".mkv",".avi",".mov",".m4v",".webm"};
    size_t n=strcspn(url,"?|");
    for(unsigned i=0;i<sizeof exts/sizeof *exts;i++){size_t k=strlen(exts[i]);if(n>=k&&!strncasecmp(url+n-k,exts[i],k))return 1;}
  }
  if(duration>0)return 1;
  for(const char *p=name?name:"";*p;p++){
    if(tolower((unsigned char)*p)=='s'&&isdigit((unsigned char)p[1])){
      char *e;strtol(p+1,&e,10);if(tolower((unsigned char)*e)=='e'&&isdigit((unsigned char)e[1]))return 3;
    }
  }
  if(group&&(strcasestr(group,"vod")||strcasestr(group,"temporada")))return group&&strcasestr(group,"temporada")?3:1;
  if(type&&(!strcasecmp(type,"live")||!strcasecmp(type,"live_streams")))return 0;
  if((epg&&*epg)||(url&&(strcasestr(url,"/live/")||strcasestr(url,".m3u8")||strcasestr(url,".ts"))))return 0;
  if(group&&(strcasestr(group,"séries")||strcasestr(group,"series")))return 2;
  if(group&&(strcasestr(group,"filmes")||strcasestr(group,"movies")))return 1;
  return 0;
}
#endif
