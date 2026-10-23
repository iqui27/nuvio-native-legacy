// Collection layout geometry at every supported TV text scale; no GL or network.
#define desc_vertudo_n layout_count
#define desc_vertudo_item layout_item
#include "../src/vertudo.c"
#include <assert.h>

int layout_count(void) { return 122; }
int layout_item(int i, CatItem *item) {
  if (i < 0 || i >= 122) return 0;
  if (item) { memset(item, 0, sizeof *item); snprintf(item->meta, sizeof item->meta, "2020"); }
  return 1;
}

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
  fprintf(stderr, "scale %.2f: %s failed\n", scale, #condition); failures++; \
} } while (0)

int main(void) {
  const float scales[] = {.85f, .9f, 1.f, 1.1f, 1.2f, 1.3f};
  ColFolder folder = {0};
  collection = &folder;
  for (int s = 0; s < 6; s++) {
    float scale = scales[s];
    gfx_escala_txt_definir(scale);
    gfx_escala_sair(scale);
    float start = vtInicio(), end = vtFim(), width = end - start;
    float height = NV_TELA_H - VT_TOPO;
    lay = VT_LAY_MURAL; timeline = 0; montar(122);
    for (int block = 0; block < 24; block++) {
      float left = width, right = 0;
      for (int i = block * 5; i < block * 5 + 5; i++) {
        left = fminf(left, celX[i]); right = fmaxf(right, celX[i] + celW[i]);
        CHECK(celX[i] >= -.01f && celX[i] + celW[i] <= width + .01f);
      }
      CHECK(right - left >= width * .95f);
      CHECK(fabsf(left - (width - right)) < 1.f);
    }
    lay = VT_LAY_LINHA; timeline = 1; montar(122);
    GfxRect card = vtTimelineCard(VT_TOPO), art = vtTimelineArte(card);
    CHECK(card.x >= start && card.x + card.w <= end + .01f);
    CHECK(card.y >= VT_TOPO && card.y + card.h <= NV_TELA_H);
    CHECK(start + celX[0] >= start && start + celX[0] + celW[0] <= end + .01f);
    CHECK(art.x >= card.x && art.x + art.w <= card.x + card.w);
    CHECK(art.y >= card.y && art.y + art.h <= card.y + card.h);
    CHECK(card.x + card.w - (art.x + art.w + 24.f) - 32.f > 0);
    lay = VT_LAY_FLUXO; timeline = 0; montar(122); fluxoPos = 6;
    GfxRect center = fluxoRect(6, start, NULL), side = fluxoRect(7, start, NULL);
    CHECK(center.h >= height * .50f);
    CHECK(scale > 1.f || (center.h >= height * .55f && center.h <= height * .65f));
    CHECK(center.x >= start && center.x + center.w <= fluxoFim());
    CHECK(center.y >= VT_TOPO && center.y + center.h < fluxoLinhaY());
    CHECK(side.h < center.h && side.h > center.h * .7f);
    CHECK(fluxoInfoY() > fluxoLinhaY());
    // Reserve the existing logo/title, metadata and two synopsis lines.
    CHECK(fluxoInfoY() + 62.f + BADGE_H + 8.f + 60.f <= NV_TELA_H);
    CHECK(!precisaMontar(122));
    gfx_escala_txt_definir(scale == 1.f ? .85f : 1.f);
    gfx_escala_sair(gfx_escala_txt());
    CHECK(precisaMontar(122));
    gfx_escala_txt_definir(scale); gfx_escala_sair(scale);
    printf("scale %.2f: mural %.0f px, timeline %.0f px, flow %.0f/%.0f px\n",
           scale, width * scale, card.w * scale, center.h * scale, height * scale);
  }
  assert(failures == 0);
  puts("collection layout: ok");
  return 0;
}
