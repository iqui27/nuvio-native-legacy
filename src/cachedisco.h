#ifndef NV_CACHEDISCO_H
#define NV_CACHEDISCO_H
#ifndef __EMSCRIPTEN__
#include <stdint.h>
/* Chamador serializa escrita/poda. Apenas arquivos hash.ext entram na poda. */
typedef int (*NvCacheProtegido)(const char *caminho, void *ctx);
long nv_cache_podar(const char *dir, long *total, long entrada, long teto,
                   uint64_t reserva, int forcar, NvCacheProtegido protegido, void *ctx);
#endif
#endif
