// RESOLVER UM PROBLEMA (2.1): a pessoa escolhe o sintoma, o app testa na ordem
// em que o video passa e para na primeira falha, e o envio do registro devolve
// um codigo curto. Mockup: design/2.1/resolver.html (5A, 5B, 5C). A ordem dos
// testes e a guarda de 30 dias estao em resolverregra.h (testada sem SDL); aqui
// so a tela e a juncao com o resto do app.
//
// O QUE ESTA LIGADO DE VERDADE: internet (redesaude), add-ons (contagem e
// motivo da ultima busca vazia), debrid (conta sem plano / recusa), o ultimo
// erro de fonte do player (so em memoria, ate 6 h), a recusa da TV ao abrir o
// player, o envio do registro (o mesmo de Ajustes › Enviar registro) e a lista
// de registros enviados, guardada 30 dias em resolver-enviados.txt.
// O QUE AINDA NAO: o teste de Audio nao tem de onde ler (aparece "sem teste"),
// "Esconder fontes que esta TV nao toca" e "Mandar o codigo pro celular"
// aparecem apagados com "Em breve", e o estado "respondido" de cada registro
// precisa do servidor. Nenhum teste toca midia nem muda Ajustes.
#ifndef NV_RESOLVER_H
#define NV_RESOLVER_H
#include <SDL2/SDL.h>

#define RESOLVER_ARQ "resolver-enviados.txt"

// Abre a tela de qualquer lugar (Ajustes › Ajuda).
void resolver_abrir(void);
int  resolver_aberto(void);
void resolver_evento(const SDL_Event *e);
void resolver_atualizar(float dt, Uint32 agora);
// Desenha a tela inteira, ou o fim do esmaecer de saida por cima do app.
void resolver_desenhar(Uint32 agora);

#define RESOLVER_PEDIU_NADA   0
#define RESOLVER_PEDIU_FONTES 1   // abrir Ajustes › Fontes e addons
int  resolver_pedido(void);

// ---- Para a captura e os testes.
enum { RESOLVER_FASE_SINTOMA = 0, RESOLVER_FASE_TESTES, RESOLVER_FASE_ENVIO };
int  resolver_fase(void);
int  resolver_foco(void);
int  resolver_sintoma(void);
int  resolver_enviados_n(void);
void resolver_teste_esquecer(void);
#ifdef RESOLVER_TESTE
#include "resolverregra.h"
// Fixa os fatos lidos do app (o harness nao tem rede, add-ons nem player).
void resolver_teste_fatos(const ResolverFatos *f);
#endif

#endif
