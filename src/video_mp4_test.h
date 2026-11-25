#ifndef NV_VIDEO_MP4_TEST_H
#define NV_VIDEO_MP4_TEST_H
/* A working native MP4 must never be interrupted by this compatibility test.
 * Retry only the observed startup demux refusal, once per source opening. */
static inline int nv_mp4_bd_retry(int enabled,int code,int mp4,int started,
                                  int live,int converter,int tried,int recovering) {
  return enabled && code==204 && mp4 && !started && !live && !converter && !tried && !recovering;
}
#endif
