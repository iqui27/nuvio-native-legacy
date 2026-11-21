#ifndef NV_NVBACKEND_H
#define NV_NVBACKEND_H

#include <stddef.h>

/* URL do servidor Nuvio (recomendações, social): compile-time NV_REC_URL ou Ajustes. */
/* Copia a URL efetiva para `dst` (sempre terminada). Devolve o tamanho
   escrito. Segura para qualquer fio: o texto nao aponta para estado interno. */
size_t nv_rec_url_copiar(char *dst, size_t tam);
/* 1 se ha URL efetiva (configurada ou da build); forma barata de testar. */
int nv_rec_url_ativo(void);
int nvbackend_definir_url(const char *u);
/* Inicia a descoberta GET origin/.well-known/nuvio num fio proprio (a rede
   nao roda no fio do chamador). 1 aceitou, 0 recusou (origem invalida). */
int nvbackend_descobrir_iniciar(const char *origin);
/* 0 nunca pediu, 1 descobrindo, 2 encontrou e gravou, -1 falhou. */
int nvbackend_descobrir_estado(void);
/* Recolhe o fio (se houver) e volta o estado para 0. */
void nvbackend_descobrir_limpar(void);

#endif
