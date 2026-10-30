#ifndef NV_WIN_DLFCN_H
#define NV_WIN_DLFCN_H
#include <windows.h>
#define RTLD_NOW 0
#define RTLD_LOCAL 0
static inline void *dlopen(const char *p, int f) { (void)f; return p ? (void *)LoadLibraryA(p) : (void *)GetModuleHandleA(NULL); }
static inline void *dlsym(void *h, const char *p) { return (void *)GetProcAddress((HMODULE)h, p); }
static inline int dlclose(void *h) { return FreeLibrary((HMODULE)h) ? 0 : -1; }
static inline const char *dlerror(void) { return "Windows DLL unavailable"; }
#endif
