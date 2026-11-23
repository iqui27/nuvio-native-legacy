#ifndef NV_DESKTOP_NATIVE_H
#define NV_DESKTOP_NATIVE_H
#include <SDL2/SDL.h>
#if defined(NV_DESKTOP_UI) && defined(_WIN32)
#include <SDL2/SDL_syswm.h>
#include <windows.h>
static inline void desktop_native_init(SDL_Window *window) {
  SDL_SysWMinfo info; SDL_VERSION(&info.version);
  if (!SDL_GetWindowWMInfo(window,&info)) return;
  HMODULE dwm = LoadLibraryA("dwmapi.dll");
  if (!dwm) return;
  typedef HRESULT (WINAPI *SetAttribute)(HWND,DWORD,LPCVOID,DWORD);
  SetAttribute set = (SetAttribute)(void *)GetProcAddress(dwm,"DwmSetWindowAttribute");
  if (set) { int backdrop=2, dark=1; set(info.info.win.window,38,&backdrop,sizeof backdrop); set(info.info.win.window,20,&dark,sizeof dark); }
  FreeLibrary(dwm);
}
#endif
#define NV_DESKTOP_PIXEL_WHEEL 0x4e565057u
// Tagged precise wheel events carry Cocoa lifecycle in wheel.x (not ticks).
enum {
  NV_WHEEL_BEGIN=1, NV_WHEEL_END=2, NV_WHEEL_CANCEL=4,
  NV_WHEEL_MOMENTUM=8, NV_WHEEL_SWIPE=16, NV_WHEEL_PHASED=32, NV_WHEEL_UNPHASED=64
};
// Menu tags are the caller's existing action IDs; Cocoa owns tracking/dismissal.
typedef struct {
  const char *title, *symbol;
  int action, checked, separator;
} DesktopMenuItem;
#if defined(NV_DESKTOP_UI) && defined(__APPLE__)
// Sources chrome is AppKit; its capture texture is the same native controls.
typedef struct {
  float x, y, w, h;
  int action, selected, enabled, kind; // 0 push, 1 filter, 2 close, 3 primary
  char title[160];
} DesktopSourceControl;
unsigned desktop_native_sources(const DesktopSourceControl *controls, int count, void (*activate)(int),
                                float x, float y, float w, float h, float scale, float alpha);
#include "gfx.h"
void desktop_native_profile_text(const char *text, float cx, float y, float size, int bold, float ink, float alpha);
void desktop_native_profile_button(GfxRect rect, const char *title, int enabled, int selected, float alpha,
                                  void (*activate)(int, int), int tag, int slot);
#define NV_DESKTOP_NATIVE_CONTROLS 1
// AppKit draws the controls into cached GL textures; SDL rows own hit testing.
// kind: 0 switch, 1 popup, 2 button, 3 search, 4 label (state = font points).
// Coordinates use the caller's canvas.
int desktop_native_control(int slot, int kind, const char *title, int state, int enabled,
                           float x, float y, float w, float h, float alpha);
int desktop_native_menu(const DesktopMenuItem *items, int count, float x, float y);
void desktop_native_init(SDL_Window *window);
void desktop_native_sidebar(float fraction, float reveal);
// Query field geometry is normalized to the content view; returns 0 for GL proof.
int desktop_native_search(const char *query, float x, float y, float w, float h);
void desktop_native_finish(void);
void desktop_native_appearance(const char *name);
int desktop_native_fullscreen(void);
void desktop_native_capture_fullscreen(int width, int height);
int desktop_native_dark(void);
int desktop_native_swipes(void);
// NSAlert sheet on the window: buttons translated, [0] default (destructive),
// last = Escape; `text` adds a field. 0 when shown; the answer arrives once via
// desktop_native_alert_result (-1 while pending; else button index + field text).
int desktop_native_alert(const char *title, const char *message, const char *const *buttons, int count,
                         int destructive, const char *text, int secure);
int desktop_native_alert_result(char *text, int size);
#else
#if !defined(NV_DESKTOP_UI) || !defined(_WIN32)
static inline void desktop_native_init(SDL_Window *window) { (void)window; }
#endif
static inline int desktop_native_control(int slot, int kind, const char *title, int state, int enabled,
                                         float x, float y, float w, float h, float alpha) {
  (void)slot; (void)kind; (void)title; (void)state; (void)enabled;
  (void)x; (void)y; (void)w; (void)h; (void)alpha; return 0;
}
static inline void desktop_native_sidebar(float fraction, float reveal) { (void)fraction; (void)reveal; }
static inline int desktop_native_search(const char *q,float x,float y,float w,float h) { (void)q;(void)x;(void)y;(void)w;(void)h;return 0; }
static inline void desktop_native_finish(void) {}
static inline void desktop_native_appearance(const char *name) { (void)name; }
static inline int desktop_native_fullscreen(void) { return 0; }
static inline void desktop_native_capture_fullscreen(int width, int height) { (void)width; (void)height; }
static inline int desktop_native_dark(void) { return 1; }
static inline int desktop_native_swipes(void) { return 1; }
#endif
#endif
