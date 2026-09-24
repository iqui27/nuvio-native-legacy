#ifndef NV_HOMEESTADO_H
#define NV_HOMEESTADO_H

#include "catalogo.h"

// Estado estrutural da Home, separado do corpo de itens. O arquivo e privado
// ao usuario/perfil/idioma/configuracao local e serve como guarda contra uma
// resposta parcial apagar a estrutura persistida.
void homeestado_iniciar(void);

// AS PARTES DO CONTEXTO, separadas pelo que cada uma invalida (ver o
// comentario de hashAjustes em homeestado.c). `identidade` vai inteira (dono,
// perfil, idioma) para a comparacao nao depender de hash; o resto e hash.
typedef struct {
  char identidade[448];
  int perfil;
  unsigned addons;       // bases e liga/desliga: O QUE se busca
  unsigned ajustes;      // limite, estilo do CW, fonte do destaque...
  unsigned fileiras;     // escolhas de fileiras.c (oculta/forma/tamanho/ordem)
  unsigned ordemConta;   // catordem da conta
  unsigned colecoes;     // pastas e fontes das colecoes
} HomeContexto;

#define HOMEESTADO_MUDOU_IDENTIDADE   0x01
#define HOMEESTADO_MUDOU_ADDONS       0x02
#define HOMEESTADO_MUDOU_AJUSTES      0x04
#define HOMEESTADO_MUDOU_FILEIRAS     0x08
#define HOMEESTADO_MUDOU_ORDEM_CONTA  0x10
#define HOMEESTADO_MUDOU_COLECOES     0x20
// Mudou O QUE foi buscado: dado da montagem em voo nao serve mais.
#define HOMEESTADO_MUDOU_FONTE (HOMEESTADO_MUDOU_IDENTIDADE | HOMEESTADO_MUDOU_ADDONS)
// Mudou so COMO o que foi buscado e arrumado: publica e remonta sem rede.
#define HOMEESTADO_MUDOU_ESTRUTURA (HOMEESTADO_MUDOU_AJUSTES | HOMEESTADO_MUDOU_FILEIRAS | \
                                    HOMEESTADO_MUDOU_ORDEM_CONTA | HOMEESTADO_MUDOU_COLECOES)

void homeestado_contexto(HomeContexto *c);
// Bits HOMEESTADO_MUDOU_* do que difere entre `antes` e `agora`.
int homeestado_mudancas(const HomeContexto *antes, const HomeContexto *agora);
// "colecoes+fileiras", para log. Devolve `buf`.
const char *homeestado_mudancas_texto(int mudancas, char *buf, unsigned tam);

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
