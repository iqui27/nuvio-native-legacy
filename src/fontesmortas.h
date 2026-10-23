// Fontes VOD que nao abriram: identidade estavel, memoria por seis horas.
#ifndef NV_FONTESMORTAS_H
#define NV_FONTESMORTAS_H
#include "streams.h"
// Pode ser chamada de novo para reler o arquivo (ex.: inicio do app).
void fontesmortas_iniciar(void);
// O arquivo pertence a conta e ao perfil; abrir outro nunca apaga o anterior.
void fontesmortas_definir_escopo(const char *conta, int perfil);
// -1 se viva; caso contrario, segundos desde a falha.
long fontesmortas_idade(const Stream *s);
void fontesmortas_marcar(const Stream *s);
void fontesmortas_limpar(const Stream *s);
// Changes on mutations, profile changes, and expiry; independent of source count.
unsigned fontesmortas_versao(void);
#endif
