// Ponte entre o GLES2 da TV e o OpenGL do Mac.
//
// Existe para poder ver a UI no computador em vez de esperar o ciclo de
// empacotar, enviar e instalar na TV a cada ajuste de layout. O que roda no Mac
// e o MESMO codigo de desenho; muda so como as funcoes se chamam e o dialeto do
// shader. A TV continua sendo a palavra final — cor, escala e desempenho so
// valem medidos la.
#ifndef NV_GL_COMPAT_H
#define NV_GL_COMPAT_H

#ifdef __APPLE__
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
  #define GL_FRAMEBUFFER           GL_FRAMEBUFFER_EXT
  #define GL_COLOR_ATTACHMENT0     GL_COLOR_ATTACHMENT0_EXT
  #define GL_FRAMEBUFFER_COMPLETE  GL_FRAMEBUFFER_COMPLETE_EXT
  // GLSL 1.20 nao tem qualificadores de precisao; declara-los quebra a
  // compilacao, entao viram nada.
  #define NV_GLSL_PREFIXO \
    "#version 120\n" \
    "#define lowp\n#define mediump\n#define highp\n"
#elif defined(__linux__) && defined(NV_GL_DESKTOP)
  // O MESMO atalho do Mac, para rodar as capturas de UI num Linux de mesa
  // (Xvfb + Mesa). So entra com -DNV_GL_DESKTOP: o alvo Tizen compila em
  // Linux tambem, e ele continua no ramo GLES2 de baixo.
  #include <GL/gl.h>
  #include <GL/glext.h>
  #define glGenFramebuffers        glGenFramebuffersEXT
  #define glBindFramebuffer        glBindFramebufferEXT
  #define glFramebufferTexture2D   glFramebufferTexture2DEXT
  #define glCheckFramebufferStatus glCheckFramebufferStatusEXT
  #define glDeleteFramebuffers     glDeleteFramebuffersEXT
  #define glGenerateMipmap         glGenerateMipmapEXT
  #define GL_FRAMEBUFFER           GL_FRAMEBUFFER_EXT
  #define GL_COLOR_ATTACHMENT0     GL_COLOR_ATTACHMENT0_EXT
  #define GL_FRAMEBUFFER_COMPLETE  GL_FRAMEBUFFER_COMPLETE_EXT
  #define NV_GLSL_PREFIXO \
    "#version 120\n" \
    "#define lowp\n#define mediump\n#define highp\n"
#else
  #include <GLES2/gl2.h>
  #define NV_GLSL_PREFIXO "precision mediump float;\n"
#endif

#endif
