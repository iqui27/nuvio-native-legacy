#ifndef NV_DTS_TV_H
#define NV_DTS_TV_H
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
