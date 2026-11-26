// Log 14 (Fate OP, 23/11/2026): the UI ran at 7 fps because a per-frame call
// on the GL thread (assrender_fonte_local_ativa) took assTrava, which the ASS
// worker holds for the whole libass render (150-430 ms per frame there).
//  [1] a "worker" holds assTrava for 300 ms; meanwhile every function the GL
//      thread calls per frame must return in under 5 ms;
//  [2] the lock-free copy still answers right: the registered local font is
//      active, another path or none is not, and a change is seen at once;
//  [3] the GL-side lock-wait counter counts a real wait on assFilaTrava.
// Fails on a6a657a ([1]: assrender_fonte_local_ativa waits ~300 ms).
// src/assrender.c is included (its statics), with the fake GL of ass_matriz.
#include "../src/assrender.c"
#include <stdio.h>

void teste_glGenTextures(GLsizei n, GLuint *t) { static GLuint p = 1; int i; for (i = 0; i < n; i++) t[i] = p++; }
void teste_glDeleteTextures(GLsizei n, const GLuint *t) { (void)n; (void)t; }
void teste_glBindTexture(GLenum a, GLuint t) { (void)a; (void)t; }
void teste_glPixelStorei(GLenum a, GLint v) { (void)a; (void)v; }
void teste_glGetIntegerv(GLenum a, GLint *v) { (void)a; *v = 4096; }
GLenum teste_glGetError(void) { return 0; }
void teste_glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
void teste_glTexImage2D(GLenum a, GLint b, GLint c, GLsizei w, GLsizei h, GLint d, GLenum e, GLenum f, const void *p) {
  (void)a; (void)b; (void)c; (void)w; (void)h; (void)d; (void)e; (void)f; (void)p; }
void teste_glTexSubImage2D(GLenum a, GLint b, GLint x, GLint y, GLsizei w, GLsizei h, GLenum e, GLenum f, const void *p) {
  (void)a; (void)b; (void)x; (void)y; (void)w; (void)h; (void)e; (void)f; (void)p; }
float gfx_tex_aspect_atual;
void gfx_tex_esquecer(GLuint tex) { (void)tex; }
int gfx_grupo_comecar(GfxGrupo *g, GfxRect caixa, float alpha) { (void)g; (void)caixa; (void)alpha; return 0; }
void gfx_grupo_terminar(GfxGrupo *g, float alpha) { (void)g; (void)alpha; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float parx, float pary, float raio,
              float cr, float cg, float cb, float ca) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)parx; (void)pary; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca; }

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }
static double agora_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }

static pthread_mutex_t *segurarQual;
static volatile int segurando;
static void *segurar(void *u) {   /* the "worker": one long render under the lock */
  pthread_mutex_lock(segurarQual);
  __atomic_store_n(&segurando, 1, __ATOMIC_RELEASE);
  usleep((useconds_t)(intptr_t)u * 1000);
  pthread_mutex_unlock(segurarQual);
  return NULL;
}
static pthread_t segurar_comecar(pthread_mutex_t *m, int ms) {
  pthread_t t;
  segurarQual = m; __atomic_store_n(&segurando, 0, __ATOMIC_RELEASE);
  pthread_create(&t, NULL, segurar, (void *)(intptr_t)ms);
  while (!__atomic_load_n(&segurando, __ATOMIC_ACQUIRE)) usleep(100);
  return t;
}

#define MEDIR(nome, chamada) do { double t0 = agora_ms(); chamada; double d = agora_ms() - t0; \
  if (d > pior) pior = d; if (d >= 5.0) { printf("      %s took %.1f ms\n", nome, d); lentas++; } } while (0)

