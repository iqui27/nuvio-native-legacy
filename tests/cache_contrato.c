/* Real cache contract: LRU alone must not evict nearby rows under churn. */
#include "../src/sdlcompat.h"
#include "../src/gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
static int offMain, uploads;
static SDL_threadID mainThread;
static void upload(GLenum t, GLint l, GLint in, GLsizei w, GLsizei h,
                   GLint border, GLenum f, GLenum type, const void *p) {
  if (SDL_ThreadID() != mainThread) offMain++;
  uploads++;
  glTexImage2D(t, l, in, w, h, border, f, type, p);
}
#undef glTexImage2D
#define glTexImage2D upload
#include "../src/perfiltv.h"
static PtvPlataforma testPlatform;
#define ptv_plataforma() testPlatform
#include "../src/tex_cache.c"
#undef glTexImage2D
#undef ptv_plataforma

static void rowURL(char *url, int row, int col) {
  snprintf(url, 128, "https://contract.test/row%d/poster%d.jpg", row, col);
}
static int find(int row, int col) {
  char url[128]; rowURL(url, row, col);
  return acharIndice(url, hashCaminho(url));
}
static void publish(int row, int col) {
  char url[128]; rowURL(url, row, col);
  tex_pedido_fora(0);
  (void)tex_obter_limite(url, 128, 0, 0);
  int i = find(row, col);
  if (i < 0 && row >= 100) return; // Speculative work may wait for a cold slot.
  assert(i >= 0);
  /* Network/codec boundary fixture; all admission, eviction and GL are real. */
  filaIni = filaFim = decIni = decFim = 0;
  itens[i].estado = DECODIFICADO;
  itens[i].tetoUsado = itens[i].fonteW = 128;
  itens[i].sup = SDL_CreateRGBSurfaceWithFormat(0, 128, 192, 32, SDL_PIXELFORMAT_ABGR8888);
  assert(itens[i].sup);
  for (int f = 0; f < 16 && itens[i].estado != PRONTO; f++) {
    Uint32 start = SDL_GetTicks(); tex_bombear(3);
    assert(SDL_GetTicks() - start < 250);
  }
  assert(itens[i].estado == PRONTO);
}
static void *wrongThread(void *unused) { (void)unused; tex_bombear(1); return NULL; }
int main(void) {
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  SDL_Window *window = SDL_CreateWindow("cache contract", 0, 0, 32, 32,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window); SDL_GLContext gl = SDL_GL_CreateContext(window); assert(gl);
  mainThread = SDL_ThreadID();
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  condDec = SDL_CreateCond(); condLivre = SDL_CreateCond();
  const long ram[] = {650, 1500, 2400, 4096};
  for (int platform = PTV_LG; platform <= PTV_ANDROID; platform++)
    for (int cls = 0; cls < 4; cls++) {
      testPlatform = platform;
      memset(itens, 0, sizeof itens); bytesUsados = 0; quadroAtual = 100;
      orcamento = ptv_tex_auto_mb(platform, ram[cls]) * 1048576L;
      orcMemTotal = ram[cls];
      pressaoPct = pressaoLivrePct = pressaoPlay = 100;
      decodeReservado = (long)ptv_cache(platform, ram[cls]).decodeMb * 1048576;
      assert(!decodeCabe()); decodeReservado = 0; assert(decodeCabe());
      /* Small slots force churn without wasting hundreds of MiB on host GL. */
      nMax = 48; int misses = 0, previous = -100;
      int fetched[21][6] = {{0}}, decodedCount[21][6] = {{0}};
      for (int step = 0; step <= 40; step++) {
        int focus = step <= 20 ? step : 40 - step;
        tex_novo_quadro(); tex_novo_quadro(); tex_novo_quadro();
#ifdef NV_CACHE_RESIDENCY
        tex_residencia_inicio(focus);
        int neighbours = ptv_cache(platform, ram[cls]).fileiras;
#else
        int neighbours = ram[cls] < 1200 ? 1 : 2;
#endif
        /* Protect already visited neighbour rows before admitting new art. */
        for (int row = 0; row <= 20; row++) for (int c = 0; c < 6; c++) {
          char url[128]; rowURL(url, row, c);
#ifdef NV_CACHE_RESIDENCY
          tex_residencia_marcar(url, row);
#endif
        }
        for (int row = focus - neighbours; row <= focus + neighbours; row++) {
          if (row < 0 || row > 20) continue;
          for (int c = 0; c < 6; c++) if (find(row, c) < 0) {
            // An overlapping window has already paid network and decode.
            if (abs(row - previous) <= neighbours && decodedCount[row][c]) misses++;
            fetched[row][c]++; decodedCount[row][c]++;
            publish(row, c);
          }
        }
        /* Hold focus while a background burst competes with older rows. */
        tex_novo_quadro(); tex_novo_quadro(); tex_novo_quadro();
        for (int c = 0; c < 6; c++) {
          char url[128]; rowURL(url, focus, c);
          (void)tex_obter_limite(url, 128, 0, 0);
        }
#ifdef NV_CACHE_RESIDENCY
        tex_residencia_inicio(focus);
        for (int row = 0; row <= 20; row++) for (int c = 0; c < 6; c++) {
          char url[128]; rowURL(url, row, c); tex_residencia_marcar(url, row);
        }
#endif
        /* Fill spare slots with unrelated recent art; nearby rows are older. */
        for (int c = 0; c < 24; c++) publish(100 + step, c);
        for (int row = focus - neighbours; row <= focus + neighbours; row++) {
          if (row < 0 || row > 20) continue;
          for (int c = 0; c < 6; c++) {
            char url[128]; rowURL(url, row, c);
            int i = find(row, c);
            if (i < 0) { misses++; continue; }
            GLuint old = itens[i].tex;
            assert(old && tex_obter_limite(url, 128, 0, 0) == old);
          }
        }
        assert(bytesUsados <= orcamento && !tex_despejos_quentes);
        previous = focus;
      }
      if (misses) {
        fprintf(stderr, "FAIL: platform=%d RAM=%ld: %d resident re-fetch/re-decode requests\n",
                platform, ram[cls], misses);
        return 1;
      }
      for (int i = 0; i < nMax; i++) if (itens[i].tex) glDeleteTextures(1, &itens[i].tex);
      printf("PASS: platform=%d RAM=%ld down20/up20, resident misses=0, budget=%ld MiB\n",
             platform, ram[cls], orcamento / 1048576);
    }
#ifdef NV_CACHE_RESIDENCY
  // Byte admission: retain visible art and queue a new upload when full.
  memset(itens, 0, sizeof itens); bytesUsados = 0; quadroAtual += 3;
  publish(0, 0);
  long full = bytesUsados; orcamento = full;
  itens[1].estado = DECODIFICADO;
  itens[1].sup = SDL_CreateRGBSurfaceWithFormat(0, 128, 192, 32, SDL_PIXELFORMAT_ABGR8888);
  assert(tex_bombear(3) == 0 && bytesUsados == full && itens[0].tex && itens[1].sup);
  liberarSup(itens[1].sup); itens[1].sup = NULL;
  glDeleteTextures(1, &itens[0].tex);
  orcamento = 32L * 1048576;
  // Trim sheds the far row before a nearer, older row; visible survives.
  memset(itens, 0, sizeof itens); bytesUsados = 0; quadroAtual += 3;
  publish(0, 0); publish(1, 0); publish(5, 0);
  tex_novo_quadro(); tex_novo_quadro(); tex_novo_quadro();
  tex_residencia_inicio(0);
  for (int r = 0; r <= 5; r++) {
    char url[128]; rowURL(url, r, 0); tex_residencia_marcar(url, r);
  }
  int visible = find(0, 0), near = find(1, 0), far = find(5, 0);
  itens[visible].ultimoQuadro = quadroAtual;
  tex_pressao_memoria(80);
  assert(despejar(0) == far && itens[near].tex && itens[visible].tex);
  assert(despejar(1) == near && itens[visible].tex);
  assert(despejar(1) == -1);
  glDeleteTextures(1, &itens[visible].tex);
  /* A decoded upload offered from another thread must leave GL untouched. */
  memset(itens, 0, sizeof itens); bytesUsados = 0;
  itens[0].estado = DECODIFICADO;
  itens[0].sup = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 32, SDL_PIXELFORMAT_ABGR8888);
  int before = uploads;
  pthread_t worker; assert(!pthread_create(&worker, NULL, wrongThread, NULL));
  pthread_join(worker, NULL);
  assert(uploads == before && !offMain && itens[0].sup);
  liberarSup(itens[0].sup); itens[0].sup = NULL;
#endif
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(window); SDL_Quit();
  puts("PASS: main-context uploads only");
}
