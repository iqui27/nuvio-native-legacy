// The worker's per-frame shortcuts (22/11/2026), against the plain way:
//  [1] event time index: active events, the forecast windows and the next
//      start match a full walk of the track, after loads and after events go;
//  [2] rigid signature cache: the same signature, mode and position (also
//      along a \move) as the raw parse, on first use and from the cache;
//  [3] flatten of split frames: images of each pass go to layers at their
//      own scale (stacking kept where images touch), or into one grid when
//      that needs too many layers; drawn like the GPU does, the result
//      matches a straightforward float composite; one scale stays exact.
// src/assrender.c is included (its statics), with the fake GL of ass_matriz.
#include "../src/assrender.c"
#include <stdio.h>

/* ---- fake GL / gfx (nothing is drawn here) ---- */
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
static unsigned semente = 12345;
static int aleat(int n) { semente = semente * 1103515245u + 12345u; return (int)((semente >> 8) % (unsigned)n); }

#define CAB "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n\n[V4+ Styles]\n" \
  "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n" \
  "Style: Default,Arial,56,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,3,2,2,20,20,40,1\n\n" \
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"

static void tempo(char *b, long long ms) {
  sprintf(b, "%lld:%02lld:%02lld.%02lld", ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms / 10 % 100);
}

/* Brute force over the track, the way the worker walked it before. */
static void bruto(long long t, long long ate, int *cand, int *n) {
  int i; *n = 0;
  for (i = 0; i < assTrack->n_events; i++) {
    const ASS_Event *e = &assTrack->events[i];
    if (e->Start <= ate && e->Start + e->Duration > t) cand[(*n)++] = i;
  }
}

static void teste_indice(void) {
  static char doc[4 << 20];
  size_t o = strlen(CAB);
  int i, k, erros = 0, *bc = malloc(sizeof(int) * 20000);
  memcpy(doc, CAB, o);
  for (i = 0; i < 6000; i++) {
    char a[32], b[32];
    long long ini = aleat(600000), dur = aleat(10) == 0 ? 2001 + aleat(90000) : 1 + aleat(2000);
    int len = aleat(60);
    tempo(a, ini); tempo(b, ini + dur);
    o += (size_t)sprintf(doc + o, "Dialogue: %d,%s,%s,Default,,0,0,0,,{\\pos(%d,%d)}", aleat(3), a, b, aleat(1900), aleat(1000));
    for (k = 0; k < len; k++) doc[o++] = (char)('a' + aleat(26));
    doc[o++] = '\n';
  }
  doc[o] = 0;
  assLib = ass_library_init();
  assTrack = ass_read_memory(assLib, doc, o, NULL);
  ok(assTrack && assTrack->n_events == 6000, "[1] track of 6000 events (10 % longer than 2 s)");
  for (k = 0; k < 2; k++) {
    int r;
    __atomic_add_fetch(&assTrackMut, 1, __ATOMIC_RELEASE);
    ok(ass_idx_garantir(), k ? "[1] index rebuilt after events left" : "[1] index built");
    for (r = 0; r < 3000; r++) {
      long long t = aleat(610000) - 5000, ate = t + (aleat(2) ? 1000 : 0);
      const int *c = NULL;
      int nc = ass_idx_candidatos(t, ate, &c), nb, j, nI, nIb = 0;
      long ch, chb = 0;
      long long px = ass_idx_prox(t), pxb = LLONG_MAX;
      bruto(t, ate, bc, &nb);
      if (nc != nb || memcmp(c, bc, sizeof(int) * (size_t)nb)) erros++;
      ass_idx_janela(t + 500, t + 2200, &nI, &ch);
      for (j = 0; j < assTrack->n_events; j++) {
        const ASS_Event *e = &assTrack->events[j];
        if (e->Start >= t + 500 && e->Start < t + 2200) { nIb++; chb += (long)strlen(e->Text); }
        if (e->Start > t && e->Start < pxb) pxb = e->Start;
      }
      if (nI != nIb || ch != chb || px != pxb) erros++;
    }
    ok(!erros, k ? "[1] after removing events: active set, window and next start = full walk"
                 : "[1] active set (track order), forecast window and next start = full walk (3000 instants)");
    /* Events leave (as the 2-minute prune): drop every third. */
    { int w = 0;
      for (i = 0; i < assTrack->n_events; i++) {
        if (i % 3 == 0) { ass_free_event(assTrack, i); continue; }
        assTrack->events[w++] = assTrack->events[i];
      }
      assTrack->n_events = w; }
    erros = 0;
  }
  free(bc);
}

