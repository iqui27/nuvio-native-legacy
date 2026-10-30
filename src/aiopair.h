#ifndef NV_AIOPAIR_H
#define NV_AIOPAIR_H
#include <stddef.h>
enum { AIOPAIR_OFF, AIOPAIR_STARTING, AIOPAIR_WAITING, AIOPAIR_CHECKING,
       AIOPAIR_SYNCED, AIOPAIR_ERROR, AIOPAIR_EXPIRED, AIOPAIR_UNAVAILABLE };
int aiopair_start(const char *origin);
void aiopair_close(void);
/* Snapshot on the app thread. URL contains only short-lived pairing keys.
 * Successful authorization is consumed once, after same-origin /api/v1/user. */
int aiopair_state(char *url, size_t n);
int aiopair_take(char *authorization, size_t n, char *origin, size_t no);
#endif
