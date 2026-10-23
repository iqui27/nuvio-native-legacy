#ifndef NV_ANDROID_CACHE_H
#define NV_ANDROID_CACHE_H
#include <stdint.h>
#define NV_ANDROID_CACHE_BYTES (80ULL << 20)
const char *nv_android_cache_dir(void);
int nv_android_cache_name(const char *name);
uint64_t nv_android_cache_size(void);
uint64_t nv_android_cache_size_all(void);
void nv_android_cache_trim(uint64_t cap);
int nv_android_cache_clear(void);
void nv_android_cache_touch(const char *path);
void nv_android_cache_migrate(const char *data);
#endif
