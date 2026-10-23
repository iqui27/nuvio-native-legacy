// Row spacing (Settings > Appearance, mockup margem.html) and the B2 glass
// page without bands (owner, 17/10/2026: "nao precisa ter essas faixas").
// NUVIO_DETAIL_CHECK=espaco, glass on. Each preset at 100%, plus the extreme
// text scales (85%, 130%): there the global scale (escalafixa's job) may also
// scale the margin, so side or side*scale both pass.
static int espaco_um(int p, int esc, unsigned char *pix) {
  static const int SIDE[] = { 80, 96, 120 };
  static const float K[] = { 0.85f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f };
  int fail = 0, best = 0, bx = -1, step = 0, sy = -1, ks = (int)(SIDE[p] * K[esc] + 0.5f);
  char id[40];
  { char c[600]; FILE *f;
    snprintf(c, sizeof c, "%s/ajustes.txt", dados_dir());
    f = fopen(c, "w"); assert(f);
    fprintf(f, "selected_theme 2\nvidroLocal 0\ndetEspacoLocal %d\nescalaTextoLocal %d\nescalaTextoVersaoLocal 1\n", p, esc);
    fclose(f);
    ajustes_dir(dados_dir()); ajustes_definir_vidro(1); }
  abrir(1, 1, SEC_ELENCO, 0);
  snprintf(id, sizeof id, "espaco-%d-escala-%d", p, esc);
  gravar(id);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  // The focused cast tile is the B2 ink fill (#F3F2EF): its left edge is the
  // row x. Longest run of ink pixels on any line, left of x 700.
  for (int y = 0; y < 1080; y++) {
    const unsigned char *l = pix + y * 1920 * 4;
    for (int x = 0, run = 0; x < 700; x++) {
      const unsigned char *q = l + x * 4;
      run = q[0] > 228 && q[1] > 228 && q[2] > 222 ? run + 1 : 0;
      if (run > best) { best = run; bx = x - run + 1; }
    }
  }
  // No bands: the strip left of the side margin is only backdrop, so no
  // horizontal hairline or tint step between rows (a band rim is ~+30 luma).
  for (int x = 12; x <= 40; x += 28)
    for (int y = 1; y < 1080; y++) {
      const unsigned char *a = pix + ((y - 1) * 1920 + x) * 4, *b = pix + (y * 1920 + x) * 4;
      int d = abs((a[0] + a[1] + a[2]) - (b[0] + b[1] + b[2])) / 3;
      if (d > step) { step = d; sy = 1079 - y; }
    }
  if (best < 150 || (abs(bx - SIDE[p]) > 4 && abs(bx - ks) > 4)) {
    printf("FAIL: spacing %d scale %.0f%%: cast row x %d (run %d), want %d\n", p, K[esc] * 100, bx, best, SIDE[p]); fail++;
  } else printf("PASS: spacing %d scale %.0f%%: cast row x %d (side %d)\n", p, K[esc] * 100, bx, SIDE[p]);
  if (step > 12) { printf("FAIL: spacing %d: band step %d luma at y %d\n", p, step, sy); fail++; }
  else printf("PASS: spacing %d scale %.0f%%: no band (max step %d luma)\n", p, K[esc] * 100, step);
  return fail;
}

static int espaco_checks(void) {
  static const int RUN[][2] = { { 0, 2 }, { 1, 2 }, { 2, 2 }, { 0, 0 }, { 2, 5 } };
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
  int fail = 0;
  for (int i = 0; i < 5; i++) fail += espaco_um(RUN[i][0], RUN[i][1], pix);
  free(pix);
  return fail ? 1 : 0;
}
