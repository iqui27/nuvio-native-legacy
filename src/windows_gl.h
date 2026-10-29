/* OpenGL 2 entry points on Windows are resolved through SDL. */
#ifndef NV_WINDOWS_GL_H
#define NV_WINDOWS_GL_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
static inline PFNGLACTIVETEXTUREPROC nv_glActiveTexture(void) { return (PFNGLACTIVETEXTUREPROC)SDL_GL_GetProcAddress("glActiveTexture"); }
#define glActiveTexture (nv_glActiveTexture())
static inline PFNGLATTACHSHADERPROC nv_glAttachShader(void) { return (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader"); }
#define glAttachShader (nv_glAttachShader())
static inline PFNGLBINDATTRIBLOCATIONPROC nv_glBindAttribLocation(void) { return (PFNGLBINDATTRIBLOCATIONPROC)SDL_GL_GetProcAddress("glBindAttribLocation"); }
#define glBindAttribLocation (nv_glBindAttribLocation())
static inline PFNGLBINDBUFFERPROC nv_glBindBuffer(void) { return (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer"); }
#define glBindBuffer (nv_glBindBuffer())
static inline PFNGLBINDFRAMEBUFFERPROC nv_glBindFramebuffer(void) { return (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer"); }
#define glBindFramebuffer (nv_glBindFramebuffer())
static inline PFNGLBLENDFUNCSEPARATEPROC nv_glBlendFuncSeparate(void) { return (PFNGLBLENDFUNCSEPARATEPROC)SDL_GL_GetProcAddress("glBlendFuncSeparate"); }
#define glBlendFuncSeparate (nv_glBlendFuncSeparate())
static inline PFNGLBUFFERDATAPROC nv_glBufferData(void) { return (PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData"); }
#define glBufferData (nv_glBufferData())
static inline PFNGLCHECKFRAMEBUFFERSTATUSPROC nv_glCheckFramebufferStatus(void) { return (PFNGLCHECKFRAMEBUFFERSTATUSPROC)SDL_GL_GetProcAddress("glCheckFramebufferStatus"); }
#define glCheckFramebufferStatus (nv_glCheckFramebufferStatus())
static inline PFNGLCOMPILESHADERPROC nv_glCompileShader(void) { return (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader"); }
#define glCompileShader (nv_glCompileShader())
static inline PFNGLCREATEPROGRAMPROC nv_glCreateProgram(void) { return (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram"); }
#define glCreateProgram (nv_glCreateProgram())
static inline PFNGLCREATESHADERPROC nv_glCreateShader(void) { return (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader"); }
#define glCreateShader (nv_glCreateShader())
static inline PFNGLDELETEFRAMEBUFFERSPROC nv_glDeleteFramebuffers(void) { return (PFNGLDELETEFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteFramebuffers"); }
#define glDeleteFramebuffers (nv_glDeleteFramebuffers())
static inline PFNGLDELETEPROGRAMPROC nv_glDeleteProgram(void) { return (PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress("glDeleteProgram"); }
#define glDeleteProgram (nv_glDeleteProgram())
static inline PFNGLDELETESHADERPROC nv_glDeleteShader(void) { return (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader"); }
#define glDeleteShader (nv_glDeleteShader())
static inline PFNGLENABLEVERTEXATTRIBARRAYPROC nv_glEnableVertexAttribArray(void) { return (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray"); }
#define glEnableVertexAttribArray (nv_glEnableVertexAttribArray())
static inline PFNGLFRAMEBUFFERTEXTURE2DPROC nv_glFramebufferTexture2D(void) { return (PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D"); }
#define glFramebufferTexture2D (nv_glFramebufferTexture2D())
static inline PFNGLGENBUFFERSPROC nv_glGenBuffers(void) { return (PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers"); }
#define glGenBuffers (nv_glGenBuffers())
static inline PFNGLGENFRAMEBUFFERSPROC nv_glGenFramebuffers(void) { return (PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers"); }
#define glGenFramebuffers (nv_glGenFramebuffers())
static inline PFNGLGENERATEMIPMAPPROC nv_glGenerateMipmap(void) { return (PFNGLGENERATEMIPMAPPROC)SDL_GL_GetProcAddress("glGenerateMipmap"); }
#define glGenerateMipmap (nv_glGenerateMipmap())
static inline PFNGLGETPROGRAMINFOLOGPROC nv_glGetProgramInfoLog(void) { return (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog"); }
#define glGetProgramInfoLog (nv_glGetProgramInfoLog())
static inline PFNGLGETPROGRAMIVPROC nv_glGetProgramiv(void) { return (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv"); }
#define glGetProgramiv (nv_glGetProgramiv())
static inline PFNGLGETSHADERINFOLOGPROC nv_glGetShaderInfoLog(void) { return (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog"); }
#define glGetShaderInfoLog (nv_glGetShaderInfoLog())
static inline PFNGLGETSHADERIVPROC nv_glGetShaderiv(void) { return (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv"); }
#define glGetShaderiv (nv_glGetShaderiv())
static inline PFNGLGETUNIFORMLOCATIONPROC nv_glGetUniformLocation(void) { return (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation"); }
#define glGetUniformLocation (nv_glGetUniformLocation())
static inline PFNGLLINKPROGRAMPROC nv_glLinkProgram(void) { return (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram"); }
#define glLinkProgram (nv_glLinkProgram())
static inline PFNGLSHADERSOURCEPROC nv_glShaderSource(void) { return (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource"); }
#define glShaderSource (nv_glShaderSource())
static inline PFNGLUNIFORM1FPROC nv_glUniform1f(void) { return (PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f"); }
#define glUniform1f (nv_glUniform1f())
static inline PFNGLUNIFORM1IPROC nv_glUniform1i(void) { return (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i"); }
#define glUniform1i (nv_glUniform1i())
static inline PFNGLUNIFORM2FPROC nv_glUniform2f(void) { return (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f"); }
#define glUniform2f (nv_glUniform2f())
static inline PFNGLUNIFORM3FPROC nv_glUniform3f(void) { return (PFNGLUNIFORM3FPROC)SDL_GL_GetProcAddress("glUniform3f"); }
#define glUniform3f (nv_glUniform3f())
static inline PFNGLUNIFORM3FVPROC nv_glUniform3fv(void) { return (PFNGLUNIFORM3FVPROC)SDL_GL_GetProcAddress("glUniform3fv"); }
#define glUniform3fv (nv_glUniform3fv())
static inline PFNGLUNIFORM4FPROC nv_glUniform4f(void) { return (PFNGLUNIFORM4FPROC)SDL_GL_GetProcAddress("glUniform4f"); }
#define glUniform4f (nv_glUniform4f())
static inline PFNGLUNIFORM4FVPROC nv_glUniform4fv(void) { return (PFNGLUNIFORM4FVPROC)SDL_GL_GetProcAddress("glUniform4fv"); }
#define glUniform4fv (nv_glUniform4fv())
static inline PFNGLUSEPROGRAMPROC nv_glUseProgram(void) { return (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram"); }
#define glUseProgram (nv_glUseProgram())
static inline PFNGLVERTEXATTRIBPOINTERPROC nv_glVertexAttribPointer(void) { return (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer"); }
#define glVertexAttribPointer (nv_glVertexAttribPointer())
#endif