int main(int argc, char **argv) {
  const char *fonte = argc > 1 ? argv[1] : "deploy/app/fonts/JetBrainsMonoNL-Regular.ttf";
  const char *outra = argc > 2 ? argv[2] : "deploy/app/fonts/JetBrainsMonoNL-SemiBold.ttf";
  PlainAssStyle st;
  memset(&st, 0, sizeof st);
  snprintf(st.font, sizeof st.font, "Inter"); st.size = 40; st.rgb = 0xffffff;

  /* A track on screen, as during playback (no real worker: the test is it). */
  assLib = ass_library_init();
  ok(assLib != NULL, "libass ready");
  ok(assrender_fonte_local_transacao(fonte, NULL, NULL), "[2] local font registered");
  ok(assrender_fonte_local_ativa(fonte) == 1, "[2] registered font is active");
  ok(assrender_fonte_local_ativa(outra) == 0, "[2] another path is not");
  ok(assrender_fonte_local_ativa("") == 0 && assrender_fonte_local_ativa(NULL) == 0, "[2] no path is not");
  __atomic_store_n(&assGeracao, 7u, __ATOMIC_RELEASE);
  __atomic_store_n(&assTrackGeracao, 7u, __ATOMIC_RELEASE);
  __atomic_store_n(&assTrackAtivo, 1, __ATOMIC_RELEASE);
  __atomic_store_n(&assWorkerCriado, 1, __ATOMIC_RELEASE);

  /* [1] 300 ms render under assTrava; the GL thread keeps drawing frames. */
  { pthread_t t = segurar_comecar(&assTrava, 300);
    double ini = agora_ms(), pior = 0.0, pos = 10.0;
    int lentas = 0, quadros = 0;
    while (agora_ms() - ini < 250.0) {
      MEDIR("assrender_fonte_local_ativa", (void)assrender_fonte_local_ativa(fonte));
      MEDIR("assrender_ativo", (void)assrender_ativo());
      MEDIR("assrender_texto_simples", (void)assrender_texto_simples());
      MEDIR("assrender_definir_texto_estilo", assrender_definir_texto_estilo(&st));
      MEDIR("assrender_pixels_por_unidade", assrender_pixels_por_unidade(2.0f));
      MEDIR("assrender_definir_layout", assrender_definir_layout(0, 0, 1920, 1080, 1920, 1080, 1.0));
      MEDIR("assrender_definir_cor", assrender_definir_cor(0, 255, 255, 255));
      MEDIR("assrender_definir_fps", assrender_definir_fps(23.976));
      MEDIR("assrender_definir_cor_video", assrender_definir_cor_video(1, 0, 0, 1080));
      MEDIR("assrender_aplicar_invalidacao", assrender_aplicar_invalidacao());
      MEDIR("assrender_desenhar", (void)assrender_desenhar(pos, 0, 1.0f, 0, 0, 1920, 1080));
      pos += 1.0 / 60.0; quadros++;
      usleep(2000);
    }
    pthread_join(t, NULL);
    printf("      %d UI frames while the render held the lock, slowest call %.2f ms\n", quadros, pior);
    ok(lentas == 0 && pior < 5.0, "[1] every per-frame GL call returns in < 5 ms while assTrava is held 300 ms");
  }

  /* [2] a change made under assTrava is seen by the copy at once. */
  ok(assrender_fonte_local_transacao(outra, NULL, NULL), "[2] local font changed");
  ok(assrender_fonte_local_ativa(outra) == 1 && assrender_fonte_local_ativa(fonte) == 0, "[2] the copy follows the change");
  ok(assrender_fonte_local_transacao("", NULL, NULL), "[2] local font cleared");
  ok(assrender_fonte_local_ativa(outra) == 0, "[2] cleared: none active");

  /* [3] the lock-wait counter: a real wait on assFilaTrava is counted. */
  { long antes = __atomic_load_n(&glTravaEsperas, __ATOMIC_RELAXED);
    pthread_t t = segurar_comecar(&assFilaTrava, 30);
    assrender_definir_cor(0, 1, 2, 3);
    pthread_join(t, NULL);
    ok(__atomic_load_n(&glTravaEsperas, __ATOMIC_RELAXED) == antes + 1, "[3] a wait is counted");
    ok(__atomic_load_n(&glTravaMaxUs, __ATOMIC_RELAXED) >= 20000, "[3] its length is kept (>= 20 ms)"); }

  printf(falhas ? "ass_trava_gl: %d FALHA(S)\n" : "ass_trava_gl: ok\n", falhas);
  return falhas ? 1 : 0;
}
