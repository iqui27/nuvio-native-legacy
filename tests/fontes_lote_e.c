// Batch E source-list regressions (hidden GL window, offline fixtures):
// neighbour badge warm-up bounds, UTF-8 filename wrap, [Cloud] dedupe,
// partial-block culling, source-wait pacing table and Top/Bottom badge rows.
#include "badges.h"
static float checkedSelos(uint64_t mask, float x, float y, float maxW, float h, float a);
static float checkedTom(uint64_t mask, float x, float y, float maxW, float h,
                        float r, float g, float b, float a);
#define badges_desenhar_selos checkedSelos
#define badges_desenhar_tom checkedTom
#include "../src/streams.c"
#undef badges_desenhar_selos
#undef badges_desenhar_tom
#include "player_pacing.h"
#include <assert.h>

static Uint32 now = 100000;
static int recording, badgeRows, lineCount, linesVisited;
static float badgeTop, badgeBottom, lineTop, lineBottom;

static void badgeSeen(float y, float h) {
  if (!recording) return;
  badgeRows++;
  badgeTop = y; badgeBottom = y + h;
}
static float checkedSelos(uint64_t mask, float x, float y, float maxW, float h, float a) {
  badgeSeen(y, h); return badges_desenhar_selos(mask, x, y, maxW, h, a);
}
static float checkedTom(uint64_t mask, float x, float y, float maxW, float h,
                        float r, float g, float b, float a) {
  badgeSeen(y, h); return badges_desenhar_tom(mask, x, y, maxW, h, r, g, b, a);
}
void fonte_addon_measure(void) {}
void fonte_addon_box(int x, int y, int w, int h, int emoji) { (void)x; (void)y; (void)w; (void)h; (void)emoji; }
void fonte_addon_visible(const char *text, float y, float h) { (void)text; (void)y; (void)h; }
void fonte_addon_line(const char *text, float y, float w, float h, float limit) {
  (void)text; (void)w; (void)limit;
  linesVisited++;
  if (!recording) return;
  if (!lineCount++ || y < lineTop) lineTop = y;
  if (y + h > lineBottom) lineBottom = y + h;
}

static void frame(void) {
  now += 16; SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(16);
  stream_folha_atualizar(.016f, now);
  gfx_novo_quadro();
  stream_folha_desenhar(now);
}
static int utf8Start(unsigned char c) { return (c & 0xC0) != 0x80; }
static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; stream_folha_evento(&e);
}
// Every lead byte is followed by exactly its continuation bytes.
static int utf8Valid(const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; *p;) {
    int n = *p < 0x80 ? 0 : (*p & 0xE0) == 0xC0 ? 1 : (*p & 0xF0) == 0xE0 ? 2 : (*p & 0xF8) == 0xF0 ? 3 : -1;
    if (n < 0) return 0;
    p++;
    while (n--) if ((*p++ & 0xC0) != 0x80) return 0;
  }
  return 1;
}

static void checkWrap(const char *name, float w) {
  const ArquivoLayout *a = arquivoLayout(name, w);
  char joined[2048] = {0};
  size_t off = 0;
  assert(a->n > 0);
  for (int k = 0; k < a->n; k++) {
    assert(a->off[k] == off && a->tam[k] > 0);
    assert(utf8Start((unsigned char)a->texto[a->off[k]]) && "piece starts inside a code point");
    char piece[2048]; memcpy(piece, a->texto + a->off[k], a->tam[k]); piece[a->tam[k]] = 0;
    // A single code point wider than w is the only allowed overflow.
    int cp = 1; for (const char *p = piece + 1; *p; p++) cp += utf8Start((unsigned char)*p);
    assert((cp == 1 || txt_largura(TXT_PG_FIM, piece) <= w) && "wrapped piece wider than slot");
    memcpy(joined + off, piece, a->tam[k]); off += a->tam[k];
  }
  joined[off] = 0;
  char want[2048]; snprintf(want, sizeof want, "%s", name);
  assert(!strcmp(joined, want) && "wrap must keep the whole filename, no ellipsis");
  assert(arquivoAltura(name, w) == a->n * FOLHA_ARQ_H);
}

