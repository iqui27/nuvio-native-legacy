/* GL de mentira para tests/ass_pisca.c: incluido ANTES de tudo (-include), as
 * macros renomeiam os prototipos dos cabecalhos do OpenGL e as chamadas de
 * src/assrender.c para funcoes do teste, que so contam. Sem contexto GL e sem
 * janela. */
#define glGenTextures    teste_glGenTextures
#define glDeleteTextures teste_glDeleteTextures
#define glBindTexture    teste_glBindTexture
#define glTexImage2D     teste_glTexImage2D
#define glTexParameteri  teste_glTexParameteri
