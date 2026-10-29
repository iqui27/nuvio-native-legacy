// Compile the actual SGSR body under GLES2, including the no-highp branch.
#include "gfx_sgsr.h"
#include "gfx_upscale.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static GLuint shader(GLenum type, const char *source) {
  GLuint s=glCreateShader(type);glShaderSource(s,1,&source,NULL);glCompileShader(s);
  GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(!ok) {char log[2048];glGetShaderInfoLog(s,sizeof log,NULL,log);puts(log);}
  assert(ok);return s;
}
int main(void) {
  EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint n;EGLConfig c;
  const EGLint ca[]={EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_NONE};
  const EGLint sa[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE},xa[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
  assert(eglInitialize(d,NULL,NULL)&&eglChooseConfig(d,ca,&c,1,&n)&&n);
  EGLSurface surface=eglCreatePbufferSurface(d,c,sa);
  EGLContext context=eglCreateContext(d,c,EGL_NO_CONTEXT,xa);
  assert(eglMakeCurrent(d,surface,surface,context));
  const char *bodies[]={NV_SGSR_SHADER,NV_SHARP_BILINEAR_SHADER,NV_EASU_SHADER,NV_LANCZOS_SHADER,NV_RCAS_SHADER};
  const char *names[]={"SGSR","Sharp-Bilinear","EASU","Lanczos","RCAS"};
  for(int kind=0;kind<5;kind++) for(int low=0;low<2;low++) {
    const char *parts[]={low?"#define NV_SGSR_MEDIUMP\n":"",
      "precision mediump float;\n#if defined(GL_FRAGMENT_PRECISION_HIGH) && !defined(NV_SGSR_MEDIUMP)\n"
      "varying highp vec2 vUv; uniform highp vec2 uPar;\n#else\n"
      "varying mediump vec2 vUv; uniform mediump vec2 uPar;\n#endif\n"
      "uniform sampler2D uTex; uniform float uFoco;\n",
      bodies[kind]};
    GLuint f=glCreateShader(GL_FRAGMENT_SHADER);glShaderSource(f,3,parts,NULL);glCompileShader(f);
    GLint ok;glGetShaderiv(f,GL_COMPILE_STATUS,&ok);
    if(!ok) {char log[2048];glGetShaderInfoLog(f,sizeof log,NULL,log);puts(log);}assert(ok);
    const char *vs=low?"attribute vec2 p; varying mediump vec2 vUv; void main(){vUv=p;gl_Position=vec4(p,0.,1.);}":
      "attribute vec2 p; varying highp vec2 vUv; void main(){vUv=p;gl_Position=vec4(p,0.,1.);}";
    GLuint p=glCreateProgram();glAttachShader(p,shader(GL_VERTEX_SHADER,vs));glAttachShader(p,f);
    glLinkProgram(p);glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);
    glUseProgram(p);
    GLuint tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
    const unsigned char color[]={51,102,153,85};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,color);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glUniform1i(glGetUniformLocation(p,"uTex"),0);
    glUniform1f(glGetUniformLocation(p,"uFoco"),1);
    glUniform2f(glGetUniformLocation(p,"uPar"),1,1);
    const GLfloat quad[]={-1,-1,1,-1,-1,1,1,1};
    GLint pos=glGetAttribLocation(p,"p");glEnableVertexAttribArray(pos);
    glVertexAttribPointer(pos,2,GL_FLOAT,GL_FALSE,0,quad);
    glViewport(0,0,16,16);glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    unsigned char pixel[4];glReadPixels(12,12,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    for(int channel=0;channel<4;channel++) assert(abs(pixel[channel]-color[channel])<=1);
    assert(glGetError()==GL_NO_ERROR);
    printf("PASS GLES2 %s %s compile/link and RGBA output (%s)\n",names[kind],low?"mediump":"highp",glGetString(GL_RENDERER));
  }
  return 0;
}
