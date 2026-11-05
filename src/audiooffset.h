// Manual audio presentation offset (Settings > Audio offset): positive delays
// audio; zero is unchanged. Was in audsync.h until the C audio sync was deleted
// (06/11); the DTS/TrueHD converter and Settings still use it.
#ifndef NV_AUDIOOFFSET_H
#define NV_AUDIOOFFSET_H
#define AUDIO_OFFSET_MAX_MS 500
#define AUDIO_OFFSET_STEP_MS 20
static inline int audio_offset_ms(int ms) {
  return ms < -AUDIO_OFFSET_MAX_MS ? -AUDIO_OFFSET_MAX_MS :
         ms > AUDIO_OFFSET_MAX_MS ? AUDIO_OFFSET_MAX_MS : ms;
}
#endif
