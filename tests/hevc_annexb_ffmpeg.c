/* hevc_annexb.h against the real FFmpeg hevc_mp4toannexb (pinned 7.1.5):
 * random hvcC and packets, valid and malformed. Without own parameter sets
 * before the IRAP the bytes must be identical; with them, the result must be
 * FFmpeg's output with the hvcC block removed (all three) or moved to the
 * start of the access unit, after a leading AUD (some). Rejections match. */
#include <assert.h>
#include <stdio.h>
#include <libavcodec/bsf.h>
#include <libavutil/mem.h>
#include "dts/hevc_annexb.h"

static uint32_t seed=0x1234567u;
static unsigned rnd(unsigned n) { seed=seed*1664525u+1013904223u; return (seed>>8)%n; }

static size_t nal(uint8_t *p,unsigned t,size_t len) {
  p[0]=(uint8_t)(t<<1);p[1]=1;
  for(size_t i=2;i<len;i++)p[i]=(uint8_t)rnd(256);
  return len;
}
/* FFmpeg filter, one packet. Returns 0 and fills out/size, or <0. */
static int ffmpeg_convert(const uint8_t *x,size_t xn,const uint8_t *d,size_t n,uint8_t *out,size_t *size) {
  const AVBitStreamFilter *f=av_bsf_get_by_name("hevc_mp4toannexb");
  AVBSFContext *b=NULL;AVPacket *p=av_packet_alloc();int r;
  assert(f && p && av_bsf_alloc(f,&b)==0);
  b->par_in->codec_type=AVMEDIA_TYPE_VIDEO;b->par_in->codec_id=AV_CODEC_ID_HEVC;
  b->par_in->extradata=av_mallocz(xn+AV_INPUT_BUFFER_PADDING_SIZE);memcpy(b->par_in->extradata,x,xn);
  b->par_in->extradata_size=(int)xn;
  if((r=av_bsf_init(b))<0)goto done;
  assert(av_new_packet(p,(int)n)==0);memcpy(p->data,d,n);
  if((r=av_bsf_send_packet(b,p))<0)goto done;
  if((r=av_bsf_receive_packet(b,p))<0)goto done;
  memcpy(out,p->data,(size_t)p->size);*size=(size_t)p->size;
done:
  av_packet_free(&p);av_bsf_free(&b);return r;
}

