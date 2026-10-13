// Ver homefil.c.
#ifndef NV_HOMEFIL_H
#define NV_HOMEFIL_H
#include "colecoes.h"

// --- "vistos ocultos" por fileira (#366) --------------------------------------
#define OV_MAX 128
int      ov_ativa(const char *chave);
int      ov_alternar(const char *chave);      // devolve o estado NOVO
unsigned ov_revisao(void);
void     ov_definir_perfil(int perfil);
void     ov_esquecer(void);

// --- colecao fixada: vira fileira propria -------------------------------------
// Chave da fileira de uma pasta fixada: "colpin_<id da pasta>".
#define CF_MAX 16
#define CF_PREFIXO "colpin_"
int          cf_fixada(const char *pastaId);
int          cf_alternar(const ColFolder *pasta);   // devolve o estado NOVO
unsigned     cf_revisao(void);
int          cf_n(void);
const ColFolder *cf_pasta(int i);   // NULL se a conta nao a trouxe mais
const char  *cf_titulo(int i);
// AMBAR: os titulos da pasta na ordem do add-on. Hoje devolve 0 (o resolvedor
// das fontes da pasta so existe na tela "Ver tudo"); a Home mostra o estado
// vazio. Quando existir: preencher `indicesCat` e devolver quantos.
int          cf_itens(const ColFolder *pasta, int *indicesCat, int max);
void         cf_definir_perfil(int perfil);
void         cf_esquecer(void);

// --- ocultar / mover uma fileira pela chave (a mesma escolha de Ajustes) -----
int homefil_indice(const char *chave);          // -1 = fora do registro
int homefil_ocultar(const char *chave);         // 1 = ocultou
int homefil_mover(const char *chave, int dir);  // 1 = mudou de lugar

#endif
