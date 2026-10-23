// TRILHAS (2.1): a pagina da jornada. Mockup: design/2.1/trilhas.html (7A).
//
// Uma trilha e uma lista ordenada de filmes: o catalogo vem do Wikidata CC0
// (trilhasdados.h) e o progresso e o historico de vistos da conta e perfil.
// Sem trilha real, a tela nao abre e quem chama sabe disso pelo retorno 0.
#ifndef NV_TRILHAS_H
#define NV_TRILHAS_H
#include <SDL2/SDL.h>
#include "gfx.h"

// `chave` e a do emblema ("trilha:<id>") ou o id. 0 = trilha desconhecida.
int  trilhas_abrir(const char *chave);
// A partir do titulo em cena (detail.c): 1 se ele pertence a uma trilha.
int  trilhas_abrir_titulo(const char *imdb, const char *tipo);
int  trilhas_aberta(void);
void trilhas_evento(const SDL_Event *e);
void trilhas_desenhar(Uint32 agora);

const char *trilhas_titulo(void);   // "" quando fechada
int  trilhas_passo(void);           // foco no passo; -1 = nao iniciada
int  trilhas_passos(void);
int  trilhas_vistos(void);
int  trilhas_total(void);
// Tela a abrir depois que a trilha fechar (conquistas ou nada). Consome.
int  trilhas_pediu_conquistas(void);
void trilhas_teste_esquecer(void);

#endif
