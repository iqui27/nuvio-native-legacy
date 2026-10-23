/* Execute the production HDR includes against a small EGL/native-window driver. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#define SDL_VIDEO_DRIVER_ANDROID 1
#define SDL_FALSE 0
#define SDL_EGL_DISPLAY_EXTENSION 0
#define EGL_NO_SURFACE 0
#define EGL_NONE 0x3038
#define EGL_EXTENSIONS 0x3055
#define EGL_RED_SIZE 1
#define EGL_GREEN_SIZE 2
#define EGL_BLUE_SIZE 3
#define EGL_ALPHA_SIZE 4
#define EGL_CONFIG_ID 5
#define EGL_COLOR_COMPONENT_TYPE_EXT 6
#define EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT 7
#define EGL_SMPTE2086_DISPLAY_PRIMARY_RX_EXT 0x3341
#define EGL_CTA861_3_MAX_CONTENT_LIGHT_LEVEL_EXT 0x3360
#define ADATASPACE_BT2020_PQ 163971072
#define ADATASPACE_BT2020_ITU_PQ 163971073
#define ADATASPACE_BT2020_HLG 3
#define ADATASPACE_SCRGB_LINEAR 4
#define STANDARD_BT2020 8
#define TRANSFER_LINEAR 16
#define RANGE_EXTENDED 32
#define ADATASPACE_UNKNOWN 0
#define _THIS Device *_this
#define SDL_strstr strstr
#define SDL_GL_GetCurrentContext() ((void *)1)
typedef int EGLint;
typedef int EGLSurface;
typedef void *SDL_GLContext;
typedef struct { int format; int32_t space; } ANativeWindow;
typedef ANativeWindow *NativeWindowType;
typedef struct {
    int egl_display, egl_config;
    const char *(*eglQueryString)(int, int);
    int (*eglGetConfigAttrib)(int, int, int, int *);
    EGLSurface (*eglCreateWindowSurface)(int, int, ANativeWindow *, int *);
    int (*eglDestroySurface)(int, EGLSurface);
    int (*eglGetError)(void);
} Egl;
typedef struct { Egl *egl_data; } Device;
typedef int (*PFNEGLSURFACEATTRIBPROC)(int, EGLSurface, int, int);
typedef int (*PFNEGLQUERYSURFACEPROC)(int, EGLSurface, int, int *);
typedef int SDL_bool;
static int enabled = 1, extensions = 1, attribCount, smpteCount, ctaCount;
static int createCount, destroyCount, currentCount, releaseCount, logs, tagCount;
static int red = 10, alpha = 2, failAttrib, failNative, failCreate, failCurrent;
static const char *error = "EGL_BAD_SURFACE";
static ANativeWindow nativeWindow;
static int SDL_GetHintBoolean(const char *s, int fallback) { (void)s; (void)fallback; return enabled; }
static void SDL_Log(const char *s, ...) { if (strstr(s, "metadata egl-smpte=")) ++logs; }
static const char *SDL_GetError(void) { return error; }
static int SDL_SetError(const char *s, ...) { error = s; return -1; }
static void SDL_SetHint(const char *s, const char *v) { (void)s; (void)v; }
static int android_get_device_api_level(void) { return 34; }
static int SDL_EGL_HasExtension(Device *d, int scope, const char *name) { (void)d; (void)scope; (void)name; return extensions; }
static const char *queryString(int d, int key) { (void)d; (void)key; return "mock"; }
static int configAttrib(int d, int c, int key, int *value) {
    (void)d; (void)c;
    *value = key == EGL_CONFIG_ID ? 42 : key == EGL_ALPHA_SIZE ? alpha : red;
    return 1;
}
static int querySurface(int d, EGLSurface s, int key, int *value) { (void)d; (void)s; (void)key; *value = 42; return 1; }
static int surfaceAttrib(int d, EGLSurface s, int key, int value) {
    static const int expected[] = {35400,14600,8500,39850,6550,2300,15635,16450,50000000,5,1000,400};
    int index = key >= 0x3360 ? key - 0x3360 + 10 : key - 0x3341;
    (void)d; assert(s > 0); assert(index >= 0 && index < 12);
    assert(value == expected[index]); ++attribCount;
    return !failAttrib;
}
static void *SDL_EGL_GetProcAddress(Device *d, const char *name) { (void)d; return !strcmp(name, "eglSurfaceAttrib") ? (void *)surfaceAttrib : (void *)querySurface; }
static int eglError(void) { return 0x3004; }
static EGLSurface createSurface(int d, int c, ANativeWindow *w, int *attrs) { (void)d; (void)c; (void)w; (void)attrs; ++createCount; return failCreate ? 0 : createCount; }
static int destroySurface(int d, EGLSurface s) { (void)d; (void)s; ++destroyCount; return 1; }
static int setSpace(ANativeWindow *w, int32_t space) { w->space = space; ++tagCount; return 0; }
static int32_t getSpace(ANativeWindow *w) { return w->space; }
static int ANativeWindow_getFormat(ANativeWindow *w) { return w->format; }
static void Android_SetFormat(int wanted, int got) { (void)wanted; (void)got; }
#include "../android/app/src/main/cpp/hdr_native.inc"
static int setSmpte(ANativeWindow *w, const NvSmpte2086 *m) {
    assert(w == &nativeWindow); assert(m->red.x == .708f && m->green.y == .797f);
    assert(m->blue.x == .131f && m->white.x == .3127f && m->white.y == .3290f);
    assert(m->maxLuminance == 1000.f && m->minLuminance == .0001f); ++smpteCount; return failNative ? -EINVAL : 0;
}
static int setCta(ANativeWindow *w, const NvCta8613 *m) { assert(w == &nativeWindow); assert(m->maxCLL == 1000.f && m->maxFALL == 400.f); ++ctaCount; return failNative ? -EINVAL : 0; }
static EGLSurface SDL_EGL_CreateSurface(Device *_this, ANativeWindow *nw) {
    int attribs[9] = {EGL_NONE}, attr = 1, format_got, format_wanted = 43;
    EGLSurface surface;
#include "../android/app/src/main/cpp/hdr_surface.inc"
    return createSurface(0, 0, nw, attribs);
}
typedef struct { EGLSurface egl_surface; ANativeWindow *native_window; } SDL_WindowData;
typedef struct { void *driverdata; } SDL_Window;
static int SDL_EGL_MakeCurrent(Device *d, EGLSurface s, SDL_GLContext c) { (void)d; if (s) { assert(c); ++currentCount; } return failCurrent ? -1 : 0; }
static void SDL_EGL_DestroySurface(Device *d, EGLSurface s) { destroySurface(d->egl_data->egl_display, s); }
static void ANativeWindow_release(ANativeWindow *w) { assert(w == &nativeWindow); ++releaseCount; }
static ANativeWindow *Android_JNI_GetNativeWindow(void) { return &nativeWindow; }
static int recover(Device *_this, SDL_Window *window, int retval) {
#include "../android/app/src/main/cpp/hdr_recover.inc"
    return retval;
}
int main(void) {
    Egl egl = {0,42,queryString,configAttrib,createSurface,destroySurface,eglError};
    Device device = {&egl};
    SDL_WindowData data = {0, &nativeWindow}; SDL_Window window = {&data};
    /* Mark optional symbol lookup complete, then install the fake native ABI. */
    nvHdrLoad(); nvSetDataspace = setSpace; nvGetDataspace = getSpace;
    nvSetSmpte2086 = setSmpte; nvSetCta8613 = setCta;
    data.egl_surface = SDL_EGL_CreateSurface(&device, &nativeWindow);
    assert(data.egl_surface && attribCount == 12 && smpteCount == 1 && ctaCount == 1);
    nvHdrAfterSwap(data.egl_surface); nvHdrAfterSwap(data.egl_surface); assert(logs == 1);
    assert(recover(&device, &window, -1) == 0);
    assert(createCount == 2 && destroyCount == 1 && currentCount == 1 && releaseCount == 1);
    assert(attribCount == 24 && smpteCount == 2 && ctaCount == 2 && tagCount == 5);
    nvHdrAfterSwap(data.egl_surface); assert(logs == 2 && tagCount == 6);
    failAttrib = failNative = 1;
    assert(SDL_EGL_CreateSurface(&device, &nativeWindow));
    assert(nvEglSmpteRc == 0x3004 && nvEglCtaRc == 0x3004);
    assert(nvNativeSmpteRc == -EINVAL && nvNativeCtaRc == -EINVAL);
    failAttrib = failNative = 0; extensions = 0;
    nvSetSmpte2086 = NULL; nvSetCta8613 = NULL;
    assert(SDL_EGL_CreateSurface(&device, &nativeWindow));
    assert(nvEglSmpteRc == -ENOSYS && nvNativeCtaRc == -ENOSYS);
    int calls = createCount; red = 8; alpha = 8;
    assert(!SDL_EGL_CreateSurface(&device, &nativeWindow) && createCount == calls);
    enabled = 0; calls = attribCount;
    assert(SDL_EGL_CreateSurface(&device, &nativeWindow));
    assert(recover(&device, &window, -1) == -1 && attribCount == calls);
    enabled = 1; red = 10; alpha = 2; error = "EGL_BAD_SURFACE"; failCreate = 1;
    assert(recover(&device, &window, -1) == -1 && data.egl_surface == EGL_NO_SURFACE);
    puts("PASS HDR metadata values, create/recreate, first-swap logging, optional APIs, errors, SDR isolation, no 8-bit PQ");
}
