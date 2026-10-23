/* HDR10+ (SMPTE ST 2094-40) SEI strip: unit test on hand-built Annex-B HEVC. */
#include "../src/dts/hdr10plus.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* SEI NAL: type 39 (prefix SEI), two-byte header, then one message. */
static size_t sei(uint8_t *o, unsigned type, const uint8_t *payload, size_t n) {
  o[0] = (39 << 1);   /* nal_unit_type 39, nuh_layer_id 0, nuh_temporal_id_plus1 1 */
  o[1] = 1;
  size_t w = 2;
  if (type >= 255) { o[w++] = 255; o[w++] = (uint8_t)(type - 255); }
  else o[w++] = (uint8_t)type;
  if (n >= 255) { o[w++] = 255; o[w++] = (uint8_t)(n - 255); }
  else o[w++] = (uint8_t)n;
  memcpy(o + w, payload, n);
  w += n;
  o[w++] = 0x80;   /* rbsp_trailing_bits */
  return w;
}

static size_t cat(uint8_t *dst, const uint8_t *nal, size_t n, int prefix4) {
  size_t w = 0;
  if (prefix4) { dst[0] = 0; dst[1] = 0; dst[2] = 0; dst[3] = 1; w = 4; }
  else { dst[0] = 0; dst[1] = 0; dst[2] = 1; w = 3; }
  memcpy(dst + w, nal, n);
  return w + n;
}

