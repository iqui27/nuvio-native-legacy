// Bridge between the C core and the iOS host (ios/ios.m).
//
// Same shape as android.h: the host owns the process entry, the data folders,
// the log file and the system HTTP stack; the core keeps running main() as it
// does on every other target. Only compiled with -DNV_IOS.
#ifndef NV_IOS_H
#define NV_IOS_H
#ifdef NV_IOS
#include <stddef.h>

// SDL's view framebuffer: what the core binds when it binds "0" (gl_compat.h).
extern unsigned nv_gl_fb_tela;
// Settings > Interface mode > Auto resolves to Mobile on the phone (same
// contract as android_interface_mobile_auto: NUVIO_INTERFACE_AUTO=1).
int ios_interface_mobile_auto(void);

// Safe-area insets of the key window in points, read on the main thread.
// Returns 0 when there is no window yet (all four left untouched).
int ios_insets(float *left, float *top, float *right, float *bottom);

// HTTP through NSURLSession, the libcurl stand-in on iOS (there is no libcurl
// in the iOS SDK). Same contract as android_http: GET/POST with "Name: value"
// headers separated by '\n'; returns the body (malloc, NUL-terminated, *n =
// bytes) and *status = HTTP code; NULL = transport failure with the reason in
// `erro`. Any thread.
char *nv_host_http(const char *verbo, const char *url, const char *cabs,
                   const char *corpo, int prazoMs, int *status, int *retryAfter, long *n,
                   char *erro, size_t nErro);
// rede.c keeps the Android name for the host path; on iOS it resolves here.
#define android_http nv_host_http

#endif
#endif
