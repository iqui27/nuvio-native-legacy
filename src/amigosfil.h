// A FILEIRA "AMIGOS ASSISTINDO" DA HOME, no desenho aprovado pelo dono em
// 02/10/2026 ("E2 com E1 dentro"):
//
//   - uma fileira de ROSTOS (circulos de ~8 % da largura da tela), com o anel
//     LARANJA quando ha novidade que a pessoa ainda nao viu e VERMELHO com um
//     ponto pulsando quando o amigo esta assistindo agora;
//   - o rosto EM FOCO abre ao lado, na mola da ilha (ilha.c), os ultimos tres
//     titulos dele como CARTOES DEITADOS 16:9 (arte, titulo, "Agora · T3E4 ·
//     faltam 12 min" com a barra, "gostou", "salvou");
//   - DIREITA no rosto entra nos cartoes (OK abre o titulo), ESQUERDA volta ao
//     rosto; DIREITA no ultimo cartao segue para o proximo rosto;
//   - OK no rosto abre o perfil do amigo (amigoperfil.c);
//   - o ultimo rosto e "+ Adicionar" (a tela de amigos de recenviar.c);
//   - ZERO AMIGOS: um convite "Adicione amigos" com o botao do celular
//     (celbotao.h, o QR) no lugar de "Siga pessoas no Trakt".
//
// A FONTE (Trakt, Simkl...) NAO APARECE AQUI: so no perfil.
//
// home.c continua dona do foco VERTICAL e de qual coluna (rosto) esta em foco;
// esta fileira e dona do foco DENTRO do rosto (os cartoes), da rolagem
// horizontal propria e do desenho. Os dados vem so de socialvis.h.
#ifndef NV_AMIGOSFIL_H
#define NV_AMIGOSFIL_H
#include <SDL2/SDL.h>
#include <stddef.h>
#ifdef NV_TOUCH_UI
#include "ponteiro.h"
int amigosfil_rolagem(const PonteiroRolagem *e);
void amigosfil_retomar_foco(int *coluna);
void amigosfil_ponteiro(PonteiroFn focar);
void amigosfil_focar(int coluna, int cartao);
#endif

// Colunas que a home navega: rostos + "Adicionar"; 1 (o convite) sem amigos.
int  amigosfil_n_colunas(void);
// 1 = sem amigos: a fileira e o convite.
int  amigosfil_convite(void);
// O item do catalogo que representa a coluna em foco (o cartao em foco, ou o
// titulo mais novo do rosto) — e o que o destaque da home mostra. -1 = nenhum.
int  amigosfil_indice_cat(int coluna);
// Uma tecla com o foco nesta fileira. 1 = consumida. Pode mudar *coluna (a
// direita no ultimo cartao vai ao proximo rosto).
int  amigosfil_tecla(SDL_Keycode k, int *coluna);
// OK (toque curto) na coluna.
void amigosfil_ok(int coluna);
// Por quadro. `focada` = o foco da home esta nesta fileira. *coluna pode
// mudar: quando o modelo muda, o foco segue a PESSOA e nao o indice.
void amigosfil_atualizar(float dt, int focada, int *coluna);
// Desenha a partir de (x0, y): x0 e a margem do conteudo, y o topo dos
// cartoes, `alt` a altura da fileira. `corte` e o topo do recorte da home
// (as fileiras rolam por baixo dele); o desenho volta a ele ao terminar.
void amigosfil_desenhar(float x0, float y, float alt, float corte, int focada,
                        int coluna, Uint32 agora);

// Pedidos para app.c (cada um e entregue uma vez).
int  amigosfil_pediu_titulo(char *imdb, size_t tam);
int  amigosfil_pediu_perfil(char *id, size_t tam);
// Sem o servico de recomendacoes no pacote: "+ Adicionar" leva a Ajustes.
int  amigosfil_pediu_ajustes(void);

// Para os testes: em que cartao esta o foco (-1 = no rosto).
int  amigosfil_dentro(void);
#endif

