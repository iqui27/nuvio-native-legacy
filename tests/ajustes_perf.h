// Host walk: real Settings drawing, no account, network or device writes.
// The preview scenes run on the walk's own clock, not SDL_GetTicks, so two
// runs draw the same frames.
extern void ajustes_teste_cena_t(float t);
static void perf_frame(unsigned frame, unsigned rel, FILE *csv, int check) {
  int r0 = txt_rasterizadas;
  struct timespec t0, t1;
  ajustes_teste_cena_t(rel * 0.016f);
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
  gfx_novo_quadro(); txt_novo_quadro(); tex_novo_quadro();
  ajustes_atualizar(1.0f / 60.0f, rel * 16);
  glClear(GL_COLOR_BUFFER_BIT);
  ajustes_desenhar(rel * 16);
  // Thread CPU time: wall time on a loaded host measures the scheduler.
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
  double ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
  if (csv) fprintf(csv, "%u,%.6f,%d,%d,%d,%d\n", frame, ms, gfx_n_rect, txt_rasterizadas - r0, perf_uploads, gfx_n_outros);
  if (csv && getenv("DBG_RAST") && txt_rasterizadas != r0) fprintf(stderr, "FRAME %u\n", frame);
  if (check && txt_rasterizadas != r0) {
    fprintf(stderr, "FAIL unchanged scroll frame %u recreated %d text textures\n", frame, txt_rasterizadas - r0);
    exit(1);
  }
  perf_uploads = 0;
  glFinish(); // drain outside CPU timing; no vsync/queue carryover
}
static void perf_walk(SDL_Window *win) {
  const char *dir = getenv("NUVIO_AJUSTES_PERF");
  char path[700], id[40];
  unsigned frame = 0;
  int op, sec, maxsec = 0, n;
  const char *key, *label;
  for (n = 0; ajustes_teste_cena_item(n, &op, &sec, &key, &label); n++) if (sec > maxsec) maxsec = sec;
  snprintf(path, sizeof path, "%s/frames.csv", dir);
  FILE *csv = fopen(path, "w"); assert(csv);
  fprintf(csv, "frame,cpu_ms,draw_calls,text_renders,texture_uploads,other_gl\n");
  for (sec = 0; sec <= maxsec; sec++) {
    snprintf(id, sizeof id, "v2-sec-%d", sec);
    assert(ajustes_teste_quadro(id));
    // Warm every Down position in this section before checking reuse.
    for (int pass = 0; pass < 2; pass++) {
      assert(ajustes_teste_quadro(id));
      for (int step = 0; step < 48; step++) {
        tecla(SDLK_DOWN);
        for (int q = 0; q < 8; q++) perf_frame(frame++, frame % 384, pass ? csv : NULL, pass && getenv("NUVIO_PERF_ASSERT"));
      }
    }
  }
  fclose(csv);
  assert(ajustes_teste_quadro("v2-menu"));
  // Existing capture helper gives a settled PNG at the same deterministic clock.
  snprintf(path, sizeof path, "%s/index.png", dir); captura(path, win);
  assert(ajustes_teste_quadro("v2-sec-0"));
  snprintf(path, sizeof path, "%s/section.png", dir); captura(path, win);
  printf("Settings walk: %d sections, %u frames; %s\n", maxsec + 1, frame / 2, dir);
}
