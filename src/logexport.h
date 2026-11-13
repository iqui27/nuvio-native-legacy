#ifndef NV_LOGEXPORT_H
#define NV_LOGEXPORT_H
#include <stddef.h>
/* All categories, a frozen snapshot of at most the last 16 MiB. Caller owns
 * the 0600 temporary file and must unlink it. Never serves the original log. */
int logexport_create(const char *source, char *path, size_t size);
/* Conservative best-effort redaction, shared by the on-screen viewer. */
void logexport_redact(const char *input, char *output, size_t size);
#endif
