// The 2.0.9 release card. Upstream changes are in docs/releases/2.0.9/FEATURES.md;
// this card adds the work merged into release/2.0.9. English only for now.
static void visual209(GfxRect p, float t, float a, int c);
static void visual209a(GfxRect p, float t, float a) { visual209(p, t, a, 0); }
static void visual209b(GfxRect p, float t, float a) { visual209(p, t, a, 1); }
static void visual209c(GfxRect p, float t, float a) { visual209(p, t, a, 2); }
static void visual209d(GfxRect p, float t, float a) { visual209(p, t, a, 3); }
static void visual209e(GfxRect p, float t, float a) { visual209(p, t, a, 4); }
static const NovItem ITENS_209[] = {
  { .grupo = "Destaques", .icone = "aj_star", .nome = "Your ratings",
    .frase = "Rate titles and episodes with stars and quick tags, with undo. Ratings stay with your profile and can go to Trakt and Simkl.", .linhas = 6 },
  { .icone = "aj_captions", .nome = "New subtitle engine",
    .frase = "ASS and SSA subtitles keep their fonts, colours, positions and effects, drawn by libass. Arabic and Hebrew get the right face, and Android draws PGS too.", .linhas = 8, .plataformas = NOV_LG | NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_images", .nome = "Thumbnails made on the TV",
    .frase = "The seek bar shows the scene without a Seekr key. The TV makes the thumbnails while the video plays: first where you seek, then near the current position, then the whole film.", .linhas = 8, .plataformas = NOV_TPK | NOV_LG | NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_audio-lines", .nome = "Audio from another source",
    .frase = "Keep the picture of one source and take the audio from another version of the same title, such as a dub. The next episodes of the series keep the same version and track.", .linhas = 8, .plataformas = NOV_TPK | NOV_LG | NOV_OUTRAS },
  { .icone = "aj_flame", .nome = "TorBox burn-in",
    .frase = "With TorBox Pro, turn on Burn in ASS with TorBox and TorBox draws embedded ASS subtitles into the picture. Press OK to switch when it is ready.", .linhas = 7, .plataformas = NOV_TPK | NOV_LG | NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_pause", .nome = "Pause panel",
    .frase = "After four seconds paused, see the episode synopsis, your ratings and Trakt comments, with the cast if you want it.", .linhas = 6 },
  { .icone = "aj_archive", .nome = "TorBox Airlock",
    .frase = "Keep or remove a source in TorBox's permanent Airlock storage right from its file list.", .linhas = 4 },
  { .icone = "aj_music", .nome = "Soundtracks",
    .frase = "Browse the soundtrack of a film or season, save albums to your Library and open them on your phone with a QR code.", .linhas = 6 },
  { .grupo = "Mais novidades", .icone = "aj_tv-minimal-play", .nome = "Trailerio trailers",
    .frase = "Automatic trailers try Trailerio after Apple TV and IMDb. Left and Right seek 5 s, and the Trailer button keeps the playing trailer and goes full screen.", .linhas = 7, .plataformas = NOV_LG | NOV_OUTRAS },
  { .icone = "aj_keyboard", .nome = "Samsung keyboard",
    .frase = "Set Keyboard to From the system to type with the Samsung keyboard. The arrows move the cursor, and passwords stay hidden.", .linhas = 6, .plataformas = NOV_TPK | NOV_OUTRAS },
  { .icone = "aj_monitor", .nome = "Match Frame Rate",
    .frase = "The TV switches to the frame rate of the video, always or only if seamless.", .linhas = 4, .plataformas = NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_headphones", .nome = "DTS in stereo",
    .frase = "Set DTS converted to Stereo to mix DTS tracks down to two channels, with the dialogue on both sides.", .linhas = 5, .plataformas = NOV_ANDROID | NOV_OUTRAS },
  { .icone = "aj_list-video", .nome = "Chapters",
    .frase = "MKV chapters show on the seek bar, and Up opens a row of chapters with pictures. Choose in Chapters in the player which titles get them.", .linhas = 6 },
  { .icone = "aj_monitor-play", .nome = "HLS (experimental)",
    .frase = "Plays the DTS and Dolby Vision P7 conversions over HLS, with Dolby Digital sound. The TV picks fMP4 or MPEG-TS by itself.", .linhas = 6, .plataformas = NOV_LG | NOV_OUTRAS },
  { .icone = "aj_play", .nome = "Continue watching",
    .frase = "New episode and New season badges, newly aired episodes move to the front, and resume opens the episode straight away with its source and audio.", .linhas = 7 },
  { .icone = "aj_user-round", .nome = "Profiles and PIN",
    .frase = "TV and profile settings are separate. Protect administration with a PIN, copy or restore a profile's settings, and start in the last profile if you want.", .linhas = 7 },
  { .icone = "aj_type", .nome = "Your own fonts",
    .frase = "Import interface and subtitle fonts separately, and preview the interface font.", .linhas = 4 },
  { .icone = "aj_sliders-horizontal", .nome = "Format row",
    .frase = "Show the player's format as pills, text, marks or badges, under the title or in the corner, with or without the audio codec.", .linhas = 6 },
  { .icone = "aj_fast-forward", .nome = "Skip buttons",
    .frase = "Show or hide the intro, recap and credits skip buttons one by one.", .linhas = 4 },
  { .icone = "aj_users", .nome = "Guest stars",
    .frase = "The episode panel in the player lists the episode's guest stars.", .linhas = 4 },
  { .icone = "aj_library", .nome = "See all",
    .frase = "A See all card at the end of the Library row on Home opens the full Library.", .linhas = 4 },
  { .icone = "aj_contrast", .nome = "Smoother gradients",
    .frase = "Turn on gradient smoothing to reduce banding over the video.", .linhas = 4 },
  { .icone = "aj_languages", .nome = "Arabic",
    .frase = "The interface uses Noto Sans Arabic with real bold, and Settings text lines up.", .linhas = 4 },
  { .grupo = "Correções", .icone = "aj_captions", .nome = "Subtitles",
    .frase = "The subtitles tab opens faster, opacity moves in 5% steps, Greek and Turkish subtitles read correctly, and subtitles keep up when memory runs low.", .linhas = 7 },
  { .icone = "aj_rotate-ccw-clock", .nome = "Resume",
    .frase = "Continue Watching opens the saved source sooner and waits for the right episode data. Switching sources keeps the episode's progress.", .linhas = 6 },
  { .icone = "aj_puzzle", .nome = "Sources",
    .frase = "Long signed links keep playing, manual source choice applies to every play, and more add-ons answer when their manifest is unclear.", .linhas = 6 },
  { .icone = "aj_tv", .nome = "Live TV",
    .frase = "Hold the arrows to scroll the guide, live add-on catalogs play again, and live streams on Android reconnect without stopping.", .linhas = 6 },
  { .icone = "aj_house", .nome = "Navigation",
    .frase = "Up reaches the clock and the Library menu, Home rows keep their focus, and a series page focuses the next episode.", .linhas = 6 },
  { .icone = "aj_gauge", .nome = "Speed",
    .frase = "Large source lists open faster, and artwork loads better on TCL and low-memory TVs.", .linhas = 4 },
  { .icone = "aj_sliders-horizontal", .nome = "Samsung .tpk",
    .frase = "Posters show again, the chosen subtitle survives seeks, add-on sources that need headers play through the app, and the Tizen 4/5 package is back.", .linhas = 7, .plataformas = NOV_TPK | NOV_OUTRAS },
};
static const NovCena CENAS_209[] = {
  { "Destaques", "Your ratings", visual209a, 5.5f, 0 },
  { "Destaques", "New subtitle engine", visual209b, 5.5f, NOV_LG | NOV_ANDROID | NOV_OUTRAS },
  { "Destaques", "Thumbnails made on the TV", visual209c, 5.5f, NOV_TPK | NOV_LG | NOV_ANDROID | NOV_OUTRAS },
  { "Destaques", "Audio from another source", visual209d, 5.5f, NOV_TPK | NOV_LG | NOV_OUTRAS },
  { "Destaques", "TorBox burn-in", visual209e, 5.5f, NOV_TPK | NOV_ANDROID | NOV_OUTRAS },
};
static const int ARTES_209[] = { 2, 4, 7, 9 };
static const NovConteudo NOV_ATUAL = {
  .versao = "2.0.9", .arquivo = "novidades-209.txt",
  .subtitulo = NULL,
  .itens = ITENS_209, .nItens = (int)(sizeof ITENS_209 / sizeof *ITENS_209),
  .cenas = CENAS_209, .nCenas = (int)(sizeof CENAS_209 / sizeof *CENAS_209),
  .artes = ARTES_209, .nArtes = 4,
};

