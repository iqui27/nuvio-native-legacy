#ifndef NUVIO_DV7_H
#define NUVIO_DV7_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

/* Stateless transform, AFTER hevc_mp4toannexb. Caller owns the output and
 * publishes it only on success; a bad RPU must fall back, never advertise DV8.
 * Both three/four-byte Annex-B prefixes are accepted. EL is type 63 (the
 * interleaved P7 representation), or a nonzero HEVC layer_id. */
#ifdef NV_DV7_CONVERSION
extern ptrdiff_t nv_dv7_rpu(const uint8_t *, size_t, uint8_t *, size_t);
#ifdef NV_DV7_MAPPING_TEST
extern ptrdiff_t nv_dv7_rpu_mapping(const uint8_t *, size_t, uint8_t *, size_t, int);
extern ptrdiff_t nv_dv7_rpu_mapping_report(const uint8_t *,size_t,uint8_t *,size_t,int,uint32_t *);
#endif
#endif
static inline int nv_dv7_available(void) {
#ifdef NV_DV7_CONVERSION
  return 1;
#else
  return 0;
#endif
}
static inline int nv_dv7_selected(int profile,int p58,int p7) {
  return profile==7 ? !!p7 : (profile==5 || profile==8) ? !!p58 : 0;
}
static inline int nv_dv7_profile(int profile, int el, int bl, int rpu) {
  return bl && rpu && (((profile == 5 || profile == 8) && !el) ||
                       (profile == 7 && nv_dv7_available()));
}
static inline size_t nv_dv7_start(const uint8_t *p, size_t n, size_t i, size_t *prefix) {
  for (; i + 3 <= n; i++) {
    if (p[i] || p[i+1]) continue;
    if (p[i+2] == 1) { *prefix = 3; return i; }
    if (i + 4 <= n && !p[i+2] && p[i+3] == 1) { *prefix = 4; return i; }
  }
  *prefix = 0;
  return n;
}
static inline int nv_dv7_packet_report(const uint8_t *p, size_t n, uint8_t *out,
                                    size_t cap, size_t *written,int preserve,uint32_t *report) {
#if !defined(NV_DV7_MAPPING_TEST) || !defined(NV_DV7_CONVERSION)
  (void)preserve;
  (void)report;
#endif
  size_t pre, at, used = 0;
  if (!p || !out || !written || n > INT_MAX) return -1;
  *written = 0;
  if(report)*report=0;
  at = nv_dv7_start(p, n, 0, &pre);
  /* Annex-B permits leading_zero_8bits, never arbitrary garbage. */
  for (size_t i = 0; i < at; i++) if (p[i]) return -1;
  if (!pre) return -1;
  while (at < n) {
    size_t begin = at + pre, nextpre, end = nv_dv7_start(p, n, begin, &nextpre);
    if (end - begin < 2) return -1;
    const uint8_t *nal = p + begin;
    int type = (nal[0] >> 1) & 63;
    int layer = ((nal[0] & 1) << 5) | (nal[1] >> 3);
    if ((nal[0] & 0x80) || !(nal[1] & 7)) return -1;
    if (type != 63 && !layer) {
      size_t size = end - begin;
      if (used > cap || cap - used < 4) return -1;
      memcpy(out + used, "\0\0\0\1", 4); used += 4;
      if (type == 62) {
#ifdef NV_DV7_CONVERSION
        ptrdiff_t converted;
#ifdef NV_DV7_MAPPING_TEST
        if(report && !*report)converted=nv_dv7_rpu_mapping_report(nal,size,out+used,cap-used,preserve,report);
        else if(preserve)converted=nv_dv7_rpu_mapping(nal,size,out+used,cap-used,1);
        else
#endif
        converted=nv_dv7_rpu(nal,size,out+used,cap-used);
        if (converted <= 0 || (size_t)converted > cap - used) return -1;
        size = (size_t)converted;
#else
        return -1;
#endif
      } else {
        if (size > cap - used) return -1;
        memcpy(out + used, nal, size);
      }
      used += size;
    }
    at = end; pre = nextpre;
  }
  *written = used;
  return 0;
}
static inline int nv_dv7_packet_mode(const uint8_t *p,size_t n,uint8_t *out,
                                    size_t cap,size_t *written,int preserve) {
  return nv_dv7_packet_report(p,n,out,cap,written,preserve,NULL);
}
static inline int nv_dv7_packet(const uint8_t *p,size_t n,uint8_t *out,
                               size_t cap,size_t *written) {
  return nv_dv7_packet_mode(p,n,out,cap,written,0);
}
#endif
