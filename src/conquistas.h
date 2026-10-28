// CONQUISTAS (2.1): Perfil › Conquistas, as seis familias, o detalhe de um
// emblema e o aviso "depois dos creditos". Mockup: design/2.1/conquistas.html;
// catalogo e perguntas em design/2.1/NOTAS-CONQUISTAS.md.
// Nao confundir com badges.c (selos de qualidade da fonte, 4K/HDR).
//
// O QUE ESTA LIGADO DE VERDADE: o estado do perfil (conquistasdados.h, regra em
// conquistasregra.h), as abas, o "Retomar", o detalhe de cinco niveis e o aviso.
// O QUE E ESTADO VAZIO HONESTO (ambar, precisa do nosso servidor): raridade,
// mapa de descobertas, amigos na trilha, comunidade, temporadas, secretos. A
// tela ja pergunta por eles em conquistasdados.h; hoje nenhum entrega nada.
// "Continuar a trilha" e "Ir a trilha" abrem a pagina da jornada
// (trilhas.h); "Fixar no perfil" segue apagado — o perfil nao recebe ainda.
//
// O emblema e um quadrado de cantos arredondados, nao o hexagono do mockup: o
// gfx nao tem primitiva de poligono.
//
// O FOCO E GEOMETRICO (como comecar.c): o desenho, o teclado e os alvos do
// ponteiro usam a mesma lista de retangulos.
#ifndef NV_CONQUISTAS_H
#define NV_CONQUISTAS_H
#include <SDL2/SDL.h>
#include "layout.h"
#include "gfx.h"

// ---- A tela.
void conquistas_abrir(void);
void conquistas_abrir_emblema(const char *chave);
void conquistas_ocultar(void);
int  conquistas_aberto(void);
void conquistas_evento(const SDL_Event *e);
// Tambem anda o relogio do aviso, aberta a tela ou nao.
void conquistas_atualizar(float dt, Uint32 agora);
// Tela inteira, ou o fim do esmaecer de saida por cima do app.
void conquistas_desenhar(Uint32 agora);

// Ligacao com a pagina da trilha (trilhas.h). Devolve 1 se a trilha existe
// no catalogo local e a pagina abriu; 0 deixa o botao apagado com "Em breve".
int  conquistas_abrir_trilha(const char *chave);
// Pasta de arte do pacote (main.c passa o dirArte): onde ficam os objetos 3D em art/conquistas.
void conquistas_arte_dir(const char *d);

// ---- O aviso depois dos creditos. Nao toma o foco: so o Voltar e dele, e
// so enquanto estiver na tela.
// Chamar quando o player fecha depois de um titulo visto. Le o estado do
// perfil; sem emblema novo, ou com o aviso desligado em Ajustes, nao faz nada.
void conquistas_aviso_armar(void);
int  conquistas_aviso_aberto(void);
// 1 se consumiu a tecla (Voltar com o aviso na tela).
int  conquistas_aviso_evento(const SDL_Event *e);
void conquistas_aviso_fechar(void);
void conquistas_aviso_desenhar(void);

// ---- Para a captura e os testes.
enum { CONQUISTAS_VISTA_HUB = 0, CONQUISTAS_VISTA_FAMILIAS, CONQUISTAS_VISTA_DETALHE };
int  conquistas_vista(void);
int  conquistas_foco(void);
int  conquistas_aba(void);
int  conquistas_topo(void);
int  conquistas_filtradas(void);
// Os alvos focaveis do quadro atual (coordenadas de tela). Devolve quantos.
int  conquistas_teste_itens(int *ids, GfxRect *rs, int max);
void conquistas_teste_esquecer(void);

#endif
