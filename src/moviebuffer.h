#ifndef NV_MOVIEBUFFER_H
#define NV_MOVIEBUFFER_H
#include <stddef.h>
#include "dts/dts_engine.h"
/* Main-thread lifecycle; workers never touch player state or graphics. */
int moviebuffer_url(const char *source, const char *headers, int mib, char *out, size_t n);
/* Poll on main thread: 0 preflight pending, 1 ready, -1 unsupported/inactive.
 * A returned URI must not be loaded until ready; unsupported uses the origin. */
int moviebuffer_ready(void);
void moviebuffer_stop(void);
/* 1 cache active, -1 origin cannot be cached (redirected), 0 inactive. */
int moviebuffer_info(DtsBufferInfo *info);
#endif
