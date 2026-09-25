// MENU DE CONTEXTO do cartaz, aberto SEGURANDO OK sobre um card da home.
//
// E o `posterHoldMenu` do app web, e as opcoes sao as dele, medidas no bundle
// 1.0.4 (getPosterHoldMenuOptions): "Ver detalhes", "Adicionar/Remover da
// biblioteca" e, so em filme e serie, "Marcar como assistido/nao assistido".
// Os rotulos sao os do pt-BR do proprio app.
//
// Existe porque as duas acoes de biblioteca so tinham caminho DENTRO da tela de
// titulo: para marcar um filme como visto era preciso abrir o detalhe, esperar
// a rede e descer ate o botao. Segurando o OK sobre o cartaz sao dois toques.
#ifndef NV_CTXMENU_H
#define NV_CTXMENU_H
#include <SDL2/SDL.h>
#include "catalogo.h"

// `indice` e a posicao no catalogo global.
// A integracao da pressao longa fica em home.c: ele mede NV_HOLD_MS no KEYUP e
// chama ctx_abrir somente quando o limiar foi atingido. Este modulo nao mede a
// tecla nem abre no KEYDOWN; assim o toque curto continua abrindo o detalhe e
// o modal recebe apenas o foco D-pad depois de estar visivel.
void ctx_abrir(int indice);
int  ctx_aberto(void);
void ctx_evento(const SDL_Event *e);
void ctx_atualizar(float dt, Uint32 agora);
void ctx_desenhar(Uint32 agora);
// Indice do titulo cujo detalhe o dono pediu, ou -1. Consumido uma vez.
int  ctx_pediu_detalhes(void);

// O MESMO MENU, aberto pelo painel de Salvos (salvospainel.c). A pressao longa
// la tambem e medida por quem conhece a linha, com o mesmo NV_HOLD_MS, e o
// painel chama isto no limiar. `titulo` e COPIADO: a linha do painel pode nao
// ter indice no catalogo (veio so da lista local). Opcoes: "Mais informações",
// "Remover dos Salvos" (o mesmo OP_LISTA do cartaz: lista local + Trakt ou
// Simkl + espelho) e, quando da, "Marcar como assistido".
void ctx_abrir_salvo(const CatItem *titulo);
// 1 enquanto o menu aberto e o do painel: app.c o desenha POR CIMA do painel e
// entrega a ele as teclas que chegariam ao painel.
int  ctx_do_painel(void);
// IMDb do "Mais informações" pedido no modo painel, ou NULL. Consumido uma vez.
const char *ctx_pediu_detalhes_imdb(void);
// Centro horizontal da barra "Segure OK para opções"; negativo = centro da tela.
void ctx_centro_dica(float cx);
#endif
