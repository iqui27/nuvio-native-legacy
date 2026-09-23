#ifndef NV_HOMEESTADO_H
#define NV_HOMEESTADO_H

#include "catalogo.h"

// Estado estrutural da Home, separado do corpo de itens. O arquivo e privado
// ao usuario/perfil/idioma/configuracao local e serve como guarda contra uma
// resposta parcial apagar a estrutura persistida.
void homeestado_iniciar(void);
unsigned homeestado_geracao(void);
int homeestado_contexto_valido(void);
int homeestado_tem_fileira(const char *chave);
int homeestado_ordem_fileira(const char *chave);
int homeestado_quantidade_fileiras(void);
void homeestado_salvar(const CatFileira *fils, int n);
/* Generation-scoped variants for asynchronous discovery publishers. */
int homeestado_salvar_se_geracao(const CatFileira *fils, int n, unsigned geracao);
int homeestado_identidade_geracao(unsigned geracao, char *dono, unsigned tamDono,
                                  int *perfil);
void homeestado_esquecer(void);

#endif
