#ifndef NV_MOVIEBUFFER_H
#define NV_MOVIEBUFFER_H
#include <stddef.h>
#include "dts/dts_engine.h"
/* Main-thread lifecycle; workers never touch player state or graphics. */
int moviebuffer_url(const char *source, const char *headers, int mib, char *out, size_t n);
void moviebuffer_stop(void);
/* 1 cache active, -1 origin cannot be cached (redirected), 0 inactive. */
int moviebuffer_info(DtsBufferInfo *info);
#endif
