#ifndef NV_DTS_VIDEO_DIAG_H
#define NV_DTS_VIDEO_DIAG_H
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "dts_media.h"
#include "mp4_hevc_sps.h"
typedef struct { unsigned scanned,nals; int vcl,irap,vps,sps,pps,rpu,el,clipped,invalid,sei,vcl_count,sps_ok; NvMp4Sps format; } NvVideoPacketDiag;
/* Diagnostic only: bounded inspection of borrowed Annex-B bytes. A missing
 * NAL in a clipped sample is inconclusive; never alter or copy the picture. */
static inline NvVideoPacketDiag nv_video_packet_scan(const unsigned char *p,size_t n,const char *codec,size_t budget,unsigned nal_limit) {
  NvVideoPacketDiag d={0};d.vcl=-1;
  size_t limit=n>budget?budget:n;d.scanned=(unsigned)limit;d.clipped=n>limit;
  int hevc=!strcmp(codec,"hevc"),avc=!strcmp(codec,"h264");
  if(!p || (!hevc && !avc))return d;
  for(size_t i=0;i+3<limit && d.nals<nal_limit;i++) {
    if(p[i] || p[i+1])continue;
    size_t h=p[i+2]==1?i+3:(i+4<limit && !p[i+2] && p[i+3]==1?i+4:0);
    if(!h || h>=limit || (hevc && h+1>=limit))continue;
    d.nals++;
    d.invalid|=(p[h]&128)!=0 || (hevc && !(p[h+1]&7));
    int type=hevc?(p[h]>>1)&63:p[h]&31;
    if(hevc) {
      if(type<=31) {d.vcl_count++;if(d.vcl<0)d.vcl=type;}
      d.sei|=type==39 || type==40;
      if(type==33 && !d.sps_ok) {
        size_t end=h+2;
        while(end+3<limit && !(p[end]==0 && p[end+1]==0 &&
              (p[end+2]==1 || (p[end+2]==0 && p[end+3]==1))))end++;
        if(end+3>=limit)end=limit;
        if(end-h<=8192 && nv_mp4_sps(p+h,end-h,&d.format)>0)d.sps_ok=1;
      }
      d.irap|=type>=16 && type<=23;d.vps|=type==32;d.sps|=type==33;
      d.pps|=type==34;d.rpu|=type==62;d.el|=(((p[h]&1)<<5)|(p[h+1]>>3))!=0;
    } else {
      if(type>=1 && type<=5 && d.vcl<0)d.vcl=type;
      d.irap|=type==5;d.sps|=type==7;d.pps|=type==8;
    }
    i=h;
  }
  if(d.nals>=nal_limit)d.clipped=1;
  return d;
}
static inline NvVideoPacketDiag nv_video_packet_diag(const unsigned char *p,size_t n,const char *codec) {
  return nv_video_packet_scan(p,n,codec,4096,32);
}
/* Six observations per open/seek: startup plus roughly one and five seconds
 * into 24 fps content. Scan/hash at most 2 MiB each; no packet allocation or
 * modification. clipped explicitly prevents a partial scan being called valid. */
static inline int nv_video_picture_sample(unsigned ordinal) {
  return (ordinal>=1 && ordinal<=4) || ordinal==24 || ordinal==120;
}
static inline uint32_t nv_video_picture_hash(const unsigned char *p,size_t n) {
  uint32_t h=UINT32_C(2166136261);
  size_t limit=n>2097152?2097152:n;
  if(!p)return 0;
  for(size_t i=0;i<limit;i++){h^=p[i];h*=UINT32_C(16777619);}
  return h;
}
static inline void nv_video_picture_log(const char *phase,unsigned epoch,unsigned ordinal,
                                        const DtsFrame *f,const char *codec,int feed,int copy_ok) {
  NvVideoPacketDiag d=nv_video_packet_scan(f->data,f->size,codec,2097152,256);
  printf("[picture-diag] phase=%s epoch=%u packet=%u bytes=%zu ptsNs=%lld dtsNs=%lld hash=%08x scanned=%u clipped=%d nals=%u vcl=%d slices=%d irap=%d vps=%d sps=%d pps=%d sei=%d rpu=%d el=%d headerErr=%d spsOk=%d size=%dx%d depth=%d colour=%d/%d/%d full=%d feed=%d copyOk=%d\n",
    phase,epoch,ordinal,f->size,(long long)f->pts_ns,(long long)f->dts_ns,
    (unsigned)nv_video_picture_hash(f->data,f->size),d.scanned,d.clipped,d.nals,d.vcl,d.vcl_count,
    d.irap,d.vps,d.sps,d.pps,d.sei,d.rpu,d.el,d.invalid,d.sps_ok,d.format.width,d.format.height,
    d.format.depth,d.format.primaries,d.format.transfer,d.format.matrix,d.format.full_range,feed,copy_ok);
  fflush(stdout);
}
#endif
