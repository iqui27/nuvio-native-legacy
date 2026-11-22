// PICTURE SYNC (.tpk, owner 25/10). The TV's picture runs a little behind the
// app's own layer while playing (its picture processing), so subtitles drawn on
// time looked early (150-210 ms on the S95C); paused, the frame stands still
// and there is no difference. Picture sync delays subtitles by that much, ONLY
// while playing: one value per kind of video (frame rate, HDR) for normal
// scenes (170 ms) and one for busy scenes (150 ms), set in Subtitles > Style.
#ifndef NV_LEGIMAGEM_H
#define NV_LEGIMAGEM_H

int  legimagem_ms(void);                  // correction for the video playing now (ms; heavy scenes have their own)
// Both values for this kind of video and whether a heavy scene is on screen now.
void legimagem_valores(int *leve, int *pesada, int *agoraPesada);
void legimagem_ajustar(int deltaMs, int pesada);   // nudge (+ = subtitles later) the normal (0) or busy (1) value of this kind of video; -1 = the scene on screen
void legimagem_restaurar(void);                    // this kind of video back to the defaults
double legimagem_fps(void);               // frame rate of the video playing now (0 = unknown)

#endif
