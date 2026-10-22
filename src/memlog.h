#ifndef NV_MEMLOG_H
#define NV_MEMLOG_H
#include <stddef.h>

// So nos pontos de log, nunca por quadro. Sem /proc valido, sufixo vazio.
void memlog_amostra(char *sufixo, size_t tamanho);
void memlog_evento(const char *evento);
#endif
