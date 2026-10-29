// Ponte entre o GLES2 da TV e o OpenGL do Mac.
//
// Existe para poder ver a UI no computador em vez de esperar o ciclo de
// empacotar, enviar e instalar na TV a cada ajuste de layout. O que roda no Mac
// e o MESMO codigo de desenho; muda so como as funcoes se chamam e o dialeto do
// shader. A TV continua sendo a palavra final — cor, escala e desempenho so
// valem medidos la.
#ifndef NV_GL_COMPAT_H
#define NV_GL_COMPAT_H

// NV_GLES_NO_MAC: o teste tests/fundo_assado.sh compila o nucleo no Mac
// contra o GLES2 do ANGLE (o dialeto e as regras de FBO da TV), nao o GL 2.1.
#ifdef __APPLE__
  #include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && TARGET_OS_IPHONE
  // iOS: the real GLES2, same dialect as the TVs (deprecated by Apple, still shipped).
  #define GLES_SILENCE_DEPRECATION 1
  #include <OpenGLES/ES2/gl.h>
  #include <OpenGLES/ES2/glext.h>
  #define NV_GLSL_PREFIXO "precision mediump float;\n"
  // There is no framebuffer 0 on iOS: the screen is the FBO SDL's GL view owns
  // (SDL_SysWMinfo.info.uikit.framebuffer, read by main.c after the context).
  // Every "back to the screen" bind in the core goes through here.
  extern unsigned nv_gl_fb_tela;
  static inline void nvBindFramebufferTela(GLenum alvo, GLuint fb) {
    glBindFramebuffer(alvo, fb ? fb : (GLuint)nv_gl_fb_tela);
  }
  #define glBindFramebuffer nvBindFramebufferTela
#elif defined(__APPLE__) && !defined(NV_GLES_NO_MAC)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl.h>
  #include <OpenGL/glext.h>
  // Framebuffer de objeto e core no GLES2 e extensao no OpenGL 2.1 do Mac.
  #define glGenFramebuffers        glGenFramebuffersEXT
  #define glBindFramebuffer        glBindFramebufferEXT
  #define glFramebufferTexture2D   glFramebufferTexture2DEXT
  #define glCheckFramebufferStatus glCheckFramebufferStatusEXT
  #define glDeleteFramebuffers     glDeleteFramebuffersEXT
  #define glGenerateMipmap         glGenerateMipmapEXT
  #ifndef GL_FRAMEBUFFER
    #define GL_FRAMEBUFFER           GL_FRAMEBUFFER_EXT
  #endif
  #ifndef GL_COLOR_ATTACHMENT0
    #define GL_COLOR_ATTACHMENT0     GL_COLOR_ATTACHMENT0_EXT
  #endif
  #ifndef GL_FRAMEBUFFER_COMPLETE
    #define GL_FRAMEBUFFER_COMPLETE  GL_FRAMEBUFFER_COMPLETE_EXT
  #endif
  // GLSL 1.20 nao tem qualificadores de precisao; declara-los quebra a
  // compilacao, entao viram nada.
  #define NV_GLSL_PREFIXO \
    "#version 120\n" \
    "#define lowp\n#define mediump\n#define highp\n"
#else
  #include <GLES2/gl2.h>
  #define NV_GLSL_PREFIXO "precision mediump float;\n"
#endif

#include "perftv.h"
// Count submitted pixel uploads (including text/mip levels), not allocations.
// Inline wrappers leave only a flag branch when measurement is off.
static inline void nvPerfTexImage2D(GLenum target, GLint level, GLint internal,
    GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void *pixels) {
  if (perftv_enabled && pixels) perftv_uploads++;
  glTexImage2D(target, level, internal, w, h, border, format, type, pixels);
}
static inline void nvPerfTexSubImage2D(GLenum target, GLint level, GLint x, GLint y,
    GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels) {
  if (perftv_enabled && pixels) perftv_uploads++;
  glTexSubImage2D(target, level, x, y, w, h, format, type, pixels);
}
#define glTexImage2D nvPerfTexImage2D
#define glTexSubImage2D nvPerfTexSubImage2D

#endif
