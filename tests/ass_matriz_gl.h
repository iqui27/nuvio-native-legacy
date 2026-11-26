/* GL de mentira para tests/ass_matriz.c (incluido antes de tudo, -include):
 * as texturas vivem na memoria do teste, para ler o que cada imagem subiu. */
#define glGenTextures    teste_glGenTextures
#define glDeleteTextures teste_glDeleteTextures
#define glBindTexture    teste_glBindTexture
#define glTexImage2D     teste_glTexImage2D
#define glTexSubImage2D  teste_glTexSubImage2D
#define glTexParameteri  teste_glTexParameteri
#define glPixelStorei    teste_glPixelStorei
#define glGetIntegerv    teste_glGetIntegerv
#define glGetError       teste_glGetError
