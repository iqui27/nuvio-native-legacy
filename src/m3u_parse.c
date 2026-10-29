#include "m3u.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
static int copiar(char *d,unsigned n,const char *s,size_t k) {
  if (!n || k >= n) { if(n)d[0]=0; return 0; }
  memcpy(d,s,k); d[k]=0; return 1;
}
static const char *espaco(const char *p) { while(*p && isspace((unsigned char)*p))p++; return p; }
static int atributo(const char *linha,const char *chave,char *out,unsigned n) {
  const char *p = linha; size_t k = strlen(chave);
  while(*p) {
    p=espaco(p);
    if(!strncasecmp(p,chave,k) && (p[k]=='=' || isspace((unsigned char)p[k]))) {
      const char *v=espaco(p+k); if(*v!='='){p++;continue;} v=espaco(v+1);
      const char *e; if(*v=='"'||*v=='\''){char q=*v++;e=strchr(v,q);if(!e)return 0;}
      else {e=v;while(*e && !isspace((unsigned char)*e) && *e!=',')e++;}
      return copiar(out,n,v,(size_t)(e-v));
    }
    // Skip an entire quoted value, whose words are not attribute names.
    while(*p && !isspace((unsigned char)*p)) {
      if(*p=='"'||*p=='\''){char q=*p++;while(*p&&*p!=q)p++;if(*p)p++;}
      else p++;
    }
  }
  return 0;
}
static const char *virgula(const char *p) {
  char q=0;for(;*p;p++){if(q){if(*p==q)q=0;}else if(*p=='"'||*p=='\'')q=*p;else if(*p==',')return p;}
  return NULL;
}
static int http(const char *p) { return !strncasecmp(p,"http://",7)||!strncasecmp(p,"https://",8); }
static uint64_t hash(uint64_t h,const char *s) { for(;*s;s++){h^=(unsigned char)*s;h*=UINT64_C(1099511628211);}return h; }
static void cabecalho(M3uCanal *c,const char *nome,const char *valor) {
  if(!valor||!*valor||strpbrk(valor,"\r\n"))return;
  size_t k=strlen(c->cabecalhos);int n=snprintf(c->cabecalhos+k,sizeof c->cabecalhos-k,"%s: %s\n",nome,valor);
  if(n<0||(size_t)n>=sizeof c->cabecalhos-k)c->cabecalhos[k]=0;
}
static int hex(int c) { if(c>='0'&&c<='9')return c-'0';c=tolower(c);return c>='a'&&c<='f'?c-'a'+10:-1; }
static void decodificar(char *p) {
  char *d=p;for(;*p;p++){if(*p=='%'&&p[1]&&p[2]&&hex(p[1])>=0&&hex(p[2])>=0){int c=hex(p[1])*16+hex(p[2]);*d++=(char)(c?c:' ');p+=2;}else *d++=*p=='+'?' ':*p;}*d=0;
}
static int resolver(const char *base,const char *url,char *out,unsigned n) {
  if (!url || !*url || strpbrk(url,"\r\n\t ")) return 0;
  if(http(url))return copiar(out,n,url,strlen(url));
  if(!base||!http(base)||strstr(url,"://"))return 0;
  const char *colon = strchr(url, ':'), *slash = strchr(url, '/');
  if (colon && (!slash || colon < slash)) return 0;
  if(!strncmp(url,"//",2)){const char *p=strchr(base,':');int k=snprintf(out,n,"%.*s:%s",(int)(p-base),base,url);return k>=0&&(unsigned)k<n;}
  const char *dom=strstr(base,"://")+3,*fim=strchr(dom,'/');
  if(*url=='/'){size_t k=fim?(size_t)(fim-base):strlen(base);int r=snprintf(out,n,"%.*s%s",(int)k,base,url);return r>=0&&(unsigned)r<n;}
  if(!fim){const char *q=strchr(dom,'?');size_t k=q?(size_t)(q-base):strlen(base);int r=snprintf(out,n,"%.*s/%s",(int)k,base,url);return r>=0&&(unsigned)r<n;}
  const char *q=strchr(base,'?');size_t k=q?(size_t)(q-base):strlen(base);
  while(k>0&&base[k-1]!='/')k--;
  if(k<(size_t)(dom-base)){k=fim?(size_t)(fim-base)+1:strlen(base);int r=snprintf(out,n,"%.*s/%s",(int)k,base,url);return r>=0&&(unsigned)r<n;}
  int r=snprintf(out,n,"%.*s%s",(int)k,base,url);return r>=0&&(unsigned)r<n;
}
int m3u_processar(char *buf,const char *origem,M3uCanal *out,int max,char *epg,unsigned ne,int *total) {
  int n=0,t=0, info=0; M3uCanal c={0};
  if(epg&&ne)epg[0]=0;
  if(total)*total=0;
  if(!buf||!out||max<1||max>INT_MAX/4)return 0;
  const char *primeiro = espaco(buf);
  if (!strncmp(primeiro,"\xef\xbb\xbf",3)) primeiro = espaco(primeiro+3);
  if (*primeiro != '#' && !http(primeiro)) return 0;
  size_t capacidade=1;
  while (capacidade < (size_t)max*2) capacidade*=2;
  int *vistos=calloc(capacidade,sizeof *vistos);
  if (!vistos) return 0;
  // An HLS media/master playlist is a video, not a list of TV channels.
  if(strstr(buf,"#EXT-X-TARGETDURATION")||strstr(buf,"#EXT-X-STREAM-INF")){free(vistos);return 0;}
  for(char *p=buf;*p;) {
    char *fim=strchr(p,'\n');if(fim)*fim++=0;else fim=p+strlen(p);
    size_t k=strlen(p);while(k&&isspace((unsigned char)p[k-1]))p[--k]=0;
    p=(char*)espaco(p);if(!strncmp(p,"\xef\xbb\xbf",3))p+=3;
    if(!strncasecmp(p,"#EXTM3U",7)) {
      char u[1100]="";
      if(atributo(p+7,"url-tvg",u,sizeof u)||atributo(p+7,"x-tvg-url",u,sizeof u)||atributo(p+7,"tvg-url",u,sizeof u)) {
        char *comma=strchr(u,',');if(comma)*comma=0;
        if(epg&&ne)resolver(origem,u,epg,ne);
      }
    } else if(!strncasecmp(p,"#EXTINF:",8)) {
      // Headers may precede EXTINF. A second EXTINF abandons the incomplete entry.
      if (info) memset(&c,0,sizeof c);
      info=1;
      c.nome[0]=c.logo[0]=c.categoria[0]=c.epgId[0]=0;
      atributo(p+8,"tvg-id",c.epgId,sizeof c.epgId);
      atributo(p+8,"tvg-logo",c.logo,sizeof c.logo);
      atributo(p+8,"group-title",c.categoria,sizeof c.categoria);
      const char *nome=virgula(p+8);
      if(nome&&*espaco(nome+1))copiar(c.nome,sizeof c.nome,espaco(nome+1),strlen(espaco(nome+1)));
      else atributo(p+8,"tvg-name",c.nome,sizeof c.nome);
    } else if(!strncasecmp(p,"#EXTGRP:",8))copiar(c.categoria,sizeof c.categoria,p+8,strlen(p+8));
    else if(!strncasecmp(p,"#EXTVLCOPT:",11)) {
      char *v=strchr(p+11,'=');if(v){*v++=0;
        if(!strcasecmp(p+11,"http-user-agent"))cabecalho(&c,"User-Agent",v);
        if(!strcasecmp(p+11,"http-referrer")||!strcasecmp(p+11,"http-referer"))cabecalho(&c,"Referer",v);
        if(!strcasecmp(p+11,"http-origin"))cabecalho(&c,"Origin",v);
      }
    } else if(*p && *p!='#') {
      char *op=strchr(p,'|');if(op)*op++=0;
      if((info || http(p) || *primeiro=='#') && resolver(origem,p,c.url,sizeof c.url)) {
        if(op)while(*op){char *e=strchr(op,'&');if(e)*e++=0;else e=op+strlen(op);char *v=strchr(op,'=');if(v){*v++=0;decodificar(v);
          if(!strcasecmp(op,"User-Agent"))cabecalho(&c,"User-Agent",v);
          if(!strcasecmp(op,"Referer"))cabecalho(&c,"Referer",v);
          if(!strcasecmp(op,"Origin"))cabecalho(&c,"Origin",v);
        }op=e;}
        if(!c.nome[0])snprintf(c.nome,sizeof c.nome,"M3U %d",t+1);
        uint64_t h=hash(UINT64_C(14695981039346656037),origem?origem:"");
        h=hash(h,c.epgId[0]?c.epgId:c.url);h=hash(h,c.nome);h=hash(h,c.categoria);
        snprintf(c.id,sizeof c.id,"nvm3u:%016" PRIx64,h);t++;
        size_t slot=(size_t)h & (capacidade-1);
        while (vistos[slot] && strcmp(out[vistos[slot]-1].id,c.id)) slot=(slot+1)&(capacidade-1);
        // Preserve distinct URLs sharing the same EPG ID/name/group.
        if (vistos[slot] && strcmp(out[vistos[slot]-1].url,c.url)) {
          h=hash(h,c.url);
          snprintf(c.id,sizeof c.id,"nvm3u:%016" PRIx64,h);
          slot=(size_t)h & (capacidade-1);
          while(vistos[slot] && strcmp(out[vistos[slot]-1].id,c.id)) slot=(slot+1)&(capacidade-1);
        }
        if(!vistos[slot] && n<max) { vistos[slot]=n+1;out[n++]=c; }
      }
      memset(&c,0,sizeof c);info=0;
    }
    p=fim;
  }
  free(vistos);
  if(total)*total=t;
  return n;
}
