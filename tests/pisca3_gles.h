// tests/pisca3_shot.sh: force-included in every unit. GLES leaves a texture
// made with glTexImage2D(NULL) UNDEFINED (Mali: whatever was in that memory);
// desktop GL zeroes it. The wrapper (pisca3_shot.c) fills it with garbage.
#define glTexImage2D(t, l, i, w, h, b, f, ty, p) pisca3_teximage(t, l, i, w, h, b, f, ty, p)
#define glTexSubImage2D(t, l, x, y, w, h, f, ty, p) pisca3_texsubimage(t, l, x, y, w, h, f, ty, p)
#define glGenerateMipmap(t) pisca3_mipmap(t)
#define glDrawArrays(m, f, n) pisca3_drawarrays(m, f, n)
#define glGetError pisca3_error
// One clock: home.c times the hero with SDL_GetTicks and draws with `agora`.
// Two clocks drifting apart rotated the hero carousel every slide.
#define SDL_GetTicks pisca3_ticks
