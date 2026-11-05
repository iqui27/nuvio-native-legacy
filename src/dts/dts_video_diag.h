#ifndef NV_DTS_VIDEO_DIAG_H
#define NV_DTS_VIDEO_DIAG_H
#include <stddef.h>
#include <string.h>
typedef struct { unsigned scanned,nals; int vcl,irap,vps,sps,pps,rpu,el,clipped,invalid; } NvVideoPacketDiag;
/* Diagnostic only: bounded inspection of borrowed Annex-B bytes. A missing
 * NAL in a clipped sample is inconclusive; never alter or copy the picture. */
static inline NvVideoPacketDiag nv_video_packet_diag(const unsigned char *p,size_t n,const char *codec) {
  NvVideoPacketDiag d={0};d.vcl=-1;
  size_t limit=n>4096?4096:n;d.scanned=(unsigned)limit;d.clipped=n>limit;
  int hevc=!strcmp(codec,"hevc"),avc=!strcmp(codec,"h264");
  if(!p || (!hevc && !avc))return d;
  for(size_t i=0;i+3<limit && d.nals<32;i++) {
    if(p[i] || p[i+1])continue;
    size_t h=p[i+2]==1?i+3:(i+4<limit && !p[i+2] && p[i+3]==1?i+4:0);
    if(!h || h>=limit || (hevc && h+1>=limit))continue;
    d.nals++;
    d.invalid|=(p[h]&128)!=0 || (hevc && !(p[h+1]&7));
    int type=hevc?(p[h]>>1)&63:p[h]&31;
    if(hevc) {
      if(type<=31 && d.vcl<0)d.vcl=type;
      d.irap|=type>=16 && type<=23;d.vps|=type==32;d.sps|=type==33;
      d.pps|=type==34;d.rpu|=type==62;d.el|=(((p[h]&1)<<5)|(p[h+1]>>3))!=0;
    } else {
      if(type>=1 && type<=5 && d.vcl<0)d.vcl=type;
      d.irap|=type==5;d.sps|=type==7;d.pps|=type==8;
    }
    i=h;
  }
  if(d.nals>=32)d.clipped=1;
  return d;
}
#endif
