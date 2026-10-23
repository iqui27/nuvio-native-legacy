// Optional deterministic backdrop: broad light/dark bands plus sharp stripes.
// Frost must preserve broad contrast while removing the sharp stripe edges.
#ifndef MENU_FROST_PROBE_H
#define MENU_FROST_PROBE_H
static void menu_probe_draw(void) {
  static GLuint tex;
  if (!getenv("NUVIO_SHOT_MENU_PROBE")) return;
  if (!tex) {
    unsigned char *p = malloc(1920 * 1080 * 4);
    assert(p);
    for (int y = 0; y < 1080; y++) for (int x = 0; x < 1920; x++) {
      int i = (y * 1920 + x) * 4;
      p[i] = p[i+1] = p[i+2] = (y < 480 ? 40 : 190) + (x % 8 < 4 ? 0 : 60);
      p[i+3] = 255;
    }
    glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(p); gfx_tex_esquecer(0);
  }
  gfx_rect((GfxRect){0, 0, 1920, 1080}, tex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
}
static void menu_probe_check(const unsigned char *pix) {
  double edge = 0, top = 0, bottom = 0;
  int n = 0;
  for (int y = 175; y < 780; y++) for (int x = 330; x < 410; x++) {
    const unsigned char *a = pix + ((1079-y)*1920+x)*4, *b = a+4;
    edge += abs((a[0]+a[1]+a[2])-(b[0]+b[1]+b[2])) / 3 > 6;
    n++;
    if (y >= 250 && y < 350) top += (a[0]+a[1]+a[2])/3.0;
    if (y >= 650 && y < 750) bottom += (a[0]+a[1]+a[2])/3.0;
  }
  edge /= n; top /= 8000; bottom /= 8000;
  printf("menu-frost edge=%.5f response=%.2f\n", edge, fabs(bottom-top));
  if (edge > .003 || (getenv("NUVIO_SHOT_MENU_PROBE") && fabs(bottom-top) < 5)) {
    printf("FAIL menu-frost: sharp edges or opaque backing\n"); exit(1);
  }
}
#endif
