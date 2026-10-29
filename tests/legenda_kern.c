// TV SDL_ttf (without HarfBuzz): compare glyph origins against shaped pairs.
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
// Inspect the real non-HarfBuzz renderer's glyph positions after measuring.
#include "SDL_ttf.c"
#include <hb.h>
#include <hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char **argv) {
  const char *families[] = {"InterDisplay", "Montserrat", "Roboto", "AtkinsonHyperlegibleNext", "JetBrainsSans"};
  const char *weights[] = {"Regular", "Medium", "Bold"};
  const char *lines[] = {"We", "You", "Why", "\"Yes", "AV", "To", "\"W", "Va", "Ta", "Ay", "'Yes"};
  const int sizes[] = {24, 36, 48, 72};
  int failures = 0, checks = 0;
  FT_Library library; FT_Init_FreeType(&library); TTF_Init();
  SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_FillRect(shot, NULL, SDL_MapRGB(shot->format, 24, 24, 24));
  for (int f = 0; f < 5; f++) for (int weight = 0; weight < 3; weight++) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s-%s.ttf",
             getenv("NV_TEST_FONT_DIR") ? getenv("NV_TEST_FONT_DIR") : "deploy/app/fonts", families[f],
             f == 4 && weight == 1 ? "SemiBold" : weights[weight]);
    for (int size = 0; size < 4; size++) {
      TTF_Font *font = TTF_OpenFont(path, sizes[size]);
      FT_Face face; if (!font || FT_New_Face(library, path, 0, &face)) return 2;
      FT_Set_Pixel_Sizes(face, 0, sizes[size]);
      hb_font_t *hb = hb_ft_font_create_referenced(face);
      for (int line = 0; line < 11; line++) {
        const char *s = lines[line];
        int pen = 0;
        for (int i = 0; s[i]; i++) {
          if (i) pen += TTF_GetFontKerningSizeGlyphs(font, s[i-1], s[i]);
          // Pair kerning from HarfBuzz, independently of FreeType's kern API.
          if (s[i+1]) {
            hb_buffer_t *buf = hb_buffer_create();
            hb_buffer_add_utf8(buf, s+i, 2, 0, 2); hb_buffer_guess_segment_properties(buf);
            hb_shape(hb, buf, NULL, 0);
            unsigned count; hb_glyph_position_t *on = hb_buffer_get_glyph_positions(buf, &count);
            int advance = on[0].x_advance;
            hb_buffer_destroy(buf); buf = hb_buffer_create();
            hb_buffer_add_utf8(buf, s+i, 2, 0, 2); hb_buffer_guess_segment_properties(buf);
            hb_feature_t feature; hb_feature_from_string("kern=0", -1, &feature);
            hb_shape(hb, buf, &feature, 1);
            hb_glyph_position_t *off = hb_buffer_get_glyph_positions(buf, &count);
            double expected = (advance - off[0].x_advance) / 64.0;
            int actual = TTF_GetFontKerningSizeGlyphs(font, s[i], s[i+1]);
            if (fabs(actual - expected) > 1.0) {
              if (failures < 8) fprintf(stderr, "%s %d %s glyph %d x=%d kern=%d expected=%.2f\n", path, sizes[size], s, i+1, pen, actual, expected);
              failures++;
            }
            checks++; hb_buffer_destroy(buf);
          }
          int advance; TTF_GlyphMetrics(font, s[i], NULL, NULL, NULL, NULL, &advance); pen += advance;
        }
        int width, height; TTF_SizeUTF8(font, s, &width, &height);
        if (abs(width - pen) > 2) failures++;
        int origin = 0;
        for (int i = 0; s[i]; i++) {
          if (i) origin += TTF_GetFontKerningSizeGlyphs(font, s[i-1], s[i]);
          if (abs(FT_FLOOR(font->pos_buf[i].x) - origin) > 2) failures++;
          int advance; TTF_GlyphMetrics(font, s[i], NULL, NULL, NULL, NULL, &advance);
          origin += advance;
          checks++;
        }
        if (weight == 0 && size == 1) {
          SDL_Surface *text = TTF_RenderUTF8_Blended(font, s, (SDL_Color){255,255,255,255});
          if (!text) return 2;
          SDL_Rect dest = { (line % 6) * 320 + (320-text->w)/2, f*200 + (line/6)*85+30, 0, 0 };
          SDL_BlitSurface(text, NULL, shot, &dest); SDL_FreeSurface(text);
        }
      }
      hb_font_destroy(hb); FT_Done_Face(face); TTF_CloseFont(font);
    }
  }
  if (argc > 1 && IMG_SavePNG(shot, argv[1])) return 2;
  SDL_FreeSurface(shot); TTF_Quit(); FT_Done_FreeType(library);
  printf("legenda_kern: %d glyph position checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
