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
  AJ_MENU_TOPO, AJ_RAIL, AJ_RAIL_MODERNA, AJ_RAIL_BLUR,  // TV rails: the desktop has the native sidebar
  AJ_ICONE_APP,           // launcher icon swap exists on Android/webOS only
};
static int ajDesktopOculta(int op) {
  size_t i;
  for (i = 0; i < sizeof AJ_DESKTOP_OCULTAS / sizeof *AJ_DESKTOP_OCULTAS; i++)
    if (AJ_DESKTOP_OCULTAS[i] == op) return 1;
  return 0;
}