// Small, static feature mockups. Only their entrance moves; no live video.
static void visual209(GfxRect p, float t, float a, int c) {
  float e = ajustes_animacoes_reduzidas() ? 1 : sai3(t / .45f);
  float x = p.x + 40, y = p.y + 100 + (1 - e) * 16;
  float w = p.w - 80;
  const char *icons[] = { "aj_star", "aj_captions", "aj_images", "aj_audio-lines", "aj_flame" };
  novArte(ARTES_209[c], (GfxRect){p.x, p.y, p.w, p.h}, PV_RAIO, .68f * a);
  gfx_rect(p, 0, GFX_VEU_CARD, 0, 0, 0, PV_RAIO / p.h, .025f, .03f, .045f, a);
  float vs = fminf(1, (p.h - 230) / 540);
  gfx_transformar(p.x, p.y, vs, 0, 0);
  gfx_icone((GfxRect){x, p.y + 36, 40, 40}, icons[c], .85f, .90f, 1, a);
  if (c == 0) {
    // A poster with five stars, four lit, and two tag pills.
    novArte(2, (GfxRect){x, y, w * .42f, 330}, 18, a * e);
    float sx = x + w * .48f;
    for (int j = 0; j < 5; j++) {
      float lit = j < 4 ? 1 : .35f;
      gfx_icone((GfxRect){sx + j * 54, y + 40, 44, 44}, "aj_star", 1, .84f, .35f, a * e * lit);
    }
    for (int j = 0; j < 2; j++)
      gfx_rect((GfxRect){sx + j * 130, y + 120, 116, 36}, 0, GFX_VEU_CARD, 0, 0, 0, .5f, .5f, .9f, .9f, .35f * a * e);
    for (int j = 0; j < 3; j++) gfx_cor((GfxRect){sx, y + 200 + j * 16, w * (.45f - j * .08f), 5}, .5f, 1, 1, 1, .3f * a);
  } else if (c == 1) {
    // A frame with a styled, placed subtitle: outlined top line and a coloured bottom line.
    novArte(4, (GfxRect){x, y, w, 330}, 20, a * e);
    gfx_cor((GfxRect){x + w * .30f, y + 40, w * .40f, 10}, .5f, 1, .86f, .40f, .9f * a * e);
    gfx_cor((GfxRect){x + w * .18f, y + 260, w * .64f, 12}, .5f, 1, 1, 1, .95f * a * e);
    gfx_cor((GfxRect){x + w * .26f, y + 284, w * .48f, 12}, .5f, .55f, .85f, 1, .95f * a * e);
  } else if (c == 2) {
    // A seek bar with three thumbnails above it, the middle one in focus.
    for (int j = 0; j < 3; j++) {
      float cw = (w - 32) / 3, big = j == 1 ? 20 : 0;
      GfxRect r = {x + j * (cw + 16), y + 120 - big, cw, 170 + big};
      novArte(ARTES_209[j], r, 14, a * e * (j == 1 ? 1 : .7f));
    }
    gfx_cor((GfxRect){x, y + 330, w, 6}, .5f, 1, 1, 1, .3f * a);
    gfx_cor((GfxRect){x, y + 330, w * .5f, 6}, .5f, .8f, .88f, 1, a);
  } else if (c == 4) {
    // A frame with the subtitle burned into the picture and an OK hint.
    novArte(2, (GfxRect){x, y, w, 300}, 20, a * e);
    gfx_cor((GfxRect){x + w * .22f, y + 236, w * .56f, 12}, .5f, 1, 1, 1, .95f * a * e);
    gfx_rect((GfxRect){x + w / 2 - 70, y + 330, 140, 40}, 0, GFX_VEU_CARD, 0, 0, 0, .5f, .5f, .9f, .9f, .45f * a * e);
    gfx_icone((GfxRect){x + w / 2 - 16, y + 334, 32, 32}, "aj_check", 1, 1, 1, a * e);
  } else {
    // Picture from one source, audio from another: two cards joined by a waveform.
    novArte(7, (GfxRect){x, y, w * .58f, 300}, 18, a * e);
    novArte(9, (GfxRect){x + w * .64f, y + 70, w * .36f, 160}, 14, .8f * a * e);
    for (int j = 0; j < 17; j++) {
      float h = 12 + (j % 5) * 12;
      gfx_cor((GfxRect){x + j * (w / 17), y + 355 - h / 2, 8, h}, .4f, .75f, .84f, 1, .8f * a);
    }
  }
  gfx_sem_transformar();
}
