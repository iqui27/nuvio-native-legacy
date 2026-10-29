#define _POSIX_C_SOURCE 200809L
#include "moviebuffer.h"
#include <string.h>
#if !defined(NV_DTS_FFMPEG) || defined(__EMSCRIPTEN__) || defined(NV_ANDROID)
int moviebuffer_url(const char *s,const char *h,int m,char *o,size_t n) { (void)s;(void)h;(void)m;(void)o;(void)n;return 0; }
void moviebuffer_stop(void) {}
int moviebuffer_info(DtsBufferInfo *i) { if(i) memset(i,0,sizeof *i);return 0; }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#define MB_CLIENTS 1
typedef struct MovieBuffer MovieBuffer;
typedef struct { MovieBuffer *s; int slot; } Client;
struct MovieBuffer {
  DtsEngine *input;
  pthread_mutex_t clients_lock;
  pthread_t threads[MB_CLIENTS];
  Client clients[MB_CLIENTS];
  int fd, sockets[MB_CLIENTS], threads_n, stop, fallback, probe_done, probe_ok, preemptible;
  pthread_cond_t probe_wake;
  char source[8192], path[96], mime[40];
};
static MovieBuffer *current;
static int stopped(MovieBuffer *s) { return __atomic_load_n(&s->stop,__ATOMIC_RELAXED); }
static int send_all(MovieBuffer *s,int fd,const void *data,size_t n) {
  const char *p=data;
  while(n && !stopped(s)) {
    if(s->preemptible) {
      struct pollfd pending[2]={{s->fd,POLLIN,0},{fd,POLLOUT,0}};
      int ready=poll(pending,2,3000);
      if(ready<0 && errno==EINTR)continue;
      if(ready<=0 || pending[0].revents || !(pending[1].revents&POLLOUT))return 0;
    }
    ssize_t sent=send(fd,p,n,MSG_NOSIGNAL | (s->preemptible?MSG_DONTWAIT:0));
    if(sent<0 && (errno==EINTR || (s->preemptible && (errno==EAGAIN || errno==EWOULDBLOCK)))) continue;
    if(sent<=0) return 0;
    p+=sent;n-=(size_t)sent;
  }
  return !n;
}
static void respond(MovieBuffer *s,int fd,int code,const char *text) {
  char h[256];
  int n=snprintf(h,sizeof h,"HTTP/1.1 %d %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",code,text);
  send_all(s,fd,h,(size_t)n);
}
static int number(const char **p,int64_t *v) {
  uint64_t n=0;const char *a=*p;
  while(**p>='0' && **p<='9') {
    unsigned digit=(unsigned)(*(*p)++-'0');
    if(n>((uint64_t)INT64_MAX-digit)/10) return 0;
    n=n*10+digit;
  }
  if(*p==a) return 0;
  *v=(int64_t)n;return 1;
}
/* Confirm support before video.c chooses the native buffer preset. */
static int probe_source(MovieBuffer *s) {
  int64_t total=dts_engine_source_total(s->input);
  {
    unsigned char probe[257]={0};int got=0;
    while(got<256) {
      int part=dts_engine_source_read(s->input,got,probe+got,256-got,-1);
      if(part<=0)break;
      got+=part;
    }
    if(got>0) {
      total=dts_engine_source_total(s->input);
      if(got>=4 && !memcmp(probe,"\x1a\x45\xdf\xa3",4))
        snprintf(s->mime,sizeof s->mime,"video/x-matroska");
      else if(got>=8 && !memcmp(probe+4,"ftyp",4))
        snprintf(s->mime,sizeof s->mime,"video/mp4");
      const char *text=(const char *)probe;
      if(got>=3 && probe[0]==0xef && probe[1]==0xbb && probe[2]==0xbf)text+=3;
      while(*text==' ' || *text=='\r' || *text=='\n' || *text=='\t')text++;
      if(!strncmp(text,"#EXTM3U",7) || !strncmp(text,"<?xml",5) || !strncmp(text,"<MPD",4))
        __atomic_store_n(&s->fallback,1,__ATOMIC_RELAXED);
    }
    printf("[movie-buffer] probe: bytes=%d total=%"PRId64" type=%s\n",got,total,s->mime);fflush(stdout);
  }
  return total>0 && !s->fallback;
}
static void *probe_worker(void *opaque) {
  MovieBuffer *s=opaque;
  int ok=probe_source(s);
  pthread_mutex_lock(&s->clients_lock);
  s->probe_ok=ok;s->probe_done=1;
  pthread_cond_signal(&s->probe_wake);
  pthread_mutex_unlock(&s->clients_lock);
  return NULL;
}
static void serve(MovieBuffer *s,int fd) {
  char request[16384]={0},method[8],path[128],header[1024];
  size_t used=0;
  while(used+1<sizeof request && !stopped(s)) {
    ssize_t n=recv(fd,request+used,sizeof request-used-1,0);
    if(n<0 && errno==EINTR) continue;
    if(n<=0) return;
    used+=(size_t)n;request[used]=0;
    if(strstr(request,"\r\n\r\n")) break;
  }
  if(!strstr(request,"\r\n\r\n") || sscanf(request,"%7s %127s",method,path)!=2) { respond(s,fd,400,"Bad Request");return; }
  if(strcmp(path,s->path)) { respond(s,fd,404,"Not Found");return; }
  int head=!strcmp(method,"HEAD");
  if(!head && strcmp(method,"GET")) { respond(s,fd,405,"Method Not Allowed");return; }
  char mime[40];
  int64_t total=dts_engine_source_total(s->input);
  snprintf(mime,sizeof mime,"%s",s->mime);
  if(stopped(s)) return;
  if(total<=0 || __atomic_load_n(&s->fallback,__ATOMIC_RELAXED)) {
    /* No authenticated, finite HTTP range stream: let webOS handle the source.
     * Redirect before emitting any media bytes; never forward partial data. */
    __atomic_store_n(&s->fallback,1,__ATOMIC_RELAXED);
    printf("[movie-buffer] fallback: no finite supported range stream\n");fflush(stdout);
    char redirect[8704];
    int n=snprintf(redirect,sizeof redirect,"HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",s->source);
    if(n>0 && (size_t)n<sizeof redirect) send_all(s,fd,redirect,(size_t)n);
    return;
  }
  int partial=0;int64_t start=0,end=total-1;
  for(char *line=strstr(request,"\r\n");line && line[2];line=strstr(line+2,"\r\n")) {
    const char *v=line+2;
    if(strncasecmp(v,"Range:",6)) continue;
    if(partial) { respond(s,fd,400,"Bad Request");return; }
    partial=1;v+=6;while(*v==' ' || *v=='\t') v++;
    if(strncmp(v,"bytes=",6)) { respond(s,fd,416,"Range Not Satisfiable");return; }
    v+=6;
    if(*v=='-') { int64_t suffix;v++;if(!number(&v,&suffix)||suffix<=0) { respond(s,fd,416,"Range Not Satisfiable");return; } start=suffix>=total?0:total-suffix; }
    else {
      if(!number(&v,&start)||*v++!='-') { respond(s,fd,416,"Range Not Satisfiable");return; }
      if(*v>='0'&&*v<='9'&&!number(&v,&end)) { respond(s,fd,416,"Range Not Satisfiable");return; }
    }
    while(*v==' '||*v=='\t')v++;
    if((*v!='\r'&&*v!='\n') || start>=total || end<start) { respond(s,fd,416,"Range Not Satisfiable");return; }
    if(end>=total)end=total-1;
  }
  int h;
  printf("[movie-buffer] %s: bytes=%"PRId64"-%"PRId64" total=%"PRId64" type=%s\n",method,start,end,total,mime);fflush(stdout);
  if(partial) h=snprintf(header,sizeof header,"HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\nContent-Range: bytes %"PRId64"-%"PRId64"/%"PRId64"\r\nContent-Length: %"PRId64"\r\nConnection: close\r\n\r\n",mime,start,end,total,end-start+1);
  else h=snprintf(header,sizeof header,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\nContent-Length: %"PRId64"\r\nConnection: close\r\n\r\n",mime,total);
  if(!send_all(s,fd,header,(size_t)h)||head)return;
  /* Keep short metadata ranges serialized; replace long playback responses
   * as soon as another request arrives, even while the old socket progresses. */
  s->preemptible=end-start+1>4*1024*1024;
  unsigned char data[65536];
  while(start<=end && !stopped(s)) {
    int want=end-start+1>(int64_t)sizeof data?(int)sizeof data:(int)(end-start+1);
    int got=dts_engine_source_read(s->input,start,data,want,s->preemptible?s->fd:-1);
    if(got<=0) {
      if(!stopped(s)) { printf("[movie-buffer] read failed at byte=%"PRId64" result=%d\n",start,got);fflush(stdout); }
      return;
    }
    if(!send_all(s,fd,data,(size_t)got)) return;
    start+=got;
  }
}
static void *worker(void *opaque) {
  Client *c=opaque;MovieBuffer *s=c->s;
  while(!stopped(s)) {
    struct pollfd listener={s->fd,POLLIN,0};
    if(poll(&listener,1,100)<=0)continue;
    if(stopped(s))break;
    int fd=accept(s->fd,NULL,NULL);
    if(fd<0) {
      if(stopped(s))break;
      if(errno!=EINTR) { struct timespec delay={0,50000000};nanosleep(&delay,NULL); }
      continue;
    }
    struct timeval timeout={3,0};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof timeout);
    pthread_mutex_lock(&s->clients_lock);
    s->sockets[c->slot]=fd;
    if(stopped(s))shutdown(fd,SHUT_RDWR);
    pthread_mutex_unlock(&s->clients_lock);
    /* One worker keeps engine reads serialized across replacement requests. */
    s->preemptible=0;
    if(!stopped(s))serve(s,fd);
    pthread_mutex_lock(&s->clients_lock);s->sockets[c->slot]=-1;close(fd);pthread_mutex_unlock(&s->clients_lock);
  }
  return NULL;
}
void moviebuffer_stop(void) {
  MovieBuffer *s=current;if(!s)return;current=NULL;
  __atomic_store_n(&s->stop,1,__ATOMIC_RELAXED);
  dts_engine_cancel(s->input);shutdown(s->fd,SHUT_RDWR);
  pthread_mutex_lock(&s->clients_lock);
  for(int i=0;i<MB_CLIENTS;i++)if(s->sockets[i]>=0)shutdown(s->sockets[i],SHUT_RDWR);
  pthread_mutex_unlock(&s->clients_lock);
  for(int i=0;i<s->threads_n;i++)pthread_join(s->threads[i],NULL);
  close(s->fd);dts_engine_destroy(s->input);
  pthread_cond_destroy(&s->probe_wake);
  pthread_mutex_destroy(&s->clients_lock);free(s);
}
int moviebuffer_info(DtsBufferInfo *info) {
  if(info) memset(info,0,sizeof *info);
  if(!current)return 0;
  if(__atomic_load_n(&current->fallback,__ATOMIC_RELAXED))return -1;
  dts_engine_buffer_info(current->input,info);return 1;
}
int moviebuffer_url(const char *source,const char *headers,int mib,char *out,size_t n) {
  moviebuffer_stop();
  if(mib!=64&&mib!=128&&mib!=256)return 0;
  if(!source || (strncmp(source,"http://",7)&&strncmp(source,"https://",8)) || strlen(source)>=8192 || strpbrk(source,"\r\n"))return 0;
  size_t path_n=strcspn(source,"?#");
  if((path_n>=5&&!strncasecmp(source+path_n-5,".m3u8",5)) || (path_n>=4&&!strncasecmp(source+path_n-4,".mpd",4)) || strstr(source,"127.0.0.1"))return 0;
  MovieBuffer *s=calloc(1,sizeof *s);if(!s)return 0;
  s->fd=-1;for(int i=0;i<MB_CLIENTS;i++)s->sockets[i]=-1;
  pthread_mutex_init(&s->clients_lock,NULL);
  pthread_cond_init(&s->probe_wake,NULL);
  s->input=dts_engine_source_create(source,headers,mib);
  s->fd=socket(AF_INET,SOCK_STREAM,0);current=s;
  if(!s->input||s->fd<0) { moviebuffer_stop();return 0; }
  struct sockaddr_in addr;memset(&addr,0,sizeof addr);addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(bind(s->fd,(struct sockaddr *)&addr,sizeof addr)||listen(s->fd,8)) {moviebuffer_stop();return 0;}
  socklen_t len=sizeof addr;
  if(getsockname(s->fd,(struct sockaddr *)&addr,&len)) {moviebuffer_stop();return 0;}
  unsigned char nonce[16];FILE *random=fopen("/dev/urandom","rb");
  int random_ok=random && fread(nonce,1,sizeof nonce,random)==sizeof nonce;if(random)fclose(random);
  if(!random_ok) {moviebuffer_stop();return 0;}
  char token[33];for(int i=0;i<16;i++)snprintf(token+i*2,3,"%02x",nonce[i]);
  const char *ext="";
  if(path_n>=4&&!strncasecmp(source+path_n-4,".mkv",4))ext="mkv";
  else if(path_n>=4&&!strncasecmp(source+path_n-4,".mp4",4))ext="mp4";
  snprintf(s->mime,sizeof s->mime,"%s",!strcmp(ext,"mkv")?"video/x-matroska":!strcmp(ext,"mp4")?"video/mp4":"application/octet-stream");
  /* Origin I/O stays off the UI thread; abandon cache startup after 500 ms. */
  if(pthread_create(&s->threads[0],NULL,probe_worker,s)) {moviebuffer_stop();return 0;}
  s->threads_n=1;
  struct timespec until;clock_gettime(CLOCK_REALTIME,&until);
  until.tv_nsec+=500000000;
  if(until.tv_nsec>=1000000000) {until.tv_sec++;until.tv_nsec-=1000000000;}
  pthread_mutex_lock(&s->clients_lock);
  while(!s->probe_done) {
    if(pthread_cond_timedwait(&s->probe_wake,&s->clients_lock,&until)==ETIMEDOUT)break;
  }
  int ready=s->probe_done && s->probe_ok;
  pthread_mutex_unlock(&s->clients_lock);
  if(!ready) {moviebuffer_stop();return 0;}
  pthread_join(s->threads[0],NULL);s->threads_n=0;
  snprintf(s->source,sizeof s->source,"%s",source);snprintf(s->path,sizeof s->path,"/%s/movie%s%s",token,*ext?".":"",ext);
  int count=snprintf(out,n,"http://127.0.0.1:%u%s",ntohs(addr.sin_port),s->path);
  if(count<0||(size_t)count>=n) {moviebuffer_stop();return 0;}
  for(int i=0;i<MB_CLIENTS;i++) {s->clients[i]=(Client){s,i};if(pthread_create(&s->threads[i],NULL,worker,&s->clients[i]))break;s->threads_n++;}
  if(!s->threads_n) {moviebuffer_stop();return 0;}
  printf("[movie-buffer] single-connection native URI cache enabled: %d MiB\n",mib);fflush(stdout);
  return 1;
}
#endif