static void teste_assinatura(void) {
  static const char *const textos[] = {
    "{\\pos(100.5,200.25)}static sign", "{\\move(100,100,500,300)}moving banner",
    "{\\move(10,20,30,40,100,200)}timed move", "{\\pos(10,10)\\t(0,100,\\fs20)}animated",
    "{\\pos(300,300)\\clip(0,0,10,10)}clipped", "plain line", "{\\pos(50,60)\\frz10}rotated",
  };
  int i, durs[2] = { 100, 1000 }, d, r, erros = 0, acertos0 = (int)assSigAcertos;
  for (i = 0; i < (int)(sizeof textos / sizeof textos[0]); i++)
    for (d = 0; d < 2; d++) {
      ASS_Event e; memset(&e, 0, sizeof e);
      e.Text = strdup(textos[i]); e.Start = 5000; e.Duration = durs[d]; e.Style = 0; e.ReadOrder = i * 2 + d;
      for (r = 0; r < 4; r++) {
        long long t = 5000 + r * durs[d] / 3;
        uint64_t s1 = 0, s2 = 0; double x1 = 0, y1 = 0, x2 = 0, y2 = 0; int m1 = -1, m2 = -1;
        AssMove mv; memset(&mv, 0, sizeof mv);
        int a = ass_rigid_parse(&e, t, 1920, 1080, &s1, &x1, &y1, &m1);
        int b = ass_rigid_parse_cru(&e, 1920, 1080, &s2, &x2, &y2, &m2, &mv);
        if (b && m2 == 2) {
          double rel = (double)(t - e.Start);
          if (rel <= mv.t1 || mv.t2 <= mv.t1) { x2 = mv.x1; y2 = mv.y1; }
          else if (rel >= mv.t2) { x2 = mv.x2; y2 = mv.y2; }
          else { double u = (rel - mv.t1) / (mv.t2 - mv.t1); x2 = mv.x1 + (mv.x2 - mv.x1) * u; y2 = mv.y1 + (mv.y2 - mv.y1) * u; }
        }
        if (a != b || (a && (s1 != s2 || m1 != m2 || fabs(x1 - x2) > 1e-9 || fabs(y1 - y2) > 1e-9))) erros++;
      }
      free(e.Text);
    }
  ok(!erros, "[2] cached signature, mode and position (also along \\move) = the raw parse");
  ok((int)assSigAcertos - acertos0 > 20, "[2] repeated frames come from the cache");
}

/* Reference composite in floats (straight colour, same order). */
static void referencia(const AssCpuImage *im, int n, float escF, int minX, int minY, int W, int H, float *rgba) {
  int i, x, y;
  memset(rgba, 0, sizeof(float) * 4 * (size_t)W * (size_t)H);
  for (i = 0; i < n; i++) {
    const AssCpuImage *m = &im[i];
    float e = m->esc > 0.01f ? m->esc : escF, rr = escF / e;
    float ox = ((float)m->x + m->subX) * rr, oy = ((float)m->y + m->subY) * rr;
    float cr = (float)((m->color >> 24) & 255u) / 255.f, cg = (float)((m->color >> 16) & 255u) / 255.f;
    float cb = (float)((m->color >> 8) & 255u) / 255.f, op = (float)(255u - (m->color & 255u)) / 255.f;
    for (y = 0; y < H; y++)
      for (x = 0; x < W; x++) {
        float sx = ((float)(x + minX) + 0.5f - ox) / rr - 0.5f, sy = ((float)(y + minY) + 0.5f - oy) / rr - 0.5f;
        int ix = (int)floorf(sx), iy = (int)floorf(sy);
        float fx = sx - (float)ix, fy = sy - (float)iy, v = 0, *d;
        int a, b;
        for (b = 0; b < 2; b++) for (a = 0; a < 2; a++) {
          int px = ix + a, py = iy + b;
          float w = (a ? fx : 1 - fx) * (b ? fy : 1 - fy);
          if (px >= 0 && px < m->w && py >= 0 && py < m->h) v += w * (float)m->bitmap[(size_t)py * (size_t)m->stride + (size_t)px];
        }
        v = v / 255.f * op;
        d = &rgba[((size_t)y * (size_t)W + (size_t)x) * 4];
        { float pa = d[3], na = v + pa * (1 - v);
          d[0] = na > 0 ? (cr * v + d[0] * pa * (1 - v)) / na : 0;
          d[1] = na > 0 ? (cg * v + d[1] * pa * (1 - v)) / na : 0;
          d[2] = na > 0 ? (cb * v + d[2] * pa * (1 - v)) / na : 0;
          d[3] = na; }
      }
  }
}

