#ifndef NV_HEVC_SEI_H
#define NV_HEVC_SEI_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Experimental webOS compatibility filter. Recognize only registered T.35
 * ST 2094-40: B5/003C/0001, application 4, version 0 or 1. All other SEI payloads,
 * VCL, VPS/SPS/PPS and Dolby Vision RPU bytes are retained. This is an original
 * streaming implementation; no decoded picture or extra packet is allocated.
 * Validate the entire packet before writing. Unknown/malformed/oversize SEI
 * leaves the entire packet unchanged, including otherwise recognized messages.
 * References: ST 2094-40 T.35 registration; Kodi webOS SetupBitstreamConverter
 * removes this same metadata class for firmware compatibility.
 */
#define NV_HEVC_SEI_PACKET_MAX (16u*1024u*1024u)
#define NV_HEVC_SEI_NAL_MAX 65536u
#define NV_HEVC_SEI_MESSAGES_MAX 256u
typedef struct {
  unsigned prefix, suffix, messages, t35, removed, malformed, oversized;
} NvHevcSeiReport;
typedef struct { const uint8_t *p; size_t n, at; unsigned zeros; } NvHevcSeiReader;
typedef struct { uint8_t *p; size_t at, cap; unsigned zeros; } NvHevcSeiWriter;
static inline int nv_hevc_sei_byte(NvHevcSeiReader *r,uint8_t *out) {
  if(r->at>=r->n)return 0;
  uint8_t c=r->p[r->at++];
  if(r->zeros==2 && c==3) {
    if(r->at>=r->n || r->p[r->at]>3)return 0;
    r->zeros=0;c=r->p[r->at++];
  } else if(r->zeros==2 && c<3)return 0;
  r->zeros=c?0:r->zeros+1;*out=c;return 1;
}
static inline int nv_hevc_sei_put(NvHevcSeiWriter *w,uint8_t c) {
  if(w->zeros==2 && c<=3) {
    if(w->at>=w->cap)return 0;
    w->p[w->at++]=3;w->zeros=0;
  }
  if(w->at>=w->cap)return 0;
  w->p[w->at++]=c;w->zeros=c?0:w->zeros+1;return 1;
}
static inline int nv_hevc_sei_number(NvHevcSeiReader *r,size_t *value,size_t *bytes) {
  uint8_t c;*value=*bytes=0;
  do {
    if(!nv_hevc_sei_byte(r,&c))return 0;
    *value+=c;(*bytes)++;
  } while(c==255);
  return 1;
}
static inline int nv_hevc_sei_trailer(const NvHevcSeiReader *r) {
  if(r->at>=r->n || r->p[r->at]!=128)return 0;
  for(size_t i=r->at+1;i<r->n;i++)if(r->p[i])return 0;
  return 1;
}
/* Return -1 for invalid syntax, otherwise the retained NAL length (zero when
 * it contained only recognized HDR10+). Writes never grow the input NAL. */
