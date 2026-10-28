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

// The host's vertical bar (ios.m). Each frame the core reports the screen in
// view (app_tela_nome), whether a Detail page is open over it (app_tela_nome
// only names it over the Home) and the sidebar destination; the bar answers with a
// destination picked on a tab (MENU_*) and a Detail action (DETAIL_BARRA_*),
// each -1 when there is none. Main thread only.
void ios_quadro(const char *tela, int detalhe, int destino);
int ios_destino_pedido(void);
int ios_acao_pedida(void);
// Moves SDL's view controller under the host's tab bar controller (the
// container that gives iOS 27.1 its vertical bar) and starts feeding the
// safe-area insets to layout.c. Idempotent; call after every GL context.
void ios_hospedar(void *uiwindow);

// Physical RAM in MB (NSProcessInfo) and the process footprint in MB (task_info).
long ios_ram_mb(void);
double ios_rss_mb(void);

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
