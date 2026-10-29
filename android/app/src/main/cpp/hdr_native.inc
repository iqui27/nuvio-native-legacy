/* Android-only helpers in the private SDL EGL translation unit. */
#ifdef SDL_VIDEO_DRIVER_ANDROID
#include <errno.h>
static int (*nvSetDataspace)(ANativeWindow *, int32_t);
static int32_t (*nvGetDataspace)(ANativeWindow *);
static ANativeWindow *nvHdrWindow;
static EGLSurface nvHdrSurface;
static int32_t nvHdrDataspace;
static const char *nvHdrName;
static int nvHdrFirstSwap;
/* Float layout of Android's HDR metadata ABI; symbols are optional at runtime. */
typedef struct { float x, y; } NvColorXY;
typedef struct {
    NvColorXY red, green, blue, white;
    float maxLuminance, minLuminance;
} NvSmpte2086;
typedef struct { float maxCLL, maxFALL; } NvCta8613;
static int (*nvSetSmpte2086)(ANativeWindow *, const NvSmpte2086 *);
static int (*nvSetCta8613)(ANativeWindow *, const NvCta8613 *);
static int nvEglSmpteRc, nvEglCtaRc, nvNativeSmpteRc, nvNativeCtaRc;

static int nvHdrLoad(void)
{
    static int loaded;
    static void *library;
    const char *error;
    if (loaded) return nvSetDataspace != NULL;
    loaded = 1;
    dlerror();
    nvSetDataspace = (int (*)(ANativeWindow *, int32_t))dlsym(RTLD_DEFAULT, "ANativeWindow_setBuffersDataSpace");
    error = dlerror();
    SDL_Log("[hdr-ui] lookup=RTLD_DEFAULT symbol=setDataspace found=%d error=%s", nvSetDataspace != NULL, error ? error : "none");
    dlerror();
    library = dlopen("libnativewindow.so", RTLD_NOW | RTLD_LOCAL);
    error = dlerror();
    SDL_Log("[hdr-ui] lookup=dlopen library=libnativewindow.so found=%d error=%s", library != NULL, error ? error : "none");
    if (library) {
        dlerror();
        int (*resolved)(ANativeWindow *, int32_t);
        resolved = (int (*)(ANativeWindow *, int32_t))dlsym(library, "ANativeWindow_setBuffersDataSpace");
        error = dlerror();
        if (resolved) nvSetDataspace = resolved;
        SDL_Log("[hdr-ui] lookup=libnativewindow.so symbol=setDataspace found=%d error=%s", nvSetDataspace != NULL, error ? error : "none");
        nvGetDataspace = (int32_t (*)(ANativeWindow *))dlsym(library, "ANativeWindow_getBuffersDataSpace");
        if (android_get_device_api_level() >= 29) {
            nvSetSmpte2086 = (int (*)(ANativeWindow *, const NvSmpte2086 *))dlsym(library, "ANativeWindow_setBuffersSmpte2086Metadata");
            nvSetCta8613 = (int (*)(ANativeWindow *, const NvCta8613 *))dlsym(library, "ANativeWindow_setBuffersCta8613Metadata");
        }
    }
    return nvSetDataspace != NULL;
}

static int nvHdrTag(ANativeWindow *window, int32_t space, const char *name, const char *stage)
{
    int result, savedErrno;
    int32_t before = nvGetDataspace ? nvGetDataspace(window) : -1, after;
    errno = 0;
    result = nvSetDataspace(window, space);
    savedErrno = errno;
    after = nvGetDataspace ? nvGetDataspace(window) : -1;
    if (!result && nvGetDataspace && after != space) result = -EINVAL;
    SDL_Log("[hdr-ui] attempt=%s stage=%s dataspace=%d before=%d after=%d rc=%d errno=%d",
            name, stage, space, before, after, result, savedErrno);
    return result;
}

static void nvHdrMetadata(_THIS, EGLSurface surface)
{
    const NvSmpte2086 mastering = {{0.708f, 0.292f}, {0.170f, 0.797f},
                                  {0.131f, 0.046f}, {0.3127f, 0.3290f}, 1000.0f, 0.0001f};
    const NvCta8613 light = {1000.0f, 400.0f};
    /* SMPTE2086 uses EGL_METADATA_SCALING_EXT (50000); CTA861.3 uses nits. */
    const EGLint values[] = {35400, 14600, 8500, 39850, 6550, 2300, 15635, 16450, 50000000, 5,
                             1000, 400};
    PFNEGLSURFACEATTRIBPROC attrib = (PFNEGLSURFACEATTRIBPROC)SDL_EGL_GetProcAddress(_this, "eglSurfaceAttrib");
    int i;
    nvEglSmpteRc = nvEglCtaRc = -ENOSYS;
    nvNativeSmpteRc = nvNativeCtaRc = -ENOSYS;
    if (nvHdrDataspace != ADATASPACE_BT2020_PQ && nvHdrDataspace != ADATASPACE_BT2020_ITU_PQ) return;
    if (attrib && SDL_EGL_HasExtension(_this, SDL_EGL_DISPLAY_EXTENSION, "EGL_EXT_surface_SMPTE2086_metadata")) {
        nvEglSmpteRc = 0;
        for (i = 0; i < 10; ++i)
            if (!attrib(_this->egl_data->egl_display, surface, EGL_SMPTE2086_DISPLAY_PRIMARY_RX_EXT + i, values[i]))
                nvEglSmpteRc = _this->egl_data->eglGetError();
    }
    if (attrib && SDL_EGL_HasExtension(_this, SDL_EGL_DISPLAY_EXTENSION, "EGL_EXT_surface_CTA861_3_metadata")) {
        nvEglCtaRc = 0;
        for (i = 10; i < 12; ++i)
            if (!attrib(_this->egl_data->egl_display, surface, EGL_CTA861_3_MAX_CONTENT_LIGHT_LEVEL_EXT + i - 10, values[i]))
                nvEglCtaRc = _this->egl_data->eglGetError();
    }
    if (nvSetSmpte2086) nvNativeSmpteRc = nvSetSmpte2086(nvHdrWindow, &mastering);
    if (nvSetCta8613) nvNativeCtaRc = nvSetCta8613(nvHdrWindow, &light);
}

static void nvHdrAfterSwap(EGLSurface surface)
{
    if (SDL_GetHintBoolean("NUVIO_HDR_UI", SDL_FALSE) && surface == nvHdrSurface && nvHdrWindow && !nvHdrFirstSwap) {
        nvHdrFirstSwap = 1;
        nvHdrTag(nvHdrWindow, nvHdrDataspace, nvHdrName, "after-first-swap");
        SDL_Log("[hdr-ui] metadata egl-smpte=%d egl-cta=%d native-smpte=%d native-cta=%d",
                nvEglSmpteRc, nvEglCtaRc, nvNativeSmpteRc, nvNativeCtaRc);
        SDL_Log("[hdr-ui] check: adb shell dumpsys SurfaceFlinger (SurfaceView: dataspace=BT2020_PQ, hdr metadata types != 0)");
    }
}
#endif
