// "Peca como fala": o fio que fala com o Gemini e confere no Cinemeta.
// Um pedido por vez; o novo abandona o anterior. A chave so sai daqui no
// cabecalho x-goog-api-key, por TLS verificado, direto ao Google; nunca em log.
#ifndef NV_PECAFIO_H
#define NV_PECAFIO_H
#include "peca.h"

// Copia tudo o que recebe. 1 = saiu.
int  pecafio_pedir(const char *chave, const char *texto, const char *idioma, const PecaContexto *ctx);
int  pecafio_ocupado(void);
// 1 UMA VEZ quando o pedido termina: resultado, ate PECA_N titulos confirmados
// e as medidas (ms no Gemini, ms no Cinemeta).
int  pecafio_pegar(PecaResultado *r, PecaSugestao *v, int *n, int *msGemini, int *msCinemeta);
void pecafio_cancelar(void);
#endif
