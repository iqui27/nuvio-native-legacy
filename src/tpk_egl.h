#ifndef NV_TPK_EGL_H
#define NV_TPK_EGL_H
// EGL do .tpk por dlopen, e nao por -lEGL. Nenhum rootstrap do Tizen (4.0 a
// 10) traz libEGL: EGL nao e API nativa publica la (a Samsung manda usar Evas
// GL). Na TV ela existe (o DALi e o Evas GL a usam), mas um DT_NEEDED num nome
// que o aparelho nao tenha derruba a .so INTEIRA antes de qualquer log. Por
// dlopen, falta de EGL vira uma linha no nuvio.log.
#include <EGL/egl.h>

typedef struct {
  EGLDisplay (*GetCurrentDisplay)(void);
  EGLSurface (*GetCurrentSurface)(EGLint);
  EGLContext (*GetCurrentContext)(void);
  EGLBoolean (*MakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
  EGLint     (*GetError)(void);
  EGLBoolean (*QueryContext)(EGLDisplay, EGLContext, EGLint, EGLint *);
  EGLBoolean (*QuerySurface)(EGLDisplay, EGLSurface, EGLint, EGLint *);
  EGLBoolean (*ChooseConfig)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
  EGLBoolean (*GetConfigAttrib)(EGLDisplay, EGLConfig, EGLint, EGLint *);
  EGLDisplay (*GetDisplay)(EGLNativeDisplayType);
  EGLBoolean (*Initialize)(EGLDisplay, EGLint *, EGLint *);
  EGLBoolean (*BindAPI)(EGLenum);
  EGLSurface (*CreateWindowSurface)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *);
  EGLContext (*CreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
  EGLBoolean (*SwapInterval)(EGLDisplay, EGLint);
  EGLBoolean (*SwapBuffers)(EGLDisplay, EGLSurface);
  // Para as funcoes de GLES3/extensao que a libGLESv2 do link nao declara
  // (glInvalidateFramebuffer, glDiscardFramebufferEXT): gpunivel.c.
  void      *(*GetProcAddress)(const char *);
} TpkEgl;
extern TpkEgl tpkEgl;
int tpk_egl_carregar(void);   // 0 = ok

#define eglGetCurrentDisplay  tpkEgl.GetCurrentDisplay
#define eglGetCurrentSurface  tpkEgl.GetCurrentSurface
#define eglGetCurrentContext  tpkEgl.GetCurrentContext
#define eglMakeCurrent        tpkEgl.MakeCurrent
#define eglGetError           tpkEgl.GetError
#define eglQueryContext       tpkEgl.QueryContext
#define eglQuerySurface       tpkEgl.QuerySurface
#define eglChooseConfig       tpkEgl.ChooseConfig
#define eglGetConfigAttrib    tpkEgl.GetConfigAttrib
#define eglGetDisplay         tpkEgl.GetDisplay
#define eglInitialize         tpkEgl.Initialize
#define eglBindAPI            tpkEgl.BindAPI
#define eglCreateWindowSurface tpkEgl.CreateWindowSurface
#define eglCreateContext      tpkEgl.CreateContext
#define eglSwapInterval       tpkEgl.SwapInterval
#define eglSwapBuffers        tpkEgl.SwapBuffers
#endif