int main(void) {
  static uint8_t x[4096],d[65536],a[1<<17],o[1<<17],w[1<<17];
  unsigned same=0,inband=0,mixed=0,rejected=0,noirap=0;
  av_log_set_level(AV_LOG_QUIET);
  for(int iter=0;iter<20000;iter++) {
    /* hvcC: 1..3 arrays of VPS/SPS/PPS (+ occasional prefix SEI). */
    int ls=1+(int)rnd(4);if(ls==3)ls=4;
    size_t xn=0;memset(x,0,23);x[0]=1;x[21]=(uint8_t)(0xfc|(ls-1));
    unsigned arrays=1+rnd(4);x[22]=(uint8_t)arrays;xn=23;
    for(unsigned k=0;k<arrays;k++) {
      unsigned t=k<3?32+k:39,c=1+rnd(2);
      x[xn++]=(uint8_t)(0x80|t);x[xn++]=0;x[xn++]=(uint8_t)c;
      for(unsigned j=0;j<c;j++) { size_t l=3+rnd(40);x[xn++]=0;x[xn++]=(uint8_t)l;xn+=nal(x+xn,t,l); }
    }
    NvHevcAnnexB h={0};assert(nv_hevc_annexb_open(&h,x,xn)==1);
    /* Packet: optional AUD, random own sets, SEI, IRAP or not, more NALs. */
    size_t n=0;unsigned count=1+rnd(8),layout=rnd(4)==0; /* layout: VPS SPS PPS IRAP ... */
    if(layout && count<4)count=4;
    for(unsigned k=0;k<count;k++) {
      static const unsigned types[]={35,32,33,34,39,19,20,21,1,0,40,62,63,16,9};
      unsigned t=types[rnd(sizeof types/sizeof *types)];
      if(t==35 && k)t=39;
      if(layout && k<4)t=k<3?32+k:19;
      size_t l=2+rnd(ls==1?200:600);if(ls==1 && l>255)l=255;
      if(ls==2 && l>65535)l=65535;
      for(int b=0;b<ls;b++)d[n+(size_t)b]=(uint8_t)(l>>(8*(ls-1-b)));
      n+=(size_t)ls;n+=nal(d+n,t,l);
    }
    int corrupt=rnd(10)==0;
    if(corrupt) {   /* break one length or truncate */
      if(rnd(2)) n-=1+rnd((unsigned)(n<4?n:4)); else d[rnd((unsigned)ls)]^=(uint8_t)(1+rnd(255));
    }
    size_t an=0,on=0;int mode=-1;
    int rf=ffmpeg_convert(x,xn,d,n,a,&an);
    int ro=nv_hevc_annexb_packet(&h,d,n,NULL,0,&on,&mode);
    if(rf<0 || ro<0) {
      assert(rf<0 && ro<0);rejected++;nv_hevc_annexb_close(&h);continue;
    }
    assert(on<=sizeof o && nv_hevc_annexb_packet(&h,d,n,o,on,&on,NULL)==0);
    if(mode==NV_HEVC_ANNEXB_NO_IRAP || mode==NV_HEVC_ANNEXB_CONTAINER) {
      assert(an==on && !memcmp(a,o,on));
      if(mode==NV_HEVC_ANNEXB_NO_IRAP)noirap++;else same++;
      nv_hevc_annexb_close(&h);continue;
    }
    /* Locate FFmpeg's hvcC block: it sits right before the first IRAP NAL. */
    size_t at=0,i=0,wn=0;
    for(;;) {
      assert(i+4<=an);
      unsigned t=(a[i+4]>>1)&0x3f;
      if(i+h.sets_size<=an && !memcmp(a+i,h.sets,h.sets_size) && h.sets_size) {
        size_t j=i+h.sets_size;unsigned tj=(a[j+4]>>1)&0x3f;
        if(tj>=16 && tj<=23) { at=i;break; }
      }
      (void)t;
      size_t next=i+4;
      while(next+4<=an && !(a[next]==0&&a[next+1]==0&&a[next+2]==0&&a[next+3]==1))next++;
      assert(next+4<=an);i=next;
    }
    size_t aud=((a[4]>>1)&0x3f)==35?0:(size_t)-1,audn=0;
    if(aud==0) { size_t next=4;while(!(a[next]==0&&a[next+1]==0&&a[next+2]==0&&a[next+3]==1))next++;audn=next; }
    if(mode==NV_HEVC_ANNEXB_INBAND) {
      memcpy(w,a,at);wn=at;memcpy(w+wn,a+at+h.sets_size,an-at-h.sets_size);wn+=an-at-h.sets_size;inband++;
    } else {
      memcpy(w,a,audn);wn=audn;memcpy(w+wn,h.sets,h.sets_size);wn+=h.sets_size;
      memcpy(w+wn,a+audn,at-audn);wn+=at-audn;memcpy(w+wn,a+at+h.sets_size,an-at-h.sets_size);wn+=an-at-h.sets_size;mixed++;
    }
    assert(wn==on && !memcmp(w,o,on));
    nv_hevc_annexb_close(&h);
  }
  assert(same>1000 && inband>100 && mixed>1000 && rejected>500 && noirap>500);
  printf("PASS hevc_annexb vs FFmpeg %s hevc_mp4toannexb: identical=%u noIrap=%u ownAll=%u ownSome=%u rejectedBoth=%u\n",
    av_version_info(),same,noirap,inband,mixed,rejected);
  return 0;
}