/* The layers of a flattened frame composited like the GPU does: each layer
 * (straight RGBA at its own scale) bilinear onto the text grid, in order. */
static void compor_camadas(const AssCpuFrame *f, float escF, int minX, int minY, int W, int H, float *rgba) {
  int c, x, y;
  memset(rgba, 0, sizeof(float) * 4 * (size_t)W * (size_t)H);
  for (c = 0; c < f->count; c++) {
    const AssCpuImage *m = &f->images[c];
    float e = m->esc > 0.01f ? m->esc : escF, rr = escF / e;
    float ox = (float)m->x * rr, oy = (float)m->y * rr;
    for (y = 0; y < H; y++)
      for (x = 0; x < W; x++) {
        float sx = ((float)(x + minX) + 0.5f - ox) / rr - 0.5f, sy = ((float)(y + minY) + 0.5f - oy) / rr - 0.5f;
        int ix = (int)floorf(sx), iy = (int)floorf(sy), a, b, k;
        float fx = sx - (float)ix, fy = sy - (float)iy, pm[4] = {0, 0, 0, 0}, *d, v;
        for (b = 0; b < 2; b++) for (a = 0; a < 2; a++) {
          int px = ix + a, py = iy + b;
          float w = (a ? fx : 1 - fx) * (b ? fy : 1 - fy);
          if (px >= 0 && px < m->w && py >= 0 && py < m->h) {
            const unsigned char *q = m->bitmap + ((size_t)py * (size_t)m->w + (size_t)px) * 4;
            float al = q[3] / 255.f;
            for (k = 0; k < 3; k++) pm[k] += w * al * q[k] / 255.f;   /* GL_LINEAR of straight RGBA, as premultiplied */
            pm[3] += w * al;
          }
        }
        v = pm[3];
        if (v <= 0) continue;
        d = &rgba[((size_t)y * (size_t)W + (size_t)x) * 4];
        { float pa = d[3], na = v + pa * (1 - v);
          for (k = 0; k < 3; k++) d[k] = na > 0 ? (pm[k] + d[k] * pa * (1 - v)) / na : 0;
          d[3] = na; }
      }
  }
}

static void teste_flatten(void) {
  AssCpuFrame f; int i, x, y, r;
  /* r 0: one scale; r 1: rigid-cache images with sub-pixel offsets (Exact:
   * resampled into place, owner 23/11/2026). */
  for (r = 0; r < 2; r++) {
    float escF = 0.5f;
    int maxErr = 0, maxErrA = 0, W, H, minX, minY;
    AssCpuImage copia[24];
    memset(&f, 0, sizeof f);
    f.count = 24; f.images = calloc(24, sizeof *f.images); f.esc = escF;
    for (i = 0; i < 24; i++) {
      AssCpuImage *m = &f.images[i];
      m->w = 20 + aleat(60); m->h = 8 + aleat(40); m->stride = m->w;
      m->x = 20 + aleat(300); m->y = 20 + aleat(200);
      m->color = ((unsigned)aleat(256) << 24) | ((unsigned)aleat(256) << 16) | ((unsigned)aleat(256) << 8) | (unsigned)(aleat(3) ? 0 : 100);
      m->type = IMAGE_TYPE_OUTLINE;
      if (r == 1 && i % 3 == 1) { m->subX = 0.37f; m->subY = 0.61f; } /* rigid cache, sub-pixel */
      m->stride = m->w;
      m->bitmap = malloc((size_t)m->w * (size_t)m->h);
      for (y = 0; y < m->h; y++) for (x = 0; x < m->w; x++) {
        int v = (x == 0 || y == 0 || x == m->w - 1 || y == m->h - 1) ? 0 : aleat(4) ? 255 : aleat(256);
        m->bitmap[(size_t)y * (size_t)m->w + (size_t)x] = (unsigned char)v;
      }
      copia[i] = *m;
      copia[i].bitmap = malloc((size_t)m->w * (size_t)m->h); memcpy(copia[i].bitmap, m->bitmap, (size_t)m->w * (size_t)m->h);
    }
    ok(ass_frame_flatten(&f, 960, 540, 0) == 1 && f.count == 1 && f.images[0].rgba,
       r ? "[3] sub-pixel frame flattened to one RGBA image" : "[3] one-scale frame flattened to one RGBA image");
    { const AssCpuImage *m = &f.images[0];
      minX = m->x; minY = m->y; W = m->w; H = m->h; }
    { float *ref = malloc(sizeof(float) * 4 * (size_t)W * (size_t)H), *got = malloc(sizeof(float) * 4 * (size_t)W * (size_t)H);
      referencia(copia, 24, escF, minX, minY, W, H, ref);
      compor_camadas(&f, escF, minX, minY, W, H, got);
      for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
        const float *p = &got[((size_t)y * (size_t)W + (size_t)x) * 4];
        const float *q = &ref[((size_t)y * (size_t)W + (size_t)x) * 4];
        int ea = abs((int)lroundf(p[3] * 255.f) - (int)lroundf(q[3] * 255.f));
        if (ea > maxErrA) maxErrA = ea;
        if (q[3] > 0.25f && p[3] > 0.25f) for (i = 0; i < 3; i++) { int e = abs((int)lroundf(p[i] * 255.f) - (int)lroundf(q[i] * 255.f)); if (e > maxErr) maxErr = e; }
      }
      free(ref); free(got); }
    { char m[200];
      snprintf(m, sizeof m, "[3] %s: the flattened image matches the float composite (alpha off by %d, colour by %d; limit 4)",
               r == 0 ? "one-scale frame" : "sub-pixel frame (Exact)", maxErrA, maxErr);
      ok(maxErrA <= 4 && maxErr <= 5, m); }
    for (i = 0; i < 24; i++) free(copia[i].bitmap);
    ass_frame_liberar(&f);
  }
}

