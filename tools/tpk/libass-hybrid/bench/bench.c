/* bench <fontdir> <hashout> <file.ass>...
 * Renders each track on a 24 fps grid at 1920x1080 with a fresh renderer,
 * times every ass_render_frame (sequential playback), REPS times (best total
 * kept), and writes one FNV hash per frame to <hashout> for byte compare.
 * env: STEP (ms, 42), REPS (3), NVTHREADS (ours: ass_nv_set_threads), MAXMS */
#define _FILE_OFFSET_BITS 64
#include <ass/ass.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define W 1920
#define H 1080
static uint64_t fnv(uint64_t h, const void *p, size_t n){const unsigned char*b=p;while(n--){h^=*b++;h*=1099511628211ULL;}return h;}
static uint64_t hash_frame(ASS_Image *img, unsigned *ni){uint64_t h=1469598103934665603ULL;for(;img;img=img->next){int hdr[5]={img->w,img->h,img->dst_x,img->dst_y,(int)img->color};h=fnv(h,hdr,sizeof hdr);for(int y=0;y<img->h;y++)h=fnv(h,img->bitmap+(size_t)y*img->stride,img->w);(*ni)++;}return h;}
static double now_ms(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec*1e3+ts.tv_nsec/1e6;}
static void load_fonts(ASS_Library *lib,const char*dir){DIR*d=opendir(dir);struct dirent*e;int n=0;while((e=readdir(d))){size_t l=strlen(e->d_name);if(l<5||strcmp(e->d_name+l-4,".ttf"))continue;char p[4096];snprintf(p,sizeof p,"%s/%s",dir,e->d_name);FILE*f=fopen(p,"rb");fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);char*b=malloc(sz);if(fread(b,1,sz,f)==(size_t)sz){ass_add_font(lib,e->d_name,b,sz);n++;}free(b);fclose(f);}closedir(d);if(!n)exit(2);}
static void quiet(int l,const char*f,va_list v,void*d){(void)l;(void)f;(void)v;(void)d;}
static int cmpd(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y;}
int main(int argc,char**argv){
  int step=getenv("STEP")?atoi(getenv("STEP")):42, reps=getenv("REPS")?atoi(getenv("REPS")):3;
  long long maxms=getenv("MAXMS")?atoll(getenv("MAXMS")):600000;
#ifdef HAVE_NV
  if(getenv("NVTHREADS")) ass_nv_set_threads(atoi(getenv("NVTHREADS")));
#endif
  ASS_Library*lib=ass_library_init(); ass_set_message_cb(lib,quiet,NULL); load_fonts(lib,argv[1]);
  FILE*ho=fopen(argv[2],"w"); double grand=0;
  for(int fi=3;fi<argc;fi++){
    ASS_Track*t0=ass_read_file(lib,argv[fi],NULL); long long end=0;
    for(int i=0;i<t0->n_events;i++){long long e=t0->events[i].Start+t0->events[i].Duration;if(e>end)end=e;}
    ass_free_track(t0); if(end>maxms)end=maxms;
    int nf=(int)(end/step)+1; double*ft=malloc(sizeof(double)*nf),*best=malloc(sizeof(double)*nf); double bestTot=1e300; unsigned imgs=0;
    for(int r=0;r<reps;r++){
      ASS_Renderer*rd=ass_renderer_init(lib); ass_set_frame_size(rd,W,H); ass_set_storage_size(rd,W,H);
      ass_set_fonts(rd,NULL,"Roboto",ASS_FONTPROVIDER_NONE,NULL,0);
      ASS_Track*t=ass_read_file(lib,argv[fi],NULL); double tot=0; imgs=0;
      for(int i=0;i<nf;i++){double a=now_ms();ASS_Image*im=ass_render_frame(rd,t,(long long)i*step,NULL);double b=now_ms();ft[i]=b-a;tot+=ft[i];
        uint64_t h=hash_frame(im,&imgs); if(r==0)fprintf(ho,"%s %lld %016llx\n",strrchr(argv[fi],'/')?strrchr(argv[fi],'/')+1:argv[fi],(long long)i*step,(unsigned long long)h);}
      if(tot<bestTot){bestTot=tot;memcpy(best,ft,sizeof(double)*nf);}
      ass_free_track(t); ass_renderer_done(rd);
    }
    qsort(best,nf,sizeof(double),cmpd);
    printf("%-14s frames %5d imgs %7u total %9.1f ms  mean %7.2f  p50 %7.2f  p95 %7.2f  max %7.2f\n",
      strrchr(argv[fi],'/')?strrchr(argv[fi],'/')+1:argv[fi],nf,imgs,bestTot,bestTot/nf,best[nf/2],best[(int)(nf*0.95)],best[nf-1]);
    grand+=bestTot; free(ft); free(best);
  }
  printf("TOTAL %.1f ms\n",grand); fclose(ho); ass_library_done(lib); return 0;}
