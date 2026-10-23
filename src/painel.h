// Physical panel discovery before creating the UI surface. SDL's desktop mode
// often describes a 1080p graphics plane even on a UHD TV.
#ifndef NV_PAINEL_H
#define NV_PAINEL_H
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(NV_WEBOS) || defined(NV_TPK)
#include <dlfcn.h>
#endif
#ifdef NV_WEBOS
#include "js.h"
#include <unistd.h>
static const char *(*painelPayload)(void *);
static int painelResposta(void *bus, void *msg, void *data) {
  char resolution[32] = "", uhd[16] = "";
  int *size = data;
  (void)bus;
  const char *payload = painelPayload(msg);
  js_texto(payload, NULL, "panelResolution", resolution, sizeof resolution);
  js_bruto(payload, NULL, "UHD", uhd, sizeof uhd);
  if (!strcmp(resolution, "UHD")) { size[0] = 3840; size[1] = 2160; }
  else if (!strcmp(resolution, "8K")) { size[0] = 7680; size[1] = 4320; }
  else if (!strcmp(resolution, "FHD")) { size[0] = 1920; size[1] = 1080; }
  else if (!strcmp(resolution, "HD")) { size[0] = 1280; size[1] = 720; }
  else (void)sscanf(resolution, "%dx%d", &size[0], &size[1]);
  if ((!size[0] || !size[1]) && (!strcmp(uhd, "true") || !strcmp(uhd, "\"true\"") ||
                               !strcmp(uhd, "1") || !strcmp(uhd, "\"1\""))) {
    size[0] = 3840; size[1] = 2160;
  }
  size[2] = 1;
  return 1;
}
// Separate local LS2 context: the player's bus needs an exported window, which
// does not exist yet. Never initialize the player merely to inspect the panel.
static void painelWebos(int *size) {
  void *lib = dlopen("libluna-service2.so.3", RTLD_NOW);
  if (!lib) lib = dlopen("libluna-service2.so", RTLD_NOW);
  void *glib = dlopen("libglib-2.0.so.0", RTLD_NOW);
  int (*reg)(const char *, void **, void *) = NULL;
  int (*attach)(void *, void *, void *) = NULL;
  int (*call)(void *, const char *, const char *, int (*)(void *, void *, void *),
              void *, unsigned long *, void *) = NULL;
  int (*unreg)(void *, void *) = NULL;
  void *(*contextNew)(void) = NULL;
  void (*contextFree)(void *) = NULL;
  int (*iterate)(void *, int) = NULL;
  void (*errorInit)(void *) = NULL, (*errorFree)(void *) = NULL;
  void *bus = NULL, *context = NULL;
  union { void *align; char bytes[256]; } error;
  unsigned long token = 0;
#define PANEL_SYMBOL(handle, target, name) *(void **)&target = dlsym(handle, name)
  if (!lib || !glib) goto done;
  PANEL_SYMBOL(lib, reg, "LSRegister");
  PANEL_SYMBOL(lib, attach, "LSGmainContextAttach");
  PANEL_SYMBOL(lib, call, "LSCallOneReply");
  PANEL_SYMBOL(lib, unreg, "LSUnregister");
  PANEL_SYMBOL(lib, painelPayload, "LSMessageGetPayload");
  PANEL_SYMBOL(lib, errorInit, "LSErrorInit");
  PANEL_SYMBOL(lib, errorFree, "LSErrorFree");
  PANEL_SYMBOL(glib, contextNew, "g_main_context_new");
  PANEL_SYMBOL(glib, contextFree, "g_main_context_unref");
  PANEL_SYMBOL(glib, iterate, "g_main_context_iteration");
#undef PANEL_SYMBOL
  if (!reg || !attach || !call || !unreg || !painelPayload || !contextNew ||
      !contextFree || !iterate || !errorInit || !errorFree) goto done;
  memset(&error, 0, sizeof error); errorInit(&error);
  context = contextNew();
  char name[96];
  snprintf(name, sizeof name, "com.webos.media.client.nuvio.panel.%d", (int)getpid());
  if (context && reg(name, &bus, &error) && attach(bus, context, &error) &&
      call(bus, "luna://com.webos.service.tv.systemproperty/getSystemInfo",
           "{\"keys\":[\"panelResolution\",\"UHD\"]}", painelResposta, size, &token, &error)) {
    Uint32 start = SDL_GetTicks();
    while (!size[2] && SDL_GetTicks() - start < 300) {
      iterate(context, 0);
      if (!size[2]) SDL_Delay(1);
    }
  }
  errorFree(&error);
  memset(&error, 0, sizeof error); errorInit(&error);
  if (bus) unreg(bus, &error); // Cancels a pending reply before stack data expires.
  errorFree(&error);
  if (context) contextFree(context);
done:
  if (glib) dlclose(glib);
  if (lib) dlclose(lib);
}
#endif
static int painel4k(void) {
  static int read, width, height;
  if (!read) {
    const char *source = "SDL desktop (fallback)";
    const char *size = getenv("NUVIO_PANEL_SIZE");
    if (size && sscanf(size, "%dx%d", &width, &height) == 2 && width > 0 && height > 0) {
      const char *origin = getenv("NUVIO_PANEL_SOURCE");
      source = origin && *origin ? origin : "platform panel";
    } else {
      width = height = 0;
#ifdef NV_WEBOS
      int panel[3] = {0};
      // LG's SDL exposes the physical panel even when LS2 denies the query.
      SDL_bool (*getPanel)(int *, int *) = NULL;
      void *process = dlopen(NULL, RTLD_NOW);
      if (process) *(void **)&getPanel = dlsym(process, "SDL_webOSGetPanelResolution");
      if (getPanel && getPanel(&panel[0], &panel[1]) && panel[0] > 0 && panel[1] > 0)
        source = "SDL_webOSGetPanelResolution";
      else {
        panel[0] = panel[1] = 0;
        painelWebos(panel);
        if (panel[0] > 0 && panel[1] > 0) source = "webOS panelResolution";
      }
      if (process) dlclose(process);
      width = panel[0]; height = panel[1];
#elif defined(NV_TPK)
      void *lib = dlopen("libcapi-system-info.so.0", RTLD_NOW);
      if (!lib) lib = dlopen("libcapi-system-info.so", RTLD_NOW);
      int (*get)(const char *, int *) = NULL;
      if (lib) *(void **)&get = dlsym(lib, "system_info_get_platform_int");
      if (get && get("http://tizen.org/feature/screen.width", &width) == 0 &&
          get("http://tizen.org/feature/screen.height", &height) == 0 && width > 0 && height > 0)
        source = "Tizen system info screen";
      else width = height = 0;
      if (lib) dlclose(lib);
#endif
    }
    if (width <= 0 || height <= 0) width = height = 0;
    SDL_DisplayMode mode;
    int w = width, h = height;
    if ((!w || !h) && SDL_GetDesktopDisplayMode(0, &mode) == 0) { w = mode.w; h = mode.h; }
    printf("[gpu-nivel] painel %dx%d (%s)\n", w, h, source);
    read = 1;
  }
  if (width > 0 && height > 0) return width >= 3840 && height >= 2160;
  SDL_DisplayMode mode;
  return SDL_GetDesktopDisplayMode(0, &mode) == 0 && mode.w >= 3840 && mode.h >= 2160;
}
#endif