/* [4] A split frame (a soft pass at its own scale) is never flattened: it
 * stays per image through the texture cache (owner 23/11/2026: Per image). */
static void teste_dividido(void) {
  AssCpuFrame f; int i, x, y;
  memset(&f, 0, sizeof f);
  f.count = 24; f.images = calloc(24, sizeof *f.images); f.esc = 0.5f;
  for (i = 0; i < 24; i++) {
    AssCpuImage *m = &f.images[i];
    m->w = 20 + aleat(60); m->h = 8 + aleat(40); m->stride = m->w;
    m->x = 20 + aleat(300); m->y = 20 + aleat(200);
    m->color = ((unsigned)aleat(256) << 24) | ((unsigned)aleat(256) << 16) | ((unsigned)aleat(256) << 8);
    m->type = IMAGE_TYPE_OUTLINE;
    if (i % 3 == 0) m->esc = 0.4f;
    m->bitmap = malloc((size_t)m->w * (size_t)m->h);
    for (y = 0; y < m->h; y++) for (x = 0; x < m->w; x++)
      m->bitmap[(size_t)y * (size_t)m->w + (size_t)x] = (unsigned char)(aleat(4) ? 255 : aleat(256));
  }
  ok(ass_frame_flatten(&f, 960, 540, 0) == 0 && f.count == 24 && !f.images[0].rgba, "[4] a split frame stays per image");
  ass_frame_liberar(&f);
}

/* [5] Overload (log 12, Fate Kaleido-subs ~600 images per frame): a split
 * frame over ASS_FLAT_SOBRECARGA images is flattened, the soft pass resampled
 * into the text grid, and matches the float composite. A busy normal frame
 * (57 images, Kaguya ED) stays per image. */
