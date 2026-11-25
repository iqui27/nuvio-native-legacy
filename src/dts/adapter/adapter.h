#ifndef NV_DTS_ADAPTER_H
#define NV_DTS_ADAPTER_H
#include "../dts_media.h"
#include <stdint.h>
#define DTS_ADAPTER_ABI 2
/* Entire boundary is C. Native std::string never crosses this table. */
typedef struct {
  uint32_t abi, size;
  int (*probe)(void);
  void *(*create)(const char *, const char *, void (*)(void *, const char *), void *);
  void (*destroy)(void *);
  int (*load)(void *, const DtsMediaInfo *, double);
  int (*feed)(void *, const DtsFrame *);
  int (*play)(void *);
  int (*pause)(void *);
  int (*flush)(void *, double); /* Reposition native segment; optional firmware Seek, else return 0. */
  int (*eos)(void *);
  const char *(*media_id)(void *);
  const char *(*error)(void *);
  int (*volume)(void *, int); /* Optional firmware capability; NULL is permitted. */
} DtsAdapter;
#ifdef __cplusplus
extern "C" {
#endif
const DtsAdapter *nuvio_dts_adapter_v2(void);
#ifdef __cplusplus
}
#endif
#endif
