/* GL contado, sem janela: o teste percorre gfx_icone/gfx_rect reais. */
#include "gl_compat.h"
#define glGenTextures teste_glGenTextures
#define glDeleteTextures teste_glDeleteTextures
#define glBindTexture teste_glBindTexture
#define glTexImage2D teste_glTexImage2D
#define glTexParameteri teste_glTexParameteri
#define glPixelStorei teste_glPixelStorei
#define glDrawArrays teste_glDrawArrays
#define glUseProgram(...) ((void)0)
#define glUniform1f(...) ((void)0)
#define glUniform2f(...) ((void)0)
#define glUniform3f(...) ((void)0)
#define glUniform4f teste_glUniform4f
#define glUniform3fv(...) ((void)0)
#define glUniform4fv(...) ((void)0)
#define glActiveTexture(...) ((void)0)
#define glEnable(...) ((void)0)
#define glDisable(...) ((void)0)
#define glClearColor(...) ((void)0)
#define glClear(...) ((void)0)
#define glDeleteProgram(...) ((void)0)
/* Sem contexto GL: vidLiberar apaga FBOs do vidro junto com as texturas;
   gl_compat.h remapeia para o sufixo EXT no Mac, entao o coto tem de cobrir
   os dois nomes. Nao contam: a afirmacao do icone so mede texturas. */
#define glDeleteFramebuffers(...) ((void)0)
#define glDeleteFramebuffersEXT(...) ((void)0)
#define glGenFramebuffers(...) ((void)0)
#define glGenFramebuffersEXT(...) ((void)0)
#define glBindFramebuffer(...) ((void)0)
#define glBindFramebufferEXT(...) ((void)0)
#define glFramebufferTexture2D(...) ((void)0)
#define glFramebufferTexture2DEXT(...) ((void)0)
#define glCheckFramebufferStatus(...) 0
#define glCheckFramebufferStatusEXT(...) 0
#define glGenerateMipmap(...) ((void)0)
#define glGenerateMipmapEXT(...) ((void)0)
void teste_glGenTextures(GLsizei n, GLuint *t);
void teste_glDeleteTextures(GLsizei n, const GLuint *t);
void teste_glBindTexture(GLenum target, GLuint t);
void teste_glTexImage2D(GLenum target, GLint level, GLint internalformat,
                      GLsizei w, GLsizei h, GLint border, GLenum format,
                      GLenum type, const void *pixels);
void teste_glTexParameteri(GLenum target, GLenum pname, GLint param);
void teste_glPixelStorei(GLenum pname, GLint param);
void teste_glDrawArrays(GLenum mode, GLint first, GLsizei count);
void teste_glUniform4f(GLint location, GLfloat a, GLfloat b, GLfloat c, GLfloat d);