static void teste_sobrecarga(void) {
  AssCpuFrame f; int i, x, y, n, maxErr = 0, maxErrA = 0, W, H, minX, minY;
  AssCpuImage *copia;
  const float escF = 1.0f;
  /* 57 split images: unchanged path */
  memset(&f, 0, sizeof f);
  n = 57; f.count = n; f.images = calloc((size_t)n, sizeof *f.images); f.esc = escF;
  for (i = 0; i < n; i++) {
    AssCpuImage *m = &f.images[i];
    m->w = 6 + aleat(20); m->h = 2 + aleat(6); m->stride = m->w;
    m->x = 20 + aleat(600); m->y = 20 + aleat(400);
    m->color = ((unsigned)aleat(256) << 24) | ((unsigned)aleat(256) << 16) | ((unsigned)aleat(256) << 8);
    m->type = IMAGE_TYPE_OUTLINE;
    if (i % 2) m->esc = 0.5f;
    m->bitmap = malloc((size_t)m->w * (size_t)m->h);
    memset(m->bitmap, 200, (size_t)m->w * (size_t)m->h);
  }
  ok(ass_frame_flatten(&f, 1920, 1080, 0) == 0 && f.count == n, "[5] 57 split images (normal busy scene) stay per image");
  ass_frame_liberar(&f);
  /* 600 split images: flattened */
  memset(&f, 0, sizeof f);
  n = 600; f.count = n; f.images = calloc((size_t)n, sizeof *f.images); f.esc = escF;
  copia = calloc((size_t)n, sizeof *copia);
  for (i = 0; i < n; i++) {
    AssCpuImage *m = &f.images[i];
    m->w = 4 + aleat(30); m->h = 1 + aleat(5); m->stride = m->w;
    m->x = 100 + aleat(300); m->y = 100 + aleat(150);
    m->color = ((unsigned)aleat(256) << 24) | ((unsigned)aleat(256) << 16) | ((unsigned)aleat(256) << 8) | (unsigned)(aleat(3) ? 0 : 90);
    m->type = IMAGE_TYPE_OUTLINE;
    if (i % 2) { m->esc = 0.5f; m->x /= 2; m->y /= 2; }   /* soft pass at 50 %, on its own grid */
    m->bitmap = malloc((size_t)m->w * (size_t)m->h);
    for (y = 0; y < m->h; y++) for (x = 0; x < m->w; x++)
      m->bitmap[(size_t)y * (size_t)m->w + (size_t)x] = (unsigned char)(aleat(4) ? 255 : aleat(256));
    copia[i] = *m;
    copia[i].bitmap = malloc((size_t)m->w * (size_t)m->h); memcpy(copia[i].bitmap, m->bitmap, (size_t)m->w * (size_t)m->h);
  }
  ok(ass_frame_flatten(&f, 1920, 1080, 0) == 2 && f.count == 1 && f.images[0].rgba,
     "[5] 600 split images (Fate) flattened to one RGBA image, reported as overload");
  minX = f.images[0].x; minY = f.images[0].y; W = f.images[0].w; H = f.images[0].h;
  { float *ref = malloc(sizeof(float) * 4 * (size_t)W * (size_t)H), *got = malloc(sizeof(float) * 4 * (size_t)W * (size_t)H);
    referencia(copia, n, escF, minX, minY, W, H, ref);
    compor_camadas(&f, escF, minX, minY, W, H, got);
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
      const float *p = &got[((size_t)y * (size_t)W + (size_t)x) * 4];
      const float *q = &ref[((size_t)y * (size_t)W + (size_t)x) * 4];
      int ea = abs((int)lroundf(p[3] * 255.f) - (int)lroundf(q[3] * 255.f));
      if (ea > maxErrA) maxErrA = ea;
      if (q[3] > 0.25f && p[3] > 0.25f) for (i = 0; i < 3; i++) { int e = abs((int)lroundf(p[i] * 255.f) - (int)lroundf(q[i] * 255.f)); if (e > maxErr) maxErr = e; }
    }
    free(ref); free(got); }
  { char m[200];
    snprintf(m, sizeof m, "[5] the overload flatten matches the float composite (alpha off by %d, colour by %d; limit 4/5)", maxErrA, maxErr);
    ok(maxErrA <= 4 && maxErr <= 5, m); }
  for (i = 0; i < n; i++) free(copia[i].bitmap);
  free(copia);
  ass_frame_liberar(&f);
}

/* [6] Overload frames in one pass (log 14, 23/11/2026): after a frame over
 * ASS_FLAT_SOBRECARGA images, the next split frame goes to one pass; a frame
 * under the limit puts the one after back on the split. Decided per frame. */
static void teste_uma_passada(void) {
  ass_ultimo_imgs_guardar(0);
  ok(!ass_uma_passada_sobrecarga(1, 0.5), "[6] new track: the split stays");
  ass_ultimo_imgs_guardar(57);
  ok(!ass_uma_passada_sobrecarga(1, 0.5), "[6] after a normal busy frame (57 images, Kaguya): the split stays");
  ass_ultimo_imgs_guardar(ASS_FLAT_SOBRECARGA);
  ok(!ass_uma_passada_sobrecarga(1, 0.5), "[6] at the limit (300 images): the split stays");
  ass_ultimo_imgs_guardar(1200);
  ok(ass_uma_passada_sobrecarga(1, 0.5), "[6] after an overloaded frame (1200 images, Fate OP): one pass");
  ok(!ass_uma_passada_sobrecarga(0, 0.0), "[6] a frame that was not split anyway is not touched");
  ass_ultimo_imgs_guardar(40);   /* the one-pass frame came out under the limit */
  ok(!ass_uma_passada_sobrecarga(1, 0.5), "[6] the frame after a normal one is split again");
  ass_ultimo_imgs_guardar(1200);
  assrender_limpar();
  ok(!ass_uma_passada_sobrecarga(1, 0.5), "[6] a new track (assrender_limpar) starts on the split");
}

int main(void) {
  assperf_teste_definir(AP_ESCALA, AP_RES_BAIXO);
  teste_indice();
  teste_assinatura();
  teste_flatten();
  teste_dividido();
  teste_sobrecarga();
  teste_uma_passada();
  printf(falhas ? "ass_indice: %d FALHAS\n" : "ass_indice: ok\n", falhas);
  return falhas != 0;
}
