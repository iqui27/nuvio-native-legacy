#ifndef NV_PLAYER_PACING_H
#define NV_PLAYER_PACING_H

/* A bare hardware-video frame draws only its transparent hole. Count actual
 * draws so every overlay, including subtitles and global islands, stays live. */
static inline int player_ui_frame(int open, int rects, int others) {
  /* gfx_furo itself counts as one rect and one other (scissored clear). */
  return open && (rects > 1 || others > (rects == 1 ? 1 : 0));
}

/* Native swaps can follow the video plane's clock. With UI visible, use an
 * unpaced swap and bound the full iteration to 60 Hz, including swap time. */
static inline unsigned player_frame_wait(int ui, unsigned elapsed) {
  return ui && elapsed < 16u ? 16u - elapsed : 0u;
}
#endif
