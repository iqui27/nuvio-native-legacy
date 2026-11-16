#ifndef NV_DTS_TV_H
#define NV_DTS_TV_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Does this TV decode DTS itself? Decided from the starfish release major
 * (/etc/starfish-release, "release 11.2" -> 11), never from the TV's answer:
 * newer LGs play the DTS track SILENT instead of raising errorCode 200 (#285,
 * webOS 26), so waiting for the error never converts anything.
 * LG dropped DTS in 2020-2022, brought it back on some 2023/2024 models
 * (OLED, QNED85 and up) and dropped it again in 2025, and gives no reliable
 * way to detect it (a webOS 23 update has been reported to leave DTS silent).
 * So every 2020+ LG (release major >= 5, including marketing-numbered ones
 * such as 26) converts DTS before playing, even 2023/24 models that have DTS.
 * Only the C9 and older (release < 5), or an unknown release (<= 0), keep the
 * native path and the errorCode 200 fallback. */
#define NV_DTS_TV_PRIMEIRA_SEM_DTS 5
static inline int nv_dts_tv_decodifica(int release_major) {
  return release_major <= 0 || release_major < NV_DTS_TV_PRIMEIRA_SEM_DTS;
}
/* What the TV itself declares:
 * configd's tv.model.edidType lists the audio formats the set decodes, e.g.
 * "TrueHD+dts" on a model with DTS (or with DTS Restore Plus), "TrueHD"
 * without. 1 = mentions DTS, 0 = answered without DTS, -1 = no such key in
 * the reply (an error, a refused call, nothing yet). */
static inline int nv_dts_edid(const char *json) {
  const char *k = json ? strstr(json, "\"tv.model.edidType\"") : NULL;
  char v[48] = "";
  if (!k) return -1;
  k += 19;
  while (*k == ' ' || *k == '\t') k++;
  if (*k != ':') return -1;
  k++;
  while (*k == ' ' || *k == '\t') k++;
  if (*k++ != '"') return -1;
  size_t n = 0;
  while (k[n] && k[n] != '"' && n < sizeof v - 1) {
    char c = k[n];
    v[n++] = (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  }
  v[n] = 0;
  return strstr(v, "dts") != NULL;
}
/* For the log: the edidType value alone, letters/digits/+-_ only (a raw JSON
 * line is hidden whole by the log export). 0 = no string value. */
static inline int nv_dts_edid_valor(const char *json, char *dst, size_t n) {
  const char *k = json ? strstr(json, "\"tv.model.edidType\"") : NULL;
  size_t i = 0;
  if (!dst || !n) return 0;
  dst[0] = 0;
  if (!k) return 0;
  k += 19;
  while (*k == ' ' || *k == '\t') k++;
  if (*k++ != ':') return 0;
  while (*k == ' ' || *k == '\t') k++;
  if (*k++ != '"') return 0;
  for (; *k && *k != '"' && i + 1 < n; k++) {
    char c = *k;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '_')
      dst[i++] = c;
  }
  dst[i] = 0;
  return 1;
}
/* errorCode of a refused call (0 when there is none). */
static inline int nv_dts_edid_erro(const char *json) {
  const char *k = json ? strstr(json, "\"errorCode\"") : NULL;
  if (!k) return 0;
  k += 11;
  while (*k == ' ' || *k == ':' || *k == '\t') k++;
  return atoi(k);
}
/* Convert now (before any native play) a DTS track on a TV with no DTS. */
static inline int nv_dts_converter_ja(int release_major, int faixa_dts,
                                      int conversao_disponivel, int ja_tentou) {
  return faixa_dts && conversao_disponivel && !ja_tentou &&
         !nv_dts_tv_decodifica(release_major);
}
/* Silent playback is certain: no DTS on this TV and no conversion to run. */
static inline int nv_dts_sem_som(int release_major, int faixa_dts,
                                 int conversao_disponivel) {
  return faixa_dts && !conversao_disponivel && !nv_dts_tv_decodifica(release_major);
}
#endif
