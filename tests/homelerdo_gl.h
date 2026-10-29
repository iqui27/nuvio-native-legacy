// tests/homelerdo.sh: count the GL calls that cost GPU time outside gfx_fill.
#include "gl_compat.h"
extern int hl_gl[8];   // read, fbo bind, copy, clear, draw, mipmap
#undef glBindFramebuffer
#undef glGenerateMipmap
#define glReadPixels(...) (hl_gl[0]++, glReadPixels(__VA_ARGS__))
#define glBindFramebuffer(...) (hl_gl[1]++, glBindFramebufferEXT(__VA_ARGS__))
#define glCopyTexSubImage2D(...) (hl_gl[2]++, glCopyTexSubImage2D(__VA_ARGS__))
#define glClear(...) (hl_gl[3]++, glClear(__VA_ARGS__))
#define glDrawArrays(...) (hl_gl[4]++, glDrawArrays(__VA_ARGS__))
#define glGenerateMipmap(...) (hl_gl[5]++, glGenerateMipmapEXT(__VA_ARGS__))
