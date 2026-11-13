#ifndef NV_DTS_HEVC_DIAG_H
#define NV_DTS_HEVC_DIAG_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "mp4_hevc_sps.h"

/* Read-only experiment diagnostics. Never decode, retain or print media bytes.
 * Two MiB / 256 NALs per sampled packet, 256 bytes of stack RBSP syntax and a
 * bounded numeric NAL map. Length-prefixed demux and Annex-B output use the
 * same payload fingerprints, excluding framing so they can be compared. */
#define NV_HEVC_DIAG_BYTES 2097152u
typedef struct {
  unsigned nals, vcl, params, rpu, sei;
  size_t vcl_bytes;
  uint32_t vcl_hash, param_hash, rpu_hash, sei_hash;
  int clipped, malformed, map_clipped, syntax_unknown;
  int first_slice, slice_pps, sps_id, pps_id, pps_sps, vps_id;
  int dependent, extra_bits, tiles, wpp;
  char map[640];
} NvHevcDiag;
static inline uint32_t nv_hevc_diag_hash(uint32_t h,const unsigned char *p,size_t n) {
  for(size_t i=0;i<n;i++){h^=p[i];h*=UINT32_C(16777619);}return h;
}
static inline size_t nv_hevc_diag_start(const unsigned char *p,size_t n,size_t from,size_t *h) {
  for(size_t i=from;i+3<=n;i++)if(!p[i] && !p[i+1]) {
    if(p[i+2]==1){*h=i+3;return i;}
    if(i+4<=n && !p[i+2] && p[i+3]==1){*h=i+4;return i;}
  }
  *h=n;return n;
}
static inline void nv_hevc_diag_syntax(NvHevcDiag *d,const unsigned char *p,size_t n,int t,int *id,int *ref) {
  unsigned char rbsp[256];size_t used=0;unsigned zeros=0;
  *id=*ref=-1;
  for(size_t i=2;i<n && used<sizeof rbsp;i++) {
    if(zeros>=2 && p[i]==3) {
      if(i+1==n || p[i+1]>3){d->syntax_unknown=1;return;}
      zeros=0;continue;
    }
    rbsp[used++]=p[i];zeros=p[i]?0:zeros+1;
  }
  NvMp4Bits b={rbsp,used*8,0,0};
  if(t<=31) {
    int first=(int)nv_mp4_bits(&b,1);
    if(t>=16 && t<=23)nv_mp4_skip(&b,1);
    unsigned pps=nv_mp4_ue(&b);
    if(b.bad || pps>63){d->syntax_unknown=1;return;}
    *ref=(int)pps;
    if(d->slice_pps<0){d->slice_pps=(int)pps;d->first_slice=first;}
  } else if(t==32) {
    int vps=(int)nv_mp4_bits(&b,4);
    if(b.bad){d->syntax_unknown=1;return;}*id=d->vps_id=vps;
  } else if(t==33) {
    int vps=(int)nv_mp4_bits(&b,4);unsigned sub=nv_mp4_bits(&b,3);
    if(sub>6){d->syntax_unknown=1;return;}
    nv_mp4_skip(&b,1+96);
    unsigned prof[7]={0},lev[7]={0};
    for(unsigned i=0;i<sub;i++){prof[i]=nv_mp4_bits(&b,1);lev[i]=nv_mp4_bits(&b,1);}
    if(sub)nv_mp4_skip(&b,2*(8-sub));
    for(unsigned i=0;i<sub;i++)nv_mp4_skip(&b,88*prof[i]+8*lev[i]);
    unsigned sps=nv_mp4_ue(&b);
    if(b.bad || sps>15){d->syntax_unknown=1;return;}
    *id=d->sps_id=(int)sps;*ref=vps;
  } else if(t==34) {
    unsigned pps=nv_mp4_ue(&b),sps=nv_mp4_ue(&b);
    if(b.bad || pps>63 || sps>15){d->syntax_unknown=1;return;}
    *id=d->pps_id=(int)pps;*ref=d->pps_sps=(int)sps;
    int dep=(int)nv_mp4_bits(&b,1);nv_mp4_skip(&b,1);
    int extra=(int)nv_mp4_bits(&b,3);nv_mp4_skip(&b,2);
    nv_mp4_ue(&b);nv_mp4_ue(&b);nv_mp4_ue(&b); /* two ref counts, signed QP code */
    nv_mp4_skip(&b,2);if(nv_mp4_bits(&b,1))nv_mp4_ue(&b);
    nv_mp4_ue(&b);nv_mp4_ue(&b);nv_mp4_skip(&b,4);
    int tiles=(int)nv_mp4_bits(&b,1),wpp=(int)nv_mp4_bits(&b,1);
    if(b.bad){d->syntax_unknown=1;return;}
    d->dependent=dep;d->extra_bits=extra;d->tiles=tiles;d->wpp=wpp;
  }
}
static inline NvHevcDiag nv_hevc_diag_scan(const unsigned char *p,size_t n,unsigned length) {
  NvHevcDiag d;memset(&d,0,sizeof d);
  d.first_slice=d.slice_pps=d.sps_id=d.pps_id=d.pps_sps=d.vps_id=-1;
  d.dependent=d.extra_bits=d.tiles=d.wpp=-1;
  d.vcl_hash=d.param_hash=d.rpu_hash=d.sei_hash=UINT32_C(2166136261);
  size_t cap=n>NV_HEVC_DIAG_BYTES?NV_HEVC_DIAG_BYTES:n,pos=0,map_at=0;
  d.clipped=n>cap;
  if(!p || length>4){d.malformed=1;return d;}
  while(pos<cap && d.nals<256) {
    size_t h,end;
    if(length) {
      if(length>cap-pos){if(!d.clipped)d.malformed=1;break;}
      uint32_t size=0;for(unsigned j=0;j<length;j++)size=(size<<8)|p[pos+j];
      h=pos+length;
      if(size<2 || size>cap-h){if(!d.clipped)d.malformed=1;break;}
      end=h+size;
    } else {
      size_t begin=nv_hevc_diag_start(p,cap,pos,&h),next;
      if(begin==cap){if(!d.nals && !d.clipped)d.malformed=1;break;}
      end=nv_hevc_diag_start(p,cap,h,&next);
      pos=end;
      if(end<h || end-h<2){d.malformed=1;break;}
    }
    size_t size=end-h;int type=(p[h]>>1)&63,layer=((p[h]&1)<<5)|(p[h+1]>>3),tid=p[h+1]&7;
    d.nals++;if((p[h]&128) || !tid)d.malformed=1;
    uint32_t hash=nv_hevc_diag_hash(UINT32_C(2166136261),p+h,size);
    int id=-1,ref=-1;
    if(!layer && type<=34)nv_hevc_diag_syntax(&d,p+h,size,type,&id,&ref);
    if(type<=31){d.vcl++;d.vcl_bytes+=size;d.vcl_hash=nv_hevc_diag_hash(d.vcl_hash,p+h,size);}
    if(type>=32 && type<=34){d.params++;d.param_hash=nv_hevc_diag_hash(d.param_hash,p+h,size);}
    if(type==62){d.rpu++;d.rpu_hash=nv_hevc_diag_hash(d.rpu_hash,p+h,size);}
    if(type==39 || type==40){d.sei++;d.sei_hash=nv_hevc_diag_hash(d.sei_hash,p+h,size);}
    if(!d.map_clipped) {
      int wrote=snprintf(d.map+map_at,sizeof d.map-map_at,"%s%d/%d/%d;%zu;%08x;%d;%d",
                         map_at?",":"",type,layer,tid,size,(unsigned)hash,id,ref);
      if(wrote<0 || (size_t)wrote>=sizeof d.map-map_at){d.map[map_at]=0;d.map_clipped=1;}
      else map_at+=(size_t)wrote;
    }
    if(length)pos=end;
  }
  if(pos<cap && d.nals>=256)d.clipped=1;
  return d;
}
static inline void nv_hevc_diag_log(const char *phase,unsigned epoch,unsigned ordinal,
    const unsigned char *p,size_t n,unsigned length,int64_t pts,int64_t dts,int64_t duration,int key,int corrupt) {
  NvHevcDiag d=nv_hevc_diag_scan(p,n,length);
  printf("[hevc-structure] phase=%s epoch=%u packet=%u ptsNs=%lld dtsNs=%lld durNs=%lld keyframe=%d corrupt=%d length=%u bytes=%zu nals=%u vcl=%u vclBytes=%zu vclHash=%08x params=%u paramHash=%08x rpu=%u rpuHash=%08x sei=%u seiHash=%08x firstSlice=%d slicePps=%d spsId=%d ppsId=%d ppsSps=%d vpsId=%d dependent=%d extraBits=%d tiles=%d wpp=%d clipped=%d malformed=%d syntaxUnknown=%d mapClipped=%d nalMap=%s\n",
    phase,epoch,ordinal,(long long)pts,(long long)dts,(long long)duration,key,corrupt,length,n,d.nals,
    d.vcl,d.vcl_bytes,(unsigned)d.vcl_hash,d.params,(unsigned)d.param_hash,d.rpu,(unsigned)d.rpu_hash,
    d.sei,(unsigned)d.sei_hash,d.first_slice,d.slice_pps,d.sps_id,d.pps_id,d.pps_sps,d.vps_id,
    d.dependent,d.extra_bits,d.tiles,d.wpp,d.clipped,d.malformed,d.syntax_unknown,d.map_clipped,d.map[0]?d.map:"-");
}