static inline int nv_hevc_sei_nal(uint8_t *p,size_t n,int write,NvHevcSeiReport *s) {
  if(write) {
    NvHevcSeiReport check={0};
    if(nv_hevc_sei_nal(p,n,0,&check)<0)return -1;
    if(!check.removed)return (int)n;
  }
  NvHevcSeiReader r={p,n,2,0};NvHevcSeiWriter w={p,2,n,0};
  unsigned kept=0,count=0;
  if(n<3 || (p[0]&128) || !(p[1]&7))return -1;
  while(!nv_hevc_sei_trailer(&r)) {
    if(++count>NV_HEVC_SEI_MESSAGES_MAX)return -1;
    NvHevcSeiReader begin=r;size_t type,size,type_bytes,size_bytes;
    if(!nv_hevc_sei_number(&r,&type,&type_bytes) ||
       !nv_hevc_sei_number(&r,&size,&size_bytes) || size>n)return -1;
    uint8_t signature[7]={0};
    for(size_t i=0;i<size;i++) {
      uint8_t c;if(!nv_hevc_sei_byte(&r,&c))return -1;
      if(i<sizeof signature)signature[i]=c;
    }
    s->messages++;if(type==4)s->t35++;
    int hdr=type==4 && size>=7 && signature[0]==0xb5 &&
      signature[1]==0 && signature[2]==0x3c && signature[3]==0 &&
      signature[4]==1 && signature[5]==4 && signature[6]<=1;
    if(hdr){s->removed++;continue;}
    kept++;
    if(write)for(size_t i=0;i<type_bytes+size_bytes+size;i++) {
      uint8_t c;if(!nv_hevc_sei_byte(&begin,&c) || !nv_hevc_sei_put(&w,c))return -1;
    }
  }
  if(!write)return (int)n;
  if(!kept)return 0;
  if(!nv_hevc_sei_put(&w,128))return -1;
  /* Annex-B trailing_zero_8bits belong to the byte stream, outside RBSP. */
  size_t trailing=n-r.at-1;
  if(trailing>w.cap-w.at)return -1;
  memset(p+w.at,0,trailing);w.at+=trailing;
  return (int)w.at;
}
static inline size_t nv_hevc_start(const uint8_t *p,size_t n,size_t from,size_t *prefix) {
  for(size_t i=from;i+3<=n;i++)if(!p[i] && !p[i+1]) {
    if(p[i+2]==1){*prefix=3;return i;}
    if(i+4<=n && !p[i+2] && p[i+3]==1){*prefix=4;return i;}
  }
  *prefix=0;return n;
}
static inline int nv_hevc_sei_walk(uint8_t *p,size_t n,int write,size_t *out,NvHevcSeiReport *s) {
  size_t prefix=0,start=nv_hevc_start(p,n,0,&prefix),used=start;
  if(start==n)return 0;
  for(size_t i=0;i<start;i++)if(p[i])return 0;
  unsigned nals=0;
  while(start<n) {
    if(++nals>4096)return 0;
    size_t h=start+prefix,next_prefix=0,next=nv_hevc_start(p,n,h,&next_prefix);
    if(next-h<2)return 0;
    unsigned type=(p[h]>>1)&63;
    size_t retained=next-h;
    if(type==39 || type==40) {
      if(type==39)s->prefix++;else s->suffix++;
      if(retained>NV_HEVC_SEI_NAL_MAX){s->oversized++;return 0;}
      /* Never interpret enhancement-layer metadata as base-layer SEI. */
      if(!(p[h]&1) && !(p[h+1]&0xf8)) {
        int result=nv_hevc_sei_nal(p+h,retained,write,s);
        if(result<0){s->malformed++;return 0;}
        retained=(size_t)result;
      }
    }
    if(write && retained) {
      memmove(p+used,p+start,prefix);used+=prefix;
      memmove(p+used,p+h,retained);used+=retained;
    }
    start=next;prefix=next_prefix;
  }
  *out=write?used:n;return 1;
}
/* 1: rewritten, 0: byte-identical pass-through. Malformed input is reported
 * separately and is never partially rewritten. No silent packet truncation. */
static inline int nv_hevc_sei_filter(uint8_t *p,size_t n,size_t *out,NvHevcSeiReport *s) {
  memset(s,0,sizeof *s);*out=n;
  if(!p || !n || n>NV_HEVC_SEI_PACKET_MAX){s->oversized=1;return 0;}
  size_t checked=n;
  if(!nv_hevc_sei_walk(p,n,0,&checked,s)) {
    if(!s->malformed && !s->oversized)s->malformed=1;
    return 0;
  }
  if(!s->removed)return 0;
  NvHevcSeiReport written={0};
  /* The dry pass proved every length and byte. The owned adapter packet is
   * immutable between passes; removing >=9 RBSP bytes per message pays for
   * any new emulation-prevention byte at a newly joined message boundary. */
  if(!nv_hevc_sei_walk(p,n,1,out,&written))return -1;
  return 1;
}
#endif
