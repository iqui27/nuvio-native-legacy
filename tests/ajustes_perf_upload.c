#include "gfx.h"
#undef glTexImage2D
extern void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
int perf_uploads;
void perf_tex_upload(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h,
                     GLint border, GLenum format, GLenum type, const GLvoid *pixels) {
  perf_uploads++;
  glTexImage2D(target, level, internal, w, h, border, format, type, pixels);
}
