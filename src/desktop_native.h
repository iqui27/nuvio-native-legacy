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
int desktop_native_menu(const DesktopMenuItem *items, int count, float x, float y);
void desktop_native_init(SDL_Window *window);
void desktop_native_sidebar(float fraction, float reveal);
void desktop_native_finish(void);
void desktop_native_appearance(const char *name);
int desktop_native_dark(void);
int desktop_native_swipes(void);
#else
#if !defined(NV_DESKTOP_UI) || !defined(_WIN32)
static inline void desktop_native_init(SDL_Window *window) { (void)window; }
#endif
static inline void desktop_native_sidebar(float fraction, float reveal) { (void)fraction; (void)reveal; }
static inline void desktop_native_finish(void) {}
static inline void desktop_native_appearance(const char *name) { (void)name; }
static inline int desktop_native_dark(void) { return 1; }
static inline int desktop_native_swipes(void) { return 1; }
#endif
#endif
