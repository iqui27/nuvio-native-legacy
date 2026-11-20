#ifndef NV_DIRECTCACHE_H
#define NV_DIRECTCACHE_H
#include <stddef.h>

// #764: grava por escrita antecipada em USB links HTTP/debrid enquanto o player lê.
int directcache_ativo(void);
int directcache_pasta(char *out, size_t n);
const char *directcache_preparar(const char *url, const char *cabecalhos,
                                 char *buf, size_t n);
void directcache_encerrar(void);

#endif
