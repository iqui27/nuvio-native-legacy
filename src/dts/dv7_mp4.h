#ifndef NV_DV7_MP4_H
#define NV_DV7_MP4_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Only metadata is retained from the separate enhancement-layer track. Never
 * decode or append its picture slices to the base-layer picture. */
#define NV_MP4_RPU_MAX 8192
#define NV_MP4_PACKET_MAX (4u * 1024u * 1024u)
#ifdef NV_DV7_CONVERSION
extern int nv_dv8_rpu_valid(const uint8_t *,size_t);
#endif
static inline int nv_mp4_p8_valid(const uint8_t *p,size_t n) {
#ifdef NV_DV7_CONVERSION
  return nv_dv8_rpu_valid(p,n)==1;
#else
  (void)p;(void)n;return 0;
#endif
}
static inline int nv_mp4_rpu(const uint8_t *p,size_t n,int length,
                             uint8_t out[NV_MP4_RPU_MAX],size_t *written) {
  if(!p || !out || !written || length<1 || length>4 || n>NV_MP4_PACKET_MAX)return -1;
  *written=0;
  for(size_t at=0;at<n;) {
    if(n-at<(size_t)length)return -1;
    uint32_t size=0;for(int k=0;k<length;k++)size=(size<<8)|p[at++];
    if(size<2 || size>n-at || (p[at]&0x80) || !(p[at+1]&7))return -1;
    if(((p[at]>>1)&63)==62) {
      if(*written || size<3 || size>NV_MP4_RPU_MAX)return -1;
      memcpy(out,p+at,size);*written=size;
    }
    at+=size;
  }
  return *written?0:-1;
}
#endif
