#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static int fake_info(const char *key, char **out) { (void)key; *out = strdup(getenv("TEST_TIZEN")); return 0; }
static void *fake_open(const char *name, int flags) { (void)name; (void)flags; return (void *)1; }
static void *fake_sym(void *handle, const char *name) { (void)handle; (void)name; return (void *)fake_info; }
#define dlopen fake_open
#define dlsym fake_sym
#define NV_TPK
#include "../src/dts/dts_hls_tpk.c"
int dts_remux_available(void) { return atoi(getenv("TEST_FFMPEG")); }
int main(int argc, char **argv) { assert(argc == 2); assert(dts_tpk_disponivel() == atoi(argv[1])); return 0; }
