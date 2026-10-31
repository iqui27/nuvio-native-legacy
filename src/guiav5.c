#include "guiav5.h"
#include "js.h"
#include "ltvkind.h"
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
int g5_parse_xtream_item(const char *p,const char *end,int kind,G5Item *out){
  if(!p||!end||!out||*p!='{'||end<=p||end[-1]!='}')return 0;
  G5Item it={.kind=kind,.age=-1};
  char stream_type[32]="";js_texto(p,end,"stream_type",stream_type,sizeof stream_type);
  if(stream_type[0])it.kind=ltv_kind(stream_type,"","","","",0);
  snprintf(it.source,sizeof it.source,"xtream");
  number_id(p,end,kind==G5_SERIES?"series_id":"stream_id",it.id,sizeof it.id);
  js_texto(p,end,"name",it.name,sizeof it.name);
  js_texto(p,end,"epg_channel_id",it.epg,sizeof it.epg);
  number_id(p,end,"category_id",it.group,sizeof it.group);
  js_texto(p,end,kind==G5_SERIES?"cover":"stream_icon",it.poster,sizeof it.poster);
  if(!it.poster[0])js_texto(p,end,"movie_image",it.poster,sizeof it.poster);
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
  js_texto(p,end,"year",it.year,sizeof it.year);
  if(!it.year[0]){int year=(int)js_num(p,end,"year",0);if(year>=1888&&year<=2200)snprintf(it.year,sizeof it.year,"%d",year);}
  if(!it.year[0]){char date[32]="";js_texto(p,end,"releaseDate",date,sizeof date);if(!date[0])js_texto(p,end,"releasedate",date,sizeof date);if(!date[0])js_texto(p,end,"release_date",date,sizeof date);if(strlen(date)>=4)snprintf(it.year,sizeof it.year,"%.4s",date);}
  js_texto(p,end,"genre",it.genre,sizeof it.genre);
  js_texto(p,end,"rating",it.rating,sizeof it.rating);
  if(!it.rating[0]){double score=js_num(p,end,"rating",0);if(score>0&&score<=10)snprintf(it.rating,sizeof it.rating,"%.1f",score);}
  *out=it;return it.id[0]&&it.name[0];
}
int g5_parse_xtream(const char *json,int kind,G5Item *out,int cap,int *total) {
  return g5_parse_xtream_page(json,kind,out,cap,0,total);
}
int g5_parse_xtream_page(const char *json,int kind,G5Item *out,int cap,int skip,int *total) {
  int n=0,t=0;
  if(total)*total=0;
  if(!json || !out || cap<1)return -1;
  while(isspace((unsigned char)*json))json++;
  if(*json!='[' || !js_fim(json) || js_fim(json)[-1]!=']')return -1;
  const char *p=js_raiz_array(json);
  while(p){
    const char *end=js_fim(p);
    if(*p!='{' || !end || end[-1]!='}')return -1;
    G5Item it;
    if(g5_parse_xtream_item(p,end,kind,&it)){t++;if(skip>0)skip--;else if(n<cap)out[n++]=it;}
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
    if(atof(s+8)>0)p->pending.kind=G5_MOVIE;
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
  it->kind=ltv_kind(NULL,s,it->name,it->group,it->epg,it->kind==G5_MOVIE?1:0);
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

/* --- multiview ------------------------------------------------------------ */
int g5_mv_layout(int mode,float x,float y,float w,float h,float gap,G5MvRect out[4]){
  if(mode<2)mode=2;if(mode>4)mode=4;
  const float A=16.0f/9.0f;
  if(mode==2){
    float sw=(w-gap)/2.0f,sh=sw/A;
    if(sh>h){sh=h;sw=h*A;}
    float x0=x+(w-(2*sw+gap))/2.0f,y0=y+(h-sh)/2.0f;
    out[0]=(G5MvRect){x0,y0,sw,sh};out[1]=(G5MvRect){x0+sw+gap,y0,sw,sh};
  }else if(mode==4){
    float sh=(h-gap)/2.0f,sw=sh*A;
    if(2*sw+gap>w){sw=(w-gap)/2.0f;sh=sw/A;}
    float x0=x+(w-(2*sw+gap))/2.0f,y0=y+(h-(2*sh+gap))/2.0f;
    out[0]=(G5MvRect){x0,y0,sw,sh};out[1]=(G5MvRect){x0+sw+gap,y0,sw,sh};
    out[2]=(G5MvRect){x0,y0+sh+gap,sw,sh};out[3]=(G5MvRect){x0+sw+gap,y0+sh+gap,sw,sh};
  }else{
    /* Big left (2hr+gap tall) + two stacked right, all 16:9:
     * W = A*(3*hr+gap)+gap <= w  and  2*hr+gap <= h. */
    float hr=(h-gap)/2.0f,lim=((w-gap)/A-gap)/3.0f;
    if(lim<hr)hr=lim;
    if(hr<1)hr=1;
    float wr=hr*A,hb=2*hr+gap,wb=hb*A;
    float x0=x+(w-(wb+gap+wr))/2.0f,y0=y+(h-hb)/2.0f;
    out[0]=(G5MvRect){x0,y0,wb,hb};
    out[1]=(G5MvRect){x0+wb+gap,y0,wr,hr};
    out[2]=(G5MvRect){x0+wb+gap,y0+hr+gap,wr,hr};
  }
  return mode;
}
int g5_mv_source_cap(const char *key,int xtream_max){
  if(!key||!key[0])return -1;
  if(!strcmp(key,"xt")||!strncmp(key,"xta",3))return xtream_max>0?xtream_max:1;
  if(!strcmp(key,"st")||!strncmp(key,"sta",3))return 1;
  return -1;   /* m<hash>/ma<id> M3U, a<hash> addon, g5m3u: no account cap */
}
int g5_mv_source_use(const char keys[][24],int n,const char *key){
  int u=0;if(!key||!key[0])return 0;
  for(int i=0;i<n;i++)if(!strcmp(keys[i],key))u++;
  return u;
}
void g5_mv_ctl_set(G5MvCtl *c,int slot,const char *id,const char *fonte,unsigned agora){
  if(slot<0||slot>3)return;
  G5MvSlot *s=&c->slot[slot];
  const char *i=id?id:"",*f=fonte?fonte:"";
  if(!strcmp(s->id,i)&&!strcmp(s->fonte,f))return;   /* same channel: no re-open */
  memset(s,0,sizeof *s);
  snprintf(s->id,sizeof s->id,"%s",i);
  snprintf(s->fonte,sizeof s->fonte,"%s",f);
  s->estado=i[0]?G5_MV_ABRINDO:G5_MV_VAZIO;
  (void)agora;
}
void g5_mv_ctl_erro(G5MvCtl *c,int slot,unsigned agora){
  if(slot<0||slot>3)return;
  G5MvSlot *s=&c->slot[slot];
  if(!s->id[0])return;
  if(s->tentativas<4){
    static const unsigned espera[4]={1000,3000,6000,10000};
    s->proxima=agora+espera[s->tentativas];
    s->tentativas++;
  }else s->proxima=0;   /* gave up: stays in error until a user action */
  s->estado=G5_MV_ERRO;
}
void g5_mv_ctl_pronto(G5MvCtl *c,int slot){
  if(slot<0||slot>3)return;
  G5MvSlot *s=&c->slot[slot];
  if(s->id[0]){s->estado=G5_MV_TOCANDO;s->tentativas=0;s->proxima=0;}
}
int g5_mv_ctl_abrir(G5MvCtl *c,int slot,unsigned agora){
  if(slot<0||slot>3)return 0;
  G5MvSlot *s=&c->slot[slot];
  if(s->estado==G5_MV_ERRO&&s->proxima&&(int)(agora-s->proxima)>=0){
    s->estado=G5_MV_ABRINDO;s->pediu=0;
  }
  if(s->estado==G5_MV_ABRINDO&&s->id[0]&&!s->pediu){s->pediu=1;c->aberturas++;return 1;}
  return 0;
}