/* Timestamp summary over the first 120 output packets, reset per open/seek.
 * Reordered PTS are expected with B-frames: counters are observations, not errors. */
typedef struct { unsigned count,steps,missing_pts,missing_dts,pts_back,dts_back;int64_t pts,dts,min_step,max_step; } NvVideoTiming;
static inline int64_t nv_video_diag_delta(int64_t a,int64_t b) {
  if(b>0 && a<INT64_MIN+b)return INT64_MIN;
  if(b<0 && a>INT64_MAX+b)return INT64_MAX;
  return a-b;
}
static inline void nv_video_timing_log(NvVideoTiming *s,const char *phase,unsigned epoch,int64_t pts,int64_t dts,int64_t duration,int emit) {
  if(s->count>=120)return;
  int pv=pts!=INT64_MIN,dv=dts!=INT64_MIN,prev=s->count!=0;
  int64_t dp=pv && prev && s->pts!=INT64_MIN?nv_video_diag_delta(pts,s->pts):0;
  int64_t dd=dv && prev && s->dts!=INT64_MIN?nv_video_diag_delta(dts,s->dts):0;
  if(prev && pv && s->pts!=INT64_MIN){
    if(dp<0)s->pts_back++;
    if(!s->steps || dp<s->min_step)s->min_step=dp;
    if(!s->steps || dp>s->max_step)s->max_step=dp;
    s->steps++;
  }
  if(prev && dv && s->dts!=INT64_MIN && dd<0)s->dts_back++;
  s->missing_pts+=!pv;s->missing_dts+=!dv;s->pts=pts;s->dts=dts;s->count++;
  if(emit)printf("[video-timing] phase=%s epoch=%u packet=%u ptsNs=%lld dtsNs=%lld durNs=%lld deltaPtsNs=%lld deltaDtsNs=%lld ptsBack=%u dtsBack=%u missingPts=%u missingDts=%u minStepNs=%lld maxStepNs=%lld\n",
    phase,epoch,s->count,(long long)pts,(long long)dts,(long long)duration,(long long)dp,(long long)dd,
    s->pts_back,s->dts_back,s->missing_pts,s->missing_dts,(long long)s->min_step,(long long)s->max_step);
}
#endif
