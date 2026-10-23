#include "guiav5.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>
const char *g5_item_category(const G5Item *item,int kind){
  (void)kind;
  if(!item)return "";
  if(item->group[0] && item->group[strspn(item->group,"0123456789")])return item->group;
  return item->name;
}

int g5_http_url(const char *url) {
  if (!url || strpbrk(url,"\r\n\t ")) return 0;
  const char *host = !strncmp(url,"https://",8) ? url+8 : !strncmp(url,"http://",7) ? url+7 : NULL;
  return host && *host && *host != '/' && !strchr(host,'@');
}
static void number_id(const char *p,const char *end,const char *key,char *out,size_t cap) {
  if (!js_texto(p,end,key,out,cap)) {
    double d=js_num(p,end,key,-1);
    if(d>=0 && d<2147483647)snprintf(out,cap,"%.0f",d);
  }
  for(const char *s=out;*s;s++) if(!isdigit((unsigned char)*s)){out[0]=0;break;}
}
int g5_parse_xtream(const char *json,int kind,G5Item *out,int cap,int *total) {
  int n=0,t=0;
  if(total)*total=0;
  if(!json || !out || cap<1)return -1;
  while(isspace((unsigned char)*json))json++;
  if(*json!='[' || !js_fim(json) || js_fim(json)[-1]!=']')return -1;
  const char *p=js_raiz_array(json);
  while(p){
    const char *end=js_fim(p);
    if(*p!='{' || !end || end[-1]!='}')return -1;
    G5Item it={.kind=kind,.age=-1};
    snprintf(it.source,sizeof it.source,"xtream");
    number_id(p,end,kind==G5_SERIES?"series_id":"stream_id",it.id,sizeof it.id);
    js_texto(p,end,"name",it.name,sizeof it.name);
    number_id(p,end,"category_id",it.group,sizeof it.group);
    js_texto(p,end,kind==G5_SERIES?"cover":"stream_icon",it.poster,sizeof it.poster);
    if(!g5_http_url(it.poster))it.poster[0]=0;
    js_texto(p,end,"container_extension",it.ext,sizeof it.ext);
    for(const char *s=it.ext;*s;s++)if(!isalnum((unsigned char)*s)){it.ext[0]=0;break;}
    js_texto(p,end,"plot",it.plot,sizeof it.plot);
    const char *background=js_array(p,end,"backdrop_path");
    if(background)js_cadeia(background,it.backdrop,sizeof it.backdrop);
    else js_texto(p,end,"backdrop_path",it.backdrop,sizeof it.backdrop);
    if(!g5_http_url(it.backdrop))it.backdrop[0]=0;
    it.adult=js_num(p,end,"is_adult",0)>0;
    char adult[8]="";js_texto(p,end,"is_adult",adult,sizeof adult);it.adult|=!strcmp(adult,"1");
    if(it.id[0] && it.name[0]){t++;if(n<cap)out[n++]=it;}
    p=js_prox(end);
  }
  if(total)*total=t;
  return n;
}
int g5_parse_episodes(const char *json,int season,G5Item *out,int cap){
  char key[16];snprintf(key,sizeof key,"%d",season);
  const char *start=strstr(json,"\"episodes\"");if(!start)return -1;
  start=strchr(start,'{');if(!start)return -1;
  const char *end=js_fim(start);if(!end||end[-1]!='}')return -1;
  const char *p=js_array(start,end,key);int n=0;
  while(p&&n<cap){const char *e=js_fim(p);if(!e||*p!='{'||e[-1]!='}')return -1;
    G5Item it={.kind=G5_EPISODE,.age=-1,.season=season};
    number_id(p,e,"id",it.id,sizeof it.id);js_texto(p,e,"title",it.name,sizeof it.name);
    js_texto(p,e,"container_extension",it.ext,sizeof it.ext);it.episode=(int)js_num(p,e,"episode_num",0);
    snprintf(it.source,sizeof it.source,"xtream");
    js_texto(p,e,"movie_image",it.poster,sizeof it.poster);if(!g5_http_url(it.poster))it.poster[0]=0;
    js_texto(p,e,"plot",it.plot,sizeof it.plot);
    for(const char *c=it.ext;*c;c++)if(!isalnum((unsigned char)*c)){it.ext[0]=0;break;}
    if(it.id[0]&&it.name[0])out[n++]=it;p=js_prox(e);
  }return n;
}
static void attr(const char *line,const char *name,char *out,size_t cap) {
  char key[80];snprintf(key,sizeof key,"%s=\"",name);
  const char *p=strstr(line,key);if(!p)return;p+=strlen(key);
  const char *e=strchr(p,'"');if(!e || (size_t)(e-p)>=cap)return;
  memcpy(out,p,(size_t)(e-p));out[e-p]=0;
}
void g5_playlist_init(G5Playlist *p,G5Item *items,int cap){memset(p,0,sizeof *p);p->items=items;p->cap=cap;}
static int playlist_line(G5Playlist *p){
  char *s=p->line;while(isspace((unsigned char)*s))s++;
  size_t n=strlen(s);while(n && isspace((unsigned char)s[n-1]))s[--n]=0;
  if(!*s)return 1;
  if(!p->header){if(strncmp(s,"#EXTM3U",7))return 0;p->header=1;return 1;}
  if(!strncmp(s,"#EXTINF:",8)){
    memset(&p->pending,0,sizeof p->pending);p->pending.age=-1;
    /* Commas inside quoted attributes are not title separators. */
    const char *comma=NULL;int quote=0;for(const char *c=s;*c;c++){if(*c=='"')quote=!quote;else if(*c==','&&!quote){comma=c;break;}}
    if(comma)snprintf(p->pending.name,sizeof p->pending.name,"%s",comma+1);
    attr(s,"tvg-id",p->pending.epg,sizeof p->pending.epg);
    attr(s,"group-title",p->pending.group,sizeof p->pending.group);
    attr(s,"tvg-logo",p->pending.poster,sizeof p->pending.poster);
    if(!g5_http_url(p->pending.poster))p->pending.poster[0]=0;
    return 1;
  }
  if(*s=='#')return 1;
  if(!p->pending.name[0] || !g5_http_url(s) || strlen(s)>=sizeof p->pending.url)return 0;
  G5Item *it=&p->pending;
  snprintf(it->source,sizeof it->source,"m3u");snprintf(it->url,sizeof it->url,"%s",s);
  uint64_t hash=UINT64_C(1469598103934665603);for(const char *c=s;*c;c++){hash^=(unsigned char)*c;hash*=UINT64_C(1099511628211);}
  snprintf(it->id,sizeof it->id,"%016llx",(unsigned long long)hash);
  const char *dot=strrchr(s,'.');
  it->kind=dot && (!strncasecmp(dot,".mp4",4)||!strncasecmp(dot,".mkv",4)||!strncasecmp(dot,".avi",4))?G5_MOVIE:G5_LIVE;
  p->total++;if(p->count<p->cap)p->items[p->count++]=*it;
  memset(it,0,sizeof *it);return 1;
}
int g5_playlist_feed(G5Playlist *p,const void *bytes,size_t n){
  const unsigned char *s=bytes;if(!p || p->failed)return 0;
  for(size_t i=0;i<n;i++){
    if(!s[i]){p->failed=1;return 0;}
    if(s[i]=='\n'){p->line[p->used]=0;if(!playlist_line(p)){p->failed=1;return 0;}p->used=0;}
    else{if(p->used+1>=sizeof p->line){p->failed=1;return 0;}p->line[p->used++]=(char)s[i];}
  }return 1;
}
int g5_playlist_finish(G5Playlist *p){
  if(p->failed)return -1;
  if(p->used){p->line[p->used]=0;if(!playlist_line(p))return -1;p->used=0;}
  return p->header?p->count:-1;
}
int g5_restricted(const G5Rules *r,const G5Lease *lease,const G5Item *it,int profile,time_t now){
  int locked=(r->hide_adult && it->adult) || (r->blocked_group[0]&&!strcmp(r->blocked_group,it->group)) ||
    (r->blocked_channel[0]&&!strcmp(r->blocked_channel,it->id)) ||
    (it->kind!=G5_LIVE && ((it->age<0 && r->ask_unrated) || it->age>r->max_age));
  for(int i=0;i<r->group_count&&i<G5_RULES_MAX;i++)if(!strcmp(r->blocked_groups[i],it->group))locked=1;
  for(int i=0;i<r->channel_count&&i<G5_RULES_MAX;i++)if(!strcmp(r->blocked_channels[i],it->id))locked=1;
  if(!locked)return 0;
  return !(lease && !r->kids && lease->profile==profile && lease->expires>now &&
    !strcmp(lease->source,it->source) && (!lease->id[0] || !strcmp(lease->id,it->id)));
}
void g5_grant(G5Lease *l,int profile,const char *source,const char *id,time_t now,int seconds,int verified){
  if(verified!=1 || seconds<1 || seconds>14400)return;
  memset(l,0,sizeof *l);l->profile=profile;l->expires=now+seconds;
  snprintf(l->source,sizeof l->source,"%s",source);snprintf(l->id,sizeof l->id,"%s",id?id:"");
}
int g5_live_slots(int requested,int decoders,int provider_max,int used){
  /* Current player API owns one session on every platform. */
  int n=requested;if(n>1)n=1;if(n>decoders)n=decoders;
  if(provider_max>=0 && n>provider_max-used)n=provider_max-used;
  return n<0?0:n;
}
