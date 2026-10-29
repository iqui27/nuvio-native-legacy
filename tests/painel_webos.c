// Offline C9 reproduction: 1080p desktop, 4K panel, unavailable LS2.
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int panelWidth, panelHeight, panelAvailable;
static SDL_bool panelResolution(int *w, int *h) {
  *w = panelWidth; *h = panelHeight;
  return panelWidth > 0 && panelHeight > 0 ? SDL_TRUE : SDL_FALSE;
}
static void *openLibrary(const char *name, int flags) {
  (void)flags;
  return name ? NULL : (void *)1;
}
static void *findSymbol(void *handle, const char *name) {
  (void)handle;
  return panelAvailable && !strcmp(name, "SDL_webOSGetPanelResolution")
    ? (void *)panelResolution : NULL;
}
static int closeLibrary(void *handle) { (void)handle; return 0; }
int SDL_GetDesktopDisplayMode(int display, SDL_DisplayMode *mode) {
  (void)display;
  mode->w = 1920; mode->h = 1080;
  return 0;
}
#define dlopen openLibrary
#define dlsym findSymbol
#define dlclose closeLibrary
#include "painel.h"
int main(int argc, char **argv) {
  assert(argc == 5);
  panelAvailable = atoi(argv[1]);
  panelWidth = atoi(argv[2]); panelHeight = atoi(argv[3]);
  assert(painel4k() == atoi(argv[4]));
  assert(painel4k() == atoi(argv[4])); // Cached detection must agree.
  return 0;
}
