// TRILHAS (2.1): a pagina da jornada. Mockup: design/2.1/trilhas.html (7A).
//
// Uma trilha e uma lista ordenada de filmes: o catalogo vem do Wikidata CC0
// (trilhasdados.h) e o progresso e o historico de vistos da conta e perfil.
// Sem trilha real, a tela nao abre e quem chama sabe disso pelo retorno 0.
#ifndef NV_TRILHAS_H
#define NV_TRILHAS_H
#include <SDL2/SDL.h>
#include "gfx.h"
#include "trilhasdados.h"

// `chave` e a do emblema ("trilha:<id>") ou o id. 0 = trilha desconhecida.
int  trilhas_abrir(const char *chave);
// A partir do titulo em cena (detail.c): 1 se ele pertence a uma trilha.
int  trilhas_abrir_titulo(const char *imdb, const char *tipo);
int  trilhas_aberta(void);
int  trilhas_foco(void);
int  trilhas_poster(const CqPasso *p, char *url, size_t tam);
void trilhas_evento(const SDL_Event *e);
void trilhas_desenhar(Uint32 agora);

const char *trilhas_titulo(void);   // "" quando fechada
int  trilhas_passo(void);           // foco no passo; -1 = nao iniciada
int  trilhas_passos(void);
int  trilhas_vistos(void);
int  trilhas_total(void);
int  trilhas_primeiro_visivel(void);
// Selected title is opened by the app router; Back restores this trail.
int  trilhas_pediu_titulo(CqPasso *dst);
void trilhas_voltar_do_titulo(void);
// Badge request includes its actual key, consumed once.
int  trilhas_pediu_conquistas(char *chave, size_t tam);
void trilhas_teste_esquecer(void);

#endif
