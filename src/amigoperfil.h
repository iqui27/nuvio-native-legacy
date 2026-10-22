// PERFIL DO AMIGO, tela cheia (tela C do desenho aprovado em 02/10/2026).
//
// A esquerda: o rosto grande, o nome, "Amigos ha 2 meses · pelo codigo" (com a
// FONTE pequena — e o unico lugar do social em que ela aparece), o "gosto
// parecido" e os quatro numeros do mes. A direita: tres fileiras de cartazes —
// Assistindo, Gostou recentemente e Voce mandou (com o estado de cada um).
//
// Abre pela fileira de amigos da home (OK no rosto) e pela linha do amigo na
// aba Amigos do painel. OK num cartaz abre o titulo; Voltar fecha. Os dados
// vem so de socialvis.h (socialvis_perfil); o que o servidor ainda nao da sai
// como "—" ou como fileira vazia, nunca como numero inventado.
#ifndef NV_AMIGOPERFIL_H
#define NV_AMIGOPERFIL_H
#include <SDL2/SDL.h>
#include <stddef.h>

void amigoperfil_abrir(const char *pessoaId);
void amigoperfil_evento(const SDL_Event *e);
void amigoperfil_atualizar(float dt, Uint32 agora);
void amigoperfil_desenhar(Uint32 agora);
// 1 uma vez quando a pessoa pediu para sair (Voltar).
int  amigoperfil_quer_sair(void);
// 1 uma vez com o IMDb do cartaz em que a pessoa deu OK.
int  amigoperfil_pediu_titulo(char *imdb, size_t tam);
// TESTE (#99): a fileira em foco (-1 = nenhuma) e, em *coluna, o cartaz.
int  amigoperfil_teste_foco(int *coluna);
#endif
