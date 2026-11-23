#ifndef NV_HOMEASSISTIU_H
#define NV_HOMEASSISTIU_H

#include <stdint.h>
#include <stddef.h>

#define HOME_ASSISTIU_CHAVE "porque_assistiu"
#define HOME_ASSISTIU_MAX 12

unsigned home_assistiu_passo(uint32_t agora);
int home_assistiu_itens(int saida[HOME_ASSISTIU_MAX]);
void home_assistiu_rotulo(char *saida, size_t tam);
int home_assistiu_sem_farol(int *recomendados, int n, const int *farol, int nFarol);

#endif
