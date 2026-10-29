// AVPlay source crop (setVideoRoi) bookkeeping. The ROI sticks in the player
// until replaced, so any plain destination (zoom off, PiP, new title) has to
// send the whole frame back or the old crop stretches the picture.
#ifndef AVROI_H
#define AVROI_H
// 0 = nothing to send, 1 = send a crop, 2 = restore the whole frame.
static inline int avroi_pedido(int ativo, int sx, int sy, int sw, int sh,
                               int qw, int qh, int out[4]) {
  int cheio = sw <= 0 || sh <= 0 || (sx <= 0 && sy <= 0 && sw >= qw && sh >= qh);
  if (qw >= 2 && qh >= 2 && !cheio) {
    out[0] = sx; out[1] = sy; out[2] = sw; out[3] = sh;
    return 1;
  }
  if (!ativo || qw < 2 || qh < 2) return 0;
  out[0] = 0; out[1] = 0; out[2] = qw; out[3] = qh;
  return 2;
}
#endif
