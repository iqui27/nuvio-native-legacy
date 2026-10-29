// "ACAO FEITA" NA ILHA (pedido do dono, 03/10/2026): "toda confirmacao, tipo
// adicionado a biblioteca ou a lista, tem que aparecer na ilha do relogio; se
// clicar, mostra o que foi feito e se quer desfazer".
//
// Quem faz a acao chama ilhaacao_feita DEPOIS de gravar. A pilula diz a frase
// (com o voo da capa quando ha `thumb` e `voar`); AZUL/OK/clique nela abre o
// modal: capa + titulo, a frase, onde, e "Desfazer" (so se `desfazer` existe) e
// "Ok". Desfazer chama a funcao com a copia do contexto e a ilha diz "Desfeito".
//
// SO SE OFERECE DESFAZER ONDE HA INVERSO DE VERDADE. Quem nao tem passa
// `desfazer` NULL e o modal fica so com "Ok". Ver a lista no relatorio da
// ilhasalvar/ctxmenu: Salvar/Remover da lista e Assistido/Nao assistido (sem
// posicao de retomada que se perderia) tem; Tirar de Continuar nao tem (apaga o
// ponto de retomada local e nas contas, e nao ha como restaurar).
#ifndef NV_ILHAACAO_H
#define NV_ILHAACAO_H
#include <stddef.h>
#include <SDL2/SDL.h>

typedef void (*IlhaDesfazer)(const void *ctx);

typedef struct {
  const char *icone;     // art/icones; NULL = "check"
  const char *frase;     // ja traduzida: "Salvo em Lista do Nuvio"
  const char *titulo;    // do modal: o nome do titulo ("" = sem)
  const char *onde;      // linha apagada do modal ("Lista do Nuvio"), pode ser ""
  const char *thumb;     // url da capa (voo ate a pilula; modal se `arte` vazio)
  const char *arte;      // opcional: fundo/paisagem do titulo; o modal prefere (senao o cartaz em moldura 2:3)
  int tipo;              // ILHA_ACENTO / ILHA_INFO ...
  int voar;              // 1 = a capa voa ate a pilula
  IlhaDesfazer desfazer; // NULL = sem Desfazer
  const void *ctx;       // copiado (ctxN bytes) para o desfazer
  size_t ctxN;
} IlhaAcao;

void ilhaacao_feita(const IlhaAcao *a);
// Botao do modal (1 = Desfazer quando existe, senao Ok); ilhasinais.c entrega.
void ilhaacao_botao(int botao);
// 1 se a ultima acao postada ainda tem Desfazer a oferecer (testes).
int  ilhaacao_tem_desfazer(void);
// Solta a copia da acao anterior, sem executar Desfazer.
void ilhaacao_abrir_sessao(void);

#endif
