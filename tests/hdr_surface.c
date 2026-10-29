/* Exercise the real SDL injection without Android hardware or EGL libraries. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#define SDL_VIDEO_DRIVER_ANDROID 1
#define SDL_FALSE 0
#define EGL_NO_SURFACE NULL
#define EGL_NONE 0x3038
#define EGL_EXTENSIONS 1
#define EGL_CONFIG_ID 6
#define EGL_RED_SIZE 2
#define EGL_GREEN_SIZE 3
#define EGL_BLUE_SIZE 4
#define EGL_ALPHA_SIZE 5
#define EGL_COLOR_COMPONENT_TYPE_EXT 0x3339
#define EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT 0x333B
#define SDL_EGL_DISPLAY_EXTENSION 0
#define RTLD_DEFAULT NULL
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define ADATASPACE_UNKNOWN 0
#define ADATASPACE_BT2020_PQ 163971072
#define ADATASPACE_BT2020_ITU_PQ 298188800
#define ADATASPACE_BT2020_HLG 168165376
#define ADATASPACE_SCRGB_LINEAR 406913024
#define STANDARD_BT2020 (6 << 16)
#define TRANSFER_LINEAR (1 << 22)
#define RANGE_EXTENDED (3 << 27)
typedef int EGLint;
typedef int SDL_bool;
typedef void *EGLSurface;
typedef void ANativeWindow;
static int enabled, pq, api, symbol = 1, bits, createFails, dataResult, acceptSpace;
static int creates, destroys, tags, attrs[9], spaces[32], stages[32], wrongConfig, ignoredTag;
static int32_t currentSpace;
static char mode[32], actualConfig[32], errorText[160], logText[16384];
static int gfx_hdr_output;
static int android_hdr_interface(int on) { return on; }
static const char *SDL_GetHint(const char *key) {
    return !strcmp(key, "NUVIO_HDR_CONFIG") ? actualConfig : mode;
}
/* The framebuffer query can disagree with the EGL surface on GLES drivers. */
#define SDL_GL_RED_SIZE 2
#define SDL_GL_GREEN_SIZE 3
#define SDL_GL_BLUE_SIZE 4
#define SDL_GL_ALPHA_SIZE 5
static int SDL_GL_GetAttribute(int key, int *value) { (void)key; *value = 8; return 0; }
static int SDL_GetHintBoolean(const char *key, int fallback);
static void summary(void) {
    int hdrColor = enabled, hdrPedido = 1;
#include "summary.inc"
}
static void checkSummary(int expected) {
    gfx_hdr_output = 0; summary(); assert(gfx_hdr_output == expected);
}
static int SDL_GetHintBoolean(const char *key, int fallback) { (void)key; (void)fallback; return enabled; }
static void SDL_SetHint(const char *key, const char *value) {
    char *target = !strcmp(key, "NUVIO_HDR_CONFIG") ? actualConfig : mode;
    snprintf(target, 32, "%s", value);
}
static void SDL_Log(const char *format, ...) {
    size_t end = strlen(logText);
    va_list ap; va_start(ap, format); vsnprintf(logText + end, sizeof logText - end, format, ap); va_end(ap);
}
static void SDL_SetError(const char *format, ...) {
    va_list ap; va_start(ap, format); vsnprintf(errorText, sizeof errorText, format, ap); va_end(ap);
}
static const char *SDL_GetError(void) { return errorText; }
static int android_get_device_api_level(void) { return api; }
static int setData(ANativeWindow *window, int32_t space) {
    assert(window && tags < 32);
    spaces[tags] = space; stages[tags++] = creates;
    if (dataResult || (acceptSpace && space != acceptSpace && space != ADATASPACE_UNKNOWN)) {
        errno = EINVAL; return dataResult ? dataResult : -22;
    }
    if (!ignoredTag) currentSpace = space;
    return 0;
}
static int32_t getData(ANativeWindow *window) { assert(window); return currentSpace; }
static const char *dlerror(void) { return symbol ? NULL : "missing symbol"; }
static void *dlopen(const char *name, int flags) {
    assert(!strcmp(name, "libnativewindow.so") && flags == (RTLD_NOW | RTLD_LOCAL)); return (void *)2;
}
static void *dlsym(void *handle, const char *name) {
    if (!strcmp(name, "ANativeWindow_getBuffersDataSpace")) return (void *)getData;
    if (strstr(name, "Metadata")) return NULL;
    assert(!strcmp(name, "ANativeWindow_setBuffersDataSpace"));
    /* Reproduce TCL: hidden from RTLD_DEFAULT, exported by libnativewindow. */
    return symbol && handle ? (void *)setData : NULL;
}
static const char *extensions(void *display, int key) { (void)display; (void)key; return "EGL_KHR_gl_colorspace EGL_EXT_pixel_format_float"; }
static int config(void *display, void *c, int key, int *value) {
    (void)display; (void)c;
    *value = key == EGL_CONFIG_ID ? 42 : key == EGL_COLOR_COMPONENT_TYPE_EXT ? EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT : key == EGL_ALPHA_SIZE && bits == 10 ? 2 : bits;
    return 1;
}
static EGLSurface *create(void *display, void *c, void *window, const int *a) {
    (void)display; (void)c; (void)window; creates++; currentSpace = 0; /* Driver resets the tag. */
    for (int i = 0; i < 9; i++) { attrs[i] = a[i]; if (a[i] == EGL_NONE) break; }
    return createFails ? NULL : (void *)1;
}
static int destroy(void *display, void *surface) { (void)display; assert(surface); destroys++; return 1; }
static int eglError(void) { return 0x3009; }
static int querySurface(void *display, void *surface, int key, int *value) {
    (void)display; assert(surface && key == EGL_CONFIG_ID); *value = wrongConfig ? 99 : 42; return 1;
}
static int ANativeWindow_getFormat(void *window) { assert(window); return 43; }
static void Android_SetFormat(int wanted, int got) { assert(wanted == 43 && got == 43); }
typedef struct {
    void *egl_display, *egl_config;
    const char *(*eglQueryString)(void *, int);
    int (*eglGetConfigAttrib)(void *, void *, int, int *);
    EGLSurface *(*eglCreateWindowSurface)(void *, void *, void *, const int *);
    int (*eglDestroySurface)(void *, void *);
    int (*eglGetError)(void);
} Egl;
typedef struct { Egl *egl_data; } Device;
static int SDL_EGL_HasExtension(Device *device, int type, const char *name) {
    (void)device; (void)type;
    return !strcmp(name, "EGL_EXT_gl_colorspace_bt2020_pq") ? pq : 0;
}
typedef int (*PFNEGLQUERYSURFACEPROC)(void *, void *, int, int *);
static void *SDL_EGL_GetProcAddress(Device *device, const char *name) {
    (void)device;
    if (!strcmp(name, "eglSurfaceAttrib")) return NULL;
    assert(!strcmp(name, "eglQuerySurface")); return (void *)querySurface;
}
#define _THIS Device *_this
#define EGL_SMPTE2086_DISPLAY_PRIMARY_RX_EXT 0x3341
#define EGL_CTA861_3_MAX_CONTENT_LIGHT_LEVEL_EXT 0x3360
typedef int (*PFNEGLSURFACEATTRIBPROC)(void *, EGLSurface, int, int);
#include "../android/app/src/main/cpp/hdr_native.inc"
static EGLSurface *surface(Device *_this, void *nw) {
    int attribs[9] = { EGL_NONE }, attr = 1, format_wanted = 43, format_got;
    EGLSurface *surface;
#include "../android/app/src/main/cpp/hdr_surface.inc"
    return create(NULL, NULL, nw, attribs);
}
static void reset(void) {
    enabled = 1; pq = createFails = dataResult = acceptSpace = 0; api = 34; bits = 10;
    creates = destroys = tags = 0; mode[0] = errorText[0] = logText[0] = 0; currentSpace = 0;
    nvHdrWindow = NULL; actualConfig[0] = 0; wrongConfig = ignoredTag = 0;
}
int main(int argc, char **argv) {
    Egl egl = {NULL, NULL, extensions, config, create, destroy, eglError}; Device device = {&egl};
    if (argc > 1) {
        assert(!strcmp(argv[1], "missing-symbol")); reset(); symbol = 0;
        assert(!surface(&device, &device));
        assert(!tags && !creates && strstr(logText, "lookup=libnativewindow.so symbol=setDataspace found=0"));
        return 0;
    }
    reset(); assert(surface(&device, &device));
    assert(!strcmp(mode, "pq") && tags == 2 && stages[0] == 0 && stages[1] == 1);
    checkSummary(1);
    assert(!strcmp(actualConfig, "1010102"));
    assert(strstr(logText, "config=10/10/10/2 component=0x0 id=42"));
    assert(strstr(logText, "lookup=RTLD_DEFAULT symbol=setDataspace found=0"));
    assert(strstr(logText, "lookup=libnativewindow.so symbol=setDataspace found=1"));
    assert(strstr(logText, "before=0 after=163971072 rc=0 errno=0"));
    currentSpace = 0; nvHdrAfterSwap((void *)1); nvHdrAfterSwap((void *)1);
    assert(tags == 3 && currentSpace == ADATASPACE_BT2020_PQ && strstr(logText, "stage=after-first-swap"));
    reset(); acceptSpace = ADATASPACE_BT2020_ITU_PQ; assert(surface(&device, &device));
    assert(!strcmp(mode, "pq-limited") && creates == 2 && destroys == 1);
    checkSummary(5);
    assert(spaces[0] == ADATASPACE_BT2020_PQ && spaces[2] == ADATASPACE_BT2020_ITU_PQ);
    reset(); acceptSpace = ADATASPACE_BT2020_HLG; assert(surface(&device, &device));
    assert(!strcmp(mode, "hlg") && creates == 3 && destroys == 2 && tags == 6);
    checkSummary(3);
    reset(); bits = 16; assert(surface(&device, &device));
    assert(!strcmp(mode, "scrgb-linear") && spaces[0] == ADATASPACE_SCRGB_LINEAR);
    checkSummary(2);
    reset(); bits = 16; acceptSpace = STANDARD_BT2020 | TRANSFER_LINEAR | RANGE_EXTENDED;
    assert(surface(&device, &device)); assert(!strcmp(mode, "bt2020-linear") && creates == 2);
    reset(); wrongConfig = 1; assert(!surface(&device, &device));
    assert(creates == 3 && destroys == 3); checkSummary(0);
    reset(); ignoredTag = 1; assert(!surface(&device, &device));
    assert(creates == 3 && destroys == 3); checkSummary(0);
    reset(); pq = 1; assert(surface(&device, &device)); assert(attrs[0] == 0x309D && attrs[1] == 0x3340);
    reset(); api = 27; assert(!surface(&device, &device)); assert(!creates && !tags);
    reset(); dataResult = -1; assert(!surface(&device, &device));
    assert(creates == 3 && destroys == 3 && strstr(logText, "rc=-1 errno=22"));
    reset(); bits = 8; assert(!surface(&device, &device)); assert(!creates && strstr(errorText, "8/8/8/8"));
    checkSummary(0);
    strcpy(mode, "pq"); strcpy(actualConfig, "8888"); checkSummary(0);
    strcpy(actualConfig, "FP16"); checkSummary(0);
    reset(); createFails = 1; assert(!surface(&device, &device)); assert(creates == 3 && strstr(logText, "egl=0x3009"));
    reset(); enabled = 0; assert(surface(&device, &device));
    assert(creates == 1 && !tags && !mode[0] && !logText[0] && attrs[0] == EGL_NONE);
    puts("PASS lookup fallback, errno, ordered PQ/HLG/FP16 attempts, creation/swap resets, SDR unchanged");
}
