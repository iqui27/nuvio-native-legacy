// libnvspike.so — spike de fase 0: 3 sondas chamadas via DllImport do app .NET.
// nv_ping: prova que a .so carregou e o ABI (calling convention, ints) bate.
// nv_thread_test: prova que pthread dentro da .so funciona (glibc/musl da
//   TV, nao do host .NET).
// nv_draw: mesmo caminho de src/gfx.c (VBO com quad, GL_TRIANGLE_STRIP) —
//   se GLView do NUI aceita a .so desenhando nele, o port do renderer do
//   Nuvio e viavel.
#include <GLES2/gl2.h>
#include <pthread.h>

// Headers do rootstrap Tizen 10, mas LINK contra o Tizen 9 (glibc 2.30):
// na glibc >= 2.34 pthread_create mudou para a libc com versao GLIBC_2.34 e
// TV com glibc mais velha recusaria a .so inteira (ver tools/tizen-tpk-spike.sh).
#include <stdio.h>
#include <string.h>

int nv_ping(void) { return 42; }

static void *thread_fn(void *arg) {
  int *ok = (int *)arg;
  *ok = 1;
  return NULL;
}

int nv_thread_test(void) {
  pthread_t t;
  int ok = 0;
  if (pthread_create(&t, NULL, thread_fn, &ok) != 0) return 0;
  pthread_join(t, NULL);
  return ok;
}

// Um conjunto de programa/VBO por contexto GL (0 = GLView, 1 = GLWindow):
// ids GL nao valem entre contextos, e os dois testes rodam ao mesmo tempo.
#define NV_CTX 2
static GLuint progs[NV_CTX], vbos[NV_CTX];
static GLint uCores[NV_CTX];
static int quadros[NV_CTX];

static const char *VS =
  "attribute vec2 aPos;\n"
  "void main(){ gl_Position = vec4(aPos-0.5, 0.0, 1.0); }\n";
static const char *FS =
  "precision mediump float;\n"
  "uniform vec4 uCor;\n"
  "void main(){ gl_FragColor = uCor; }\n";

static GLuint compila(GLenum tipo, const char *src) {
  GLuint s = glCreateShader(tipo);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  return s;
}

static int nv_draw_init(int c) {
  GLuint vs = compila(GL_VERTEX_SHADER, VS);
  GLuint fs = compila(GL_FRAGMENT_SHADER, FS);
  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glBindAttribLocation(p, 0, "aPos");
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) return 0;
  progs[c] = p;
  uCores[c] = glGetUniformLocation(p, "uCor");

  static const GLfloat quad[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
  glGenBuffers(1, &vbos[c]);
  glBindBuffer(GL_ARRAY_BUFFER, vbos[c]);
  glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
  return 1;
}

// c: contexto (0/1). w,h: viewport. t: segundos, anima a cor. Devolve
// glGetError() do quadro, ou -1 se o shader nao linkou.
int nv_draw(int c, int w, int h, float t) {
  if (c < 0 || c >= NV_CTX) return -2;
  if (progs[c] == 0 && !nv_draw_init(c)) return -1;

  glViewport(0, 0, w, h);
  // onda triangular em vez de sin(): sin puxaria a libm (sem -lm a .so nem
  // carrega) e mais uma versao de simbolo para casar com a TV.
  float f = t * 0.5f;
  f -= (float)(int)f;
  float r = f < 0.5f ? 2.0f * f : 2.0f - 2.0f * f;
  glClearColor(r, 0.2f, 0.6f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(progs[c]);
  glBindBuffer(GL_ARRAY_BUFFER, vbos[c]);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);
  glUniform4f(uCores[c], 1.0f, 1.0f, 1.0f, 1.0f);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisableVertexAttribArray(0);
  quadros[c]++;
  return (int)glGetError();
}

int nv_quadros(int c) { return (c >= 0 && c < NV_CTX) ? quadros[c] : -1; }
