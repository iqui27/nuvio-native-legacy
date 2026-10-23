#ifndef NV_FONTEINFO_H
#define NV_FONTEINFO_H
#include <stdint.h>
struct Stream;
// Display-only snapshot. Never retains URLs, headers or debrid credentials.
#define FONTEINFO_TEXTOS 12
typedef struct {
  uint64_t selos;
  int nTextos;
  char texto[FONTEINFO_TEXTOS][80];
} FonteInfo;
void fonteinfo_montar(const struct Stream *fonte, FonteInfo *saida);
// UI-thread ownership, matching the foreground prepared-source cache.
void fonteinfo_limpar(void);
void fonteinfo_guardar(const struct Stream *fonte, const char *alvo,
                      unsigned geracao, uint64_t sessao, int indice);
int fonteinfo_ler(const char *alvo, unsigned geracao, uint64_t sessao, FonteInfo *saida);
#endif
