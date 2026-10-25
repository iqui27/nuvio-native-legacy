#ifndef NV_HDR10PLUS_H
#define NV_HDR10PLUS_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Strip SMPTE ST 2094-40 (HDR10+) user_data_registered_itu_t_t35 SEI from HEVC.
 * Static HDR10 metadata (mastering display, content light level) is kept.
 * Annex-B in/out; returns 0 on success, -1 on malformed input or overflow. */

static inline int nv_hdr10plus_t35(const uint8_t *p, size_t n) {
  if (n < 4) return 0;
  size_t i = 0;
  if (p[i] != 0xB5) return 0;
  i++;
  if (p[i] == 0xFF) {
    if (n < 6) return 0;
    i += 2;
  }
  return i + 1 < n && p[i] == 0x00 && p[i + 1] == 0x3C;
}

static inline size_t nv_hdr10plus_start(const uint8_t *p, size_t n, size_t at, size_t *pre) {
  for (; at + 3 <= n; at++) {
    if (p[at] || p[at + 1]) continue;
    if (p[at + 2] == 1) { *pre = 3; return at; }
    if (at + 4 <= n && !p[at + 2] && p[at + 3] == 1) { *pre = 4; return at; }
  }
  *pre = 0;
  return n;
}

static inline int nv_hdr10plus_sei(const uint8_t *nal, size_t n, uint8_t *out, size_t cap, size_t *w) {
  size_t o = 2, used = 0;
  if (n < 3 || cap < 2) return -1;
  while (o + 1 < n) {
    unsigned pt = 0, ps = 0, b;
    do {
      if (o >= n) return -1;
      b = nal[o++];
      pt += b;
    } while (b == 255);
    do {
      if (o >= n) return -1;
      b = nal[o++];
      ps += b;
    } while (b == 255);
    if (!ps) continue;
    if (o + ps > n) return -1;
    if (pt == 4 && nv_hdr10plus_t35(nal + o, ps)) { o += ps; continue; }
    if (used + 4 + ps > cap) return -1;
    if (!used) { out[0] = nal[0]; out[1] = nal[1]; used = 2; }
    if (pt >= 255) { out[used++] = 255; out[used++] = (uint8_t)(pt - 255); }
    else out[used++] = (uint8_t)pt;
    if (ps >= 255) { out[used++] = 255; out[used++] = (uint8_t)(ps - 255); }
    else out[used++] = (uint8_t)ps;
    memcpy(out + used, nal + o, ps);
    used += ps;
    o += ps;
  }
  if (!used) { *w = 0; return 0; }
  out[used++] = 0x80;
  *w = used;
  return 0;
}

static inline int nv_hdr10plus_annexb(const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *out_n) {
  size_t pre, at = 0, w = 0;
  if (!in || !out || !out_n || n > 0x7fffffff) return -1;
  *out_n = 0;
  at = nv_hdr10plus_start(in, n, 0, &pre);
  if (!pre) return -1;
  while (at < n) {
    size_t begin = at + pre, nx;
    size_t end = nv_hdr10plus_start(in, n, begin, &nx);
    if (end - begin < 2) return -1;
    const uint8_t *nal = in + begin;
    size_t ns = end - begin;
    int type = (nal[0] >> 1) & 63;
    if (type == 39 || type == 40) {
      uint8_t tmp[65536];
      size_t nw = 0;
      if (nv_hdr10plus_sei(nal, ns, tmp, sizeof tmp, &nw) < 0) return -1;
      if (!nw) { at = end; pre = nx; continue; }
      if (w + 4 + nw > cap) return -1;
      out[w++] = 0; out[w++] = 0; out[w++] = 0; out[w++] = 1;
      memcpy(out + w, tmp, nw);
      w += nw;
    } else {
      if (w + 4 + ns > cap) return -1;
      out[w++] = 0; out[w++] = 0; out[w++] = 0; out[w++] = 1;
      memcpy(out + w, nal, ns);
      w += ns;
    }
    at = end;
    pre = nx;
  }
  *out_n = w;
  return 0;
}

#endif
