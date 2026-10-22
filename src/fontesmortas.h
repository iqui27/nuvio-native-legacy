// Fontes VOD que nao abriram: identidade estavel, memoria por seis horas.
#ifndef NV_FONTESMORTAS_H
#define NV_FONTESMORTAS_H
#include "streams.h"
// Pode ser chamada de novo para reler o arquivo (ex.: inicio do app).
void fontesmortas_iniciar(void);
// -1 se viva; caso contrario, segundos desde a falha.
long fontesmortas_idade(const Stream *s);
void fontesmortas_marcar(const Stream *s);
void fontesmortas_limpar(const Stream *s);
#endif
