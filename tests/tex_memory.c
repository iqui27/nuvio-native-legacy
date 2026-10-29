#include "sdlcompat.h"
#include "jpegrapido.h"
#include "perfiltv.h"
#include <stdio.h>
#include <jpeglib.h>
#include <assert.h>
#include <unistd.h>

static size_t largestPixels;
static SDL_Surface *trackedSurface(Uint32 flags, int w, int h, int depth,
                                   Uint32 r, Uint32 g, Uint32 b, Uint32 a) {
  size_t bytes = (size_t)w * h * ((depth + 7) / 8);
  if (bytes > largestPixels) largestPixels = bytes;
  return SDL_CreateRGBSurface(flags, w, h, depth, r, g, b, a);
}
/* Compile the real decoder with its pixel allocation boundary instrumented. */
#define SDL_CreateRGBSurface trackedSurface
#define nv_superficie trackedNvSurface
/* sdlcompat is already included; recreate its allocation boundary for this TU. */
static SDL_Surface *trackedNvSurface(Uint32 flags, int w, int h, int depth, Uint32 format) {
  int bits; Uint32 r, g, b, a;
  assert(SDL_PixelFormatEnumToMasks(format, &bits, &r, &g, &b, &a));
  (void)depth;
  return trackedSurface(flags, w, h, bits, r, g, b, a);
}
#include "../src/jpegrapido.c"

int main(void) {
  char path[] = "/tmp/nv-jpeg-memory-XXXXXX";
  int fd = mkstemp(path);
  assert(fd >= 0);
  FILE *f = fdopen(fd, "wb");
  struct jpeg_compress_struct c;
  struct jpeg_error_mgr e;
  c.err = jpeg_std_error(&e);
  jpeg_create_compress(&c);
  jpeg_stdio_dest(&c, f);
  c.image_width = 3840; c.image_height = 2160;
  c.input_components = 3; c.in_color_space = JCS_RGB;
  jpeg_set_defaults(&c); jpeg_start_compress(&c, TRUE);
  unsigned char row[3840 * 3] = {0};
  while (c.next_scanline < c.image_height) {
    JSAMPROW p = row;
    jpeg_write_scanlines(&c, &p, 1);
  }
  jpeg_finish_compress(&c); jpeg_destroy_compress(&c); fclose(f);
  int ow, oh;
  SDL_Surface *s = jpeg_rapido_carregar(path, 480, &ow, &oh);
  unlink(path);
  assert(s && ow == 3840 && oh == 2160);
  assert(s->w == 480 && s->h == 270);
  assert((size_t)s->pitch * s->h <= 4u * 480 * 270 * 4);
  assert(largestPixels <= 4u * 480 * 270 * 4);
  SDL_FreeSurface(s);
  assert(ptv_tex_auto_mb(PTV_ANDROID, 2400) >= 160);
  const long ram[] = {0, 1024, 1499, 1500, 1999, 2000, 2400, 2999, 3000, 3999, 4000, 4096};
  const int mb[] = {32, 32, 32, 96, 96, 160, 160, 160, 192, 192, 256, 256};
  for (size_t i = 0; i < sizeof(ram) / sizeof(ram[0]); i++) {
    assert(ptv_tex_auto_mb(PTV_ANDROID, ram[i]) == mb[i]);
    assert(ptv_tex_teto_mb(PTV_ANDROID, ram[i]) == mb[i]);
  }
  puts("PASS: 4K JPEG decoded at 480x270; Android budgets scale with MemTotal");
}
