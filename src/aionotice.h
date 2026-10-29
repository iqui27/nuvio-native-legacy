// Shared classification before playback, autoplay, external handoff or counting.
#ifndef NV_AIONOTICE_H
#define NV_AIONOTICE_H
#include "js.h"
#include <ctype.h>
#include <string.h>
static inline const char *aio_json_value(const char *p,int depth) {
  if(depth>32) return NULL;
  while(isspace((unsigned char)*p)) p++;
  if(*p=='"') {
    for(p++;*p;p++) {
      if(*p=='"') return p+1;
      if((unsigned char)*p<32) return NULL;
      if(*p=='\\') {
        p++; if(!*p) return NULL;
        if(*p=='u') { for(int i=0;i<4;i++) if(!isxdigit((unsigned char)*++p)) return NULL; }
        else if(!strchr("\"\\/bfnrt",*p)) return NULL;
      }
    }
    return NULL;
  }
  if(*p=='{' || *p=='[') {
    int object=*p=='{'; char close=object?'}':']'; p++;
    while(isspace((unsigned char)*p)) p++;
    if(*p==close) return p+1;
    for(;;) {
      if(object) {
        if(*p!='"' || !(p=aio_json_value(p,depth+1))) return NULL;
        while(isspace((unsigned char)*p)) p++;
        if(*p++!=':') return NULL;
      }
      if(!(p=aio_json_value(p,depth+1))) return NULL;
      while(isspace((unsigned char)*p)) p++;
      if(*p==close) return p+1;
      if(*p++!=',') return NULL;
      while(isspace((unsigned char)*p)) p++;
    }
  }
  if(!strncmp(p,"true",4) || !strncmp(p,"null",4)) return p+4;
  if(!strncmp(p,"false",5)) return p+5;
  if(*p=='-') p++;
  if(!isdigit((unsigned char)*p)) return NULL;
  if(*p=='0') p++; else while(isdigit((unsigned char)*p)) p++;
  if(*p=='.') { p++; if(!isdigit((unsigned char)*p)) return NULL; while(isdigit((unsigned char)*p)) p++; }
  if(*p=='e' || *p=='E') { p++; if(*p=='+' || *p=='-') p++; if(!isdigit((unsigned char)*p)) return NULL; while(isdigit((unsigned char)*p)) p++; }
  return p;
}
static inline int aio_json_valid(const char *p) {
  if(!p) return 0;
  const char *end=aio_json_value(p,0); if(!end) return 0;
  while(isspace((unsigned char)*end)) end++;
  return !*end;
}
static inline const char *aio_member(const char *p,const char *key) {
  if(!p || *p!='{') return NULL;
  for(p++;*p;) {
    while(isspace((unsigned char)*p)||*p==',') p++;
    if(*p!='"') return NULL;
    const char *name=++p;
    for(;*p && *p!='"';p++) if(*p=='\\' && p[1]) p++;
    size_t length=(size_t)(p-name);
    if(!*p++) return NULL;
    while(isspace((unsigned char)*p)) p++;
    if(*p++!=':') return NULL;
    while(isspace((unsigned char)*p)) p++;
    if(strlen(key)==length && !memcmp(name,key,length)) return p;
    if(*p=='{' || *p=='[') { p=js_fim(p); if(!p) return NULL; }
    else if(*p=='"') { for(p++;*p && *p!='"';p++) if(*p=='\\' && p[1]) p++; if(!*p++) return NULL; }
    else { while(*p && !strchr(",}",*p)) p++; }
  }
  return NULL;
}
static inline void aio_text_normalize(char *s) {
  unsigned char *p=(unsigned char *)s; char *out=s;
  while(*p) {
    if(p[0]==0xc2 && p[1]==0xa0) { *out++=' '; p+=2; }
    else if(p[0]==0xe2 && p[1]==0x80 && p[2]>=0x80 && p[2]<=0x8a) { *out++=' '; p+=3; }
    else if(*p=='\r') p++;
    else *out++=*p++;
  }
  *out=0;
}
static inline char *aio_label(char *s) {
  while(*s && (isspace((unsigned char)*s) || (unsigned char)*s>=128)) s++;
  return s;
}
static inline int aio_heading(const char *s) {
  static const char *const names[]={"Removal Reasons","Included Reasons","Pipeline Timing","Filter Breakdown","Precompute Breakdown","Service Wrap Breakdown","Config Variants","Digital Release Filter"};
  for(unsigned i=0;i<sizeof names/sizeof *names;i++) if(!strcmp(s,names[i])) return (int)i+2;
  const char *close=strchr(s,']');
  return *s=='[' && close && !strcmp(close,"] Scrape Summary") ? 1 : 0;
}
static inline int aio_notice(const char *p,const char *end) {
  char type[32]="", name[256]="", description[4096]="";
  const char *sd=aio_member(p,"streamData");
  if(sd) {
    js_texto_raiz_em(sd,js_fim(sd),"type",type,sizeof type);
    return !strcmp(type,"statistic")?1:!strcmp(type,"error")?2:0;
  }
  static const char *const targets[]={"url","infoHash","ytId","nzbUrl","nzb","servers","urls","clientResolve"};
  for(unsigned i=0;i<sizeof targets/sizeof *targets;i++) if(aio_member(p,targets[i])) return 0;
  js_texto_raiz_em(p,end,"name",name,sizeof name);
  js_texto_raiz_em(p,end,"description",description,sizeof description);
  aio_text_normalize(name); aio_text_normalize(description);
  int kind=aio_heading(aio_label(name)); char *d=aio_label(description);
  if(kind==1) return !strncmp(d,"Status",6) && strchr(d,':') ? 1 : 3;
  if(kind==2 || kind==3) return strchr(d,'(') && strchr(d,')') ? 1 : 3;
  if(kind==4) return !strncmp(d,"Fetch:",6) || !strncmp(d,"Total:",6) ? 1 : 3;
  if(kind>=5 && kind<=7) return !strncmp(d,"Total:",6) || !strncmp(d,"Fetcher:",8) || !strncmp(d,"Preferred regex:",16) ? 1 : 3;
  return kind || !aio_member(p,"externalUrl") ? 3 : 0;
}
#endif