static void setList(Stream *s, int count) {
  stream_definir_lista(count ? s : NULL, count);
  stream_folha_abrir();
}

static void badgeLayout(int bottom) {
  ajustes_shot_valor("sourceBadgePositionLocal", bottom);
  assert(ajustes_selos_embaixo() == bottom);
  for (int i = 0; i < 60; i++) frame();
  int row = 0;
  float y = FOLHA_TOPO + linhaTopo(row) - rolagem, h = linhaAltura(row);
  badgeRows = lineCount = 0; lineTop = lineBottom = 0; recording = 1;
  frame();
  recording = 0;
  assert(badgeRows > 0 && "badge row must still be drawn");
  assert(lineCount > 0);
  assert(badgeTop >= y - .5f && badgeBottom <= y + h + .5f && "badge row escapes its card");
  if (bottom) assert(badgeTop >= lineBottom - .5f && "bottom badges overlap the description");
  else assert(badgeBottom <= lineTop + .5f && "top badges overlap the title");
  printf("badges %s: card %.0f..%.0f badges %.0f..%.0f text %.0f..%.0f\n",
         bottom ? "bottom" : "top", y, y + h, badgeTop, badgeBottom, lineTop, lineBottom);
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  setvbuf(stdout, NULL, _IONBF, 0);
  // 1. Source-wait pacing: only sheet open + loading + no video is paced.
  fprintf(stderr, "step 1\n");
  assert(player_source_ui_unpaced(1, 0, 0, 0) == 1);
  assert(player_source_ui_unpaced(1, 1, 0, 0) == 1);
  assert(player_source_ui_unpaced(1, 1, 1, 1) == 1);
  assert(player_source_ui_unpaced(1, 1, 1, 0) == 0);
  assert(player_source_ui_unpaced(0, 1, 1, 0) == 0);
  assert(player_source_ui_unpaced(0, 0, 0, 0) == 0);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("Batch E", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1)); assert(tex_iniciar(64));
  gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  ajustes_shot_valor("relogioTelaLocal", 0);

  // 2. Full UTF-8 filename wrap: flags, CJK, accents, no separators, >2 KB input.
  fprintf(stderr, "step 2\n");
  const float aw = FOLHA_W - FOLHA_PAD_E - FOLHA_PAD_D - 2 * FOLHA_TXT;
  checkWrap("Ação.Épica.東京物語.🇯🇵🇧🇷🇺🇸.2160p.WEB-DL.DDP5.1.Atmos.DV.HDR10.HEVC-GRUPO.mkv", aw);
  checkWrap("東京物語東京物語東京物語東京物語東京物語東京物語東京物語東京物語東京物語東京物語東京物語東京物語東京物語", aw);
  checkWrap("🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵🇯🇵", 120);
  checkWrap("Averylongfilenamewithoutanyseparatorsatallthatmustwrapanyway.mkv", 1);
  { char huge[4096]; size_t o = 0;
    while (o + 4 < sizeof huge - 1) { memcpy(huge + o, "ção ", 5); o += 5; }
    huge[o] = 0;
    const ArquivoLayout *a = arquivoLayout(huge, aw);
    assert(a->n > 1 && strlen(a->texto) < sizeof a->texto);
    assert(utf8Valid(a->texto) && "bounded copy split a UTF-8 code point"); }

  // 3. [Cloud] prefix dedupe hides the filename only where the logo row
  //    really draws the whole description; the name never goes unseen.
  fprintf(stderr, "step 3\n");
  { static Stream c; char k[COPIA_CHAVE]; memset(&c, 0, sizeof c);
    snprintf(c.arquivo, sizeof c.arquivo, "[Cloud] Movie.2160p.mkv");
    snprintf(c.descricao, sizeof c.descricao, "☁️ Movie.2160p.mkv\n4K HDR");
    // Logos take the description's slot: the release name must still draw.
    c.dolbyVision = 1; assert(logosDa(&c, 0));
    assert(!strcmp(arquivoVisivel(&c, aw), "[Cloud] Movie.2160p.mkv") && "logo row hides description; name must stay");
    assert(!strcmp(arquivoDa(&c), "[Cloud] Movie.2160p.mkv"));
    copiaChave(&c, k, sizeof k); assert(!strcmp(k, "cloudmovie2160pmkv") && "copy key needs the real name");
    c.dolbyVision = 0;
    // MP4 also takes the slot (logo row draws the MP4 tag, not the text).
    snprintf(c.arquivo, sizeof c.arquivo, "[Cloud] Movie.mp4");
    snprintf(c.descricao, sizeof c.descricao, "☁️ Movie.mp4");
    c.mp4 = 1; assert(!strcmp(arquivoVisivel(&c, aw), "[Cloud] Movie.mp4")); c.mp4 = 0;
    // Plain description drawn whole on the logo row: no duplicate line.
    snprintf(c.arquivo, sizeof c.arquivo, "[Cloud] Movie.mkv");
    snprintf(c.descricao, sizeof c.descricao, "☁️ Movie.mkv");
    assert(!logosDa(&c, 0) && !arquivoVisivel(&c, aw)[0] && "visible description already shows the name");
    assert(!strcmp(arquivoDa(&c), "[Cloud] Movie.mkv"));
    // Same description clipped by the one-line ellipsis: the name returns.
    { size_t o = 0;
      while (o < 900) o += (size_t)snprintf(c.descricao + o, sizeof c.descricao - o, "Template field %zu ", o);
      snprintf(c.descricao + o, sizeof c.descricao - o, "Movie.mkv"); }
    assert(!strcmp(arquivoVisivel(&c, aw), "[Cloud] Movie.mkv") && "ellipsized description must not hide the name");
    snprintf(c.descricao, sizeof c.descricao, "Other.release.mkv");
    assert(!strcmp(arquivoVisivel(&c, aw), "[Cloud] Movie.mkv"));
    snprintf(c.arquivo, sizeof c.arquivo, "[Cloud]");
    assert(!strcmp(arquivoVisivel(&c, aw), "[Cloud]")); }

  // 4. Partial culling: a 96-line block clipped to a short window visits
  fprintf(stderr, "step 4\n");
  //    only the lines that intersect it (plus the 4 px guard).
  { char big[2047]; size_t o = 0;
    for (int i = 0; i < 96 && o + 20 < sizeof big; i++) o += (size_t)snprintf(big + o, sizeof big - o, "line %02d\n", i);
    int all = 0;
    linesVisited = 0; txt_addon_bloco(TXT_PG_FIM, big, 0, 0, 900, 200, 1, &all, -1.0e9f, 1.0e9f);
    int full = linesVisited;
    linesVisited = 0; txt_addon_bloco(TXT_PG_FIM, big, 0, -1000, 900, 200, 1, NULL, 0, 200);
    assert(full >= 90 && linesVisited > 0 && linesVisited <= 12 && "clipped block rasterized offscreen lines"); }

  // 5. Neighbour warm-up bounds under ASan: empty, single, last row, and
  fprintf(stderr, "step 5\n");
  //    focus moving while the sheet is still opening.
  AddonRemoto addon = {.nome = "AIOStreams", .url = "https://example.invalid/manifest.json", .ativo = 1};
  addons_definir_lista(&addon, 1);
  ajustes_shot_valor("fonteTextoLocal", 1);
  static Stream src[24];
  for (int i = 0; i < 24; i++) {
    memset(&src[i], 0, sizeof src[i]);
    snprintf(src[i].rotulo, sizeof src[i].rotulo, "AIOStreams 🇯🇵 Ação 東京 %d", i);
    snprintf(src[i].provedor, sizeof src[i].provedor, "AIOStreams");
    snprintf(src[i].url, sizeof src[i].url, "https://example.invalid/%d.mkv", i);
    snprintf(src[i].descricao, sizeof src[i].descricao,
             "🎞️ 2160p · DV · HDR10\n🔊 Atmos 🇯🇵🇧🇷🇺🇸 DUAL\n📦 Ação.Épica.東京物語.%d.mkv", i);
    src[i].altura = 2160; src[i].tamanhoMB = 9000; src[i].fileIdx = -1;
    src[i].badges = badges_bit("r-4k");
  }
  setList(src, 0); selosAquecerVizinhos(); for (int i = 0; i < 5; i++) frame();
  setList(src, 1); for (int i = 0; i < 3; i++) { frame(); selosAquecerVizinhos(); }
  setList(src, 24);
  for (int i = 0; i < 40; i++) {           // opening spring still running
    key(i & 1 ? SDLK_DOWN : SDLK_UP);
    frame(); selosAquecerVizinhos();
  }
  for (int i = 0; i < 60; i++) { key(SDLK_DOWN); frame(); selosAquecerVizinhos(); }
  rolagem = 1.0e6f; selosAquecerVizinhos();   // past the last row
  rolagem = -1.0e6f; selosAquecerVizinhos();  // before the first row
  for (int i = 0; i < 30; i++) frame();
  printf("PASS: neighbour warm-up stays in bounds (0/1/24 rows, last row, opening)\n");

  // 6. Top/Bottom badge row: inside its card, no overlap with the text.
  fprintf(stderr, "step 6\n");
  setList(src, 1);
  badgeLayout(0);
  badgeLayout(1);
  ajustes_shot_valor("sourceBadgePositionLocal", 0);

  // 7. Offline release labels (flags, aliases, subtitle sections) feed only
  fprintf(stderr, "step 7\n");
  //    the audio-language chip when the add-on declares no audio.
  { const char *json =
      "{\"streams\":["
      "{\"name\":\"AIO 1080p\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7\",\"title\":\"Movie.2024.1080p\",\"url\":\"https://example.invalid/a.mkv\"},"
      "{\"name\":\"AIO\",\"title\":\"Movie T\u00fcrk\u00e7e Dublaj 1080p\",\"url\":\"https://example.invalid/b.mkv\"},"
      "{\"name\":\"AIO\",\"title\":\"Movie 1080p ENG/TR SUBS\",\"url\":\"https://example.invalid/c.mkv\"},"
      "{\"name\":\"AIO\",\"title\":\"Movie 1080p\\n\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA Subs / \xF0\x9F\x87\xAB\xF0\x9F\x87\xB7\",\"url\":\"https://example.invalid/d.mkv\"}"
      "]}";
    Stream *out = NULL;
    int k = stream_extrair(json, "AIO", &out);
    assert(k == 4 && out);
    for (int i = 0; i < k; i++) printf("labels[%d]=\"%s\"\n", i, out[i].audioRotulos);
    assert(evidenciaAudio(&out[0], "tr") == 1 && "flag glued to 1080p must be a word boundary");
    assert(evidenciaAudio(&out[1], "tr") == 1 && "Dublaj alias");
    assert(evidenciaAudio(&out[2], "tr") != 1 && "ENG/TR SUBS are subtitles, not audio");
    assert(evidenciaAudio(&out[3], "de") == 1 && evidenciaAudio(&out[3], "fr") != 1 &&
           "Torrentio \"Subs /\" section must not count as audio");
    free(out); }
  printf("PASS: pacing table; UTF-8 wrap; [Cloud] dedupe; partial culling; badge rows\n");
  return 0;
}
