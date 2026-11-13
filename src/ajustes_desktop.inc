// Settings that mean nothing on a computer (table: docs/desktop-settings.md).
// ajOpcaoDisponivel hides them on NV_DESKTOP_UI; TV builds never see this list.
static const int AJ_DESKTOP_OCULTAS[] = {
  AJ_INTERFACE_MODO,      // TV/Mobile: the window width picks the layout
  AJ_TECLADO_ANDROID,     // on-screen keyboard choice: desktop types on the keyboard
  AJ_RESOLUCAO,           // TV output resolution: the window/Retina size decides
  AJ_AMPLIAR_UI, AJ_AMPLIACAO_ALGO, AJ_AMPLIACAO_NITIDEZ,   // TV panel upscaling
  AJ_NAV_RAPIDA,          // D-pad auto-repeat
  AJ_DICASCONTROLE,       // remote-control tips
  AJ_ESMAECER, AJ_DESCANSO_ESTILO, AJ_DESCANSO_FONTE,       // TV screensaver
  AJ_DTS_AC3,             // LG-only DTS conversion; mpv decodes DTS
  AJ_DV_MKV, AJ_DV7,      // LG Dolby Vision conversion
  AJ_CACHE_SEEK,          // TV player disk cache
  AJ_BUFFER,              // webOS read-ahead; mpv keeps its own cache
  AJ_BUFFER_NATIVE_CONNECTIONS, AJ_BUFFER_CONVERSION_CONNECTIONS, // webOS HTTP lanes
  AJ_MENU_TOPO, AJ_RAIL, AJ_RAIL_MODERNA, AJ_RAIL_BLUR,  // TV rails: the desktop has the native sidebar
  AJ_ICONE_APP,           // launcher icon swap exists on Android/webOS only
};
// Source (pt) descriptions that name a TV platform or the remote: hidden on the
// desktop layout, kept below the breakpoint (TV/mobile text untouched).
static int ajDesktopTextoTv(const char *s) {
  static const char *const tv[] = { "webOS", "Tizen", "Samsung", "LG", "VIDAA", "Android", "do controle", "controle remoto" };
  size_t i;
  if (!s || !layout_desktop_ativo()) return 0;
  for (i = 0; i < sizeof tv / sizeof *tv; i++) if (strstr(s, tv[i])) return 1;
  return 0;
}
// End of the sentence that starts at s: ". ", "! ", "? ", "。" or the end.
static size_t ajFraseFim(const char *s) {
  size_t i;
  for (i = 0; s[i]; i++) {
    if ((s[i] == '.' || s[i] == '!' || s[i] == '?') && (s[i + 1] == ' ' || !s[i + 1])) return i + 1;
    if ((unsigned char)s[i] == 0xE3 && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x82) return i + 3;
  }
  return i;
}
// `s` (pt source) without the sentences that name a TV platform or the remote.
// None does: `s` itself. Otherwise the kept sentences, already translated (the
// translation drops the same sentence index; i18n passes the result through);
// "" when nothing is left.
static const char *ajDesktopSemTv(const char *s) {
  static char b[2048];
  char f[1024];
  unsigned long long tv = 0;
  const char *t, *p;
  size_t n, o = 0;
  int k, np = 0, nt = 0;
  if (!ajDesktopTextoTv(s)) return s;
  for (p = s; *p && np < 64; np++) {
    while (*p == ' ') p++;
    n = ajFraseFim(p); snprintf(f, sizeof f, "%.*s", (int)n, p); p += n;
    if (ajDesktopTextoTv(f)) tv |= 1ull << np;
  }
  t = i18n(s);
  for (p = t; *p; nt++) { while (*p == ' ') p++; p += ajFraseFim(p); }
  b[0] = 0;
  for (p = t, k = 0; *p; k++) {
    while (*p == ' ') p++;
    n = ajFraseFim(p); snprintf(f, sizeof f, "%.*s", (int)n, p); p += n;
    if (!f[0] || (nt == np ? (tv >> k) & 1 : ajDesktopTextoTv(f))) continue;
    o += (size_t)snprintf(b + o, o < sizeof b ? sizeof b - o : 0, "%s%s", o ? " " : "", f);
    if (o >= sizeof b) break;
  }
  return b;
}
static int ajDesktopOculta(int op) {
  size_t i;
  for (i = 0; i < sizeof AJ_DESKTOP_OCULTAS / sizeof *AJ_DESKTOP_OCULTAS; i++)
    if (AJ_DESKTOP_OCULTAS[i] == op) return 1;
  return 0;
}