int main(void) {
  uint8_t in[4096], out[4096], nal[512], exp[4096];
  size_t n, w, e;

  /* HDR10+ payload: T.35 country 0xB5, provider 0x003C, then body. */
  const uint8_t hdr10p[] = { 0xB5, 0x00, 0x3C, 0x00, 0x01, 0x04, 0x01, 0xAA, 0xBB };
  /* Static HDR10: mastering display (payload type 137) and CLF (144). */
  const uint8_t mdm[] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D };
  const uint8_t cll[] = { 0x03, 0xE8, 0x00, 0x64 };
  /* Not HDR10+: other T.35 provider. */
  const uint8_t t35_other[] = { 0xB5, 0x00, 0x3F, 0x00, 0x01, 0x04, 0xAA };

  /* 1. HDR10+ message alone in its SEI NAL: NAL dropped whole. */
  n = cat(in, nal, 0, 1); /* placeholder, rebuilt below */
  size_t sn = sei(nal, 4, hdr10p, sizeof hdr10p);
  n = cat(in, nal, sn, 1);
  /* plus a VCL NAL after, which must survive */
  const uint8_t vcl[] = { 0x26, 0x01, 0xC9, 0xDD };
  size_t v2 = cat(in + n, vcl, sizeof vcl, 4);
  n += v2;
  assert(!nv_hdr10plus_annexb(in, n, out, sizeof out, &w));
  e = cat(exp, vcl, sizeof vcl, 1);
  assert(w == e && !memcmp(out, exp, e));

  /* 2. HDR10+ and static HDR10 in the SAME SEI NAL: only the HDR10+ message
   *    goes; the rebuilt NAL keeps the static message with a correct header. */
  uint8_t two[256];
  size_t tn = 0;
  two[tn++] = 39 << 1; two[tn++] = 1;
  two[tn++] = 4; two[tn++] = (uint8_t)sizeof hdr10p;
  memcpy(two + tn, hdr10p, sizeof hdr10p); tn += sizeof hdr10p;
  two[tn++] = 137; two[tn++] = (uint8_t)sizeof mdm;
  memcpy(two + tn, mdm, sizeof mdm); tn += sizeof mdm;
  two[tn++] = 0x80;
  n = cat(in, two, tn, 1);
  assert(!nv_hdr10plus_annexb(in, n, out, sizeof out, &w));
  uint8_t want[256];
  size_t wn = 0;
  want[wn++] = 39 << 1; want[wn++] = 1;
  want[wn++] = 137; want[wn++] = (uint8_t)sizeof mdm;
  memcpy(want + wn, mdm, sizeof mdm); wn += sizeof mdm;
  want[wn++] = 0x80;
  e = cat(exp, want, wn, 1);
  assert(w == e && !memcmp(out, exp, e));

  /* 3. SEI NAL with only non-HDR10+ content: untouched (payload bytes kept,
   *    start code normalized to 4 bytes like the rest of the packet). */
  tn = 0;
  two[tn++] = 39 << 1; two[tn++] = 1;
  two[tn++] = 4; two[tn++] = (uint8_t)sizeof t35_other;
  memcpy(two + tn, t35_other, sizeof t35_other); tn += sizeof t35_other;
  two[tn++] = 144; two[tn++] = (uint8_t)sizeof cll;
  memcpy(two + tn, cll, sizeof cll); tn += sizeof cll;
  two[tn++] = 0x80;
  n = cat(in, two, tn, 1);
  assert(!nv_hdr10plus_annexb(in, n, out, sizeof out, &w));
  e = cat(exp, two, tn, 1);
  assert(w == e && !memcmp(out, exp, e));

  /* 4. 3-byte start codes and VPS/SPS/VCL: preserved, code becomes 4 bytes. */
  const uint8_t vps[] = { 0x40, 0x01, 0x11, 0x22 };
  n = cat(in, vps, sizeof vps, 0);          /* 3-byte code */
  sn = sei(nal, 4, hdr10p, sizeof hdr10p);
  n += cat(in + n, nal, sn, 3);              /* dropped */
  n += cat(in + n, vcl, sizeof vcl, 3);
  assert(!nv_hdr10plus_annexb(in, n, out, sizeof out, &w));
  e = cat(exp, vps, sizeof vps, 1);
  e += cat(exp + e, vcl, sizeof vcl, 4);
  assert(w == e && !memcmp(out, exp, e));

  /* 5. Suffix SEI (type 40) also stripped; oversized payload coding works. */
  uint8_t big[512];
  const uint8_t bigpayload[300] = { 0xB5, 0x00, 0x3C };   /* rest zeros */
  uint8_t sn2[512];
  size_t bn = 0;
  sn2[bn++] = 40 << 1; sn2[bn++] = 1;
  sn2[bn++] = 4;                        /* payload_type 4 */
  sn2[bn++] = 255; sn2[bn++] = (uint8_t)(sizeof bigpayload - 255);
  memcpy(sn2 + bn, bigpayload, sizeof bigpayload); bn += sizeof bigpayload;
  sn2[bn++] = 0x80;
  n = cat(in, vcl, sizeof vcl, 1);
  n += cat(in + n, sn2, bn, 4);
  n += cat(in + n, vps, sizeof vps, 4);
  (void)big;
  assert(!nv_hdr10plus_annexb(in, n, out, sizeof out, &w));
  e = cat(exp, vcl, sizeof vcl, 1);
  e += cat(exp + e, vps, sizeof vps, 4);
  assert(w == e && !memcmp(out, exp, e));

  /* 6. Malformed: SEI message claiming more bytes than the NAL has fails
   *    closed (whole packet rejected, caller keeps the original). */
  tn = 0;
  two[tn++] = 39 << 1; two[tn++] = 1;
  two[tn++] = 4; two[tn++] = 200;            /* size 200, payload absent */
  two[tn++] = 0x80;
  n = cat(in, two, tn, 1);
  assert(nv_hdr10plus_annexb(in, n, out, sizeof out, &w) < 0 && !w);

  /* 7. Not Annex-B / no start code: rejected, nothing written. */
  const uint8_t junk[] = { 0x12, 0x34, 0x56, 0x78 };
  assert(nv_hdr10plus_annexb(junk, sizeof junk, out, sizeof out, &w) < 0 && !w);
  assert(nv_hdr10plus_annexb(NULL, 4, out, sizeof out, &w) < 0);

  /* 8. Output too small: fails closed. */
  sn = sei(nal, 4, hdr10p, sizeof hdr10p);
  n = cat(in, nal, sn, 1);
  n += cat(in + n, vcl, sizeof vcl, 4);
  assert(nv_hdr10plus_annexb(in, n, out, 4, &w) < 0);

  /* 9. Same T.35 with country_code_extension: also HDR10+. */
  const uint8_t ext[] = { 0xB5, 0xFF, 0x07, 0x00, 0x3C, 0x00, 0x01, 0x04, 0x01 };
  assert(nv_hdr10plus_t35(ext, sizeof ext));
  const uint8_t short_[] = { 0xB5, 0x00, 0x3C };
  assert(!nv_hdr10plus_t35(short_, sizeof short_));

  puts("HDR10+ strip: ST 2094-40 SEI removed, static HDR10 kept, malformed fails closed.");
  return 0;
}
