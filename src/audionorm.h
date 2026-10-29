#ifndef NV_AUDIO_NORM_H
#define NV_AUDIO_NORM_H
#include <math.h>
/* Linked-channel RMS leveling, bounded to 12 dB. Silence never raises gain. */
static inline float audionorm_gain(double squares, int count, float previous, int rate) {
  if(count<=0 || rate<=0)return previous;
  double rms=sqrt(squares/count);
  float target=rms<.001?1:(float)(.15/rms);
  if(target<.25f)target=.25f;
  if(target>4)target=4;
  float seconds=(float)count/rate;
  float mix=1-expf(-seconds/(target<previous?.03f:1.0f));
  return previous+(target-previous)*mix;
}
static inline float audionorm_sample(float sample,float gain) {
  float out=sample*gain;
  return out>1?1:out< -1?-1:out;
}
#endif
