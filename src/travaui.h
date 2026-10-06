// TRAVA DE SERIE — as telas (Glass UI, controle remoto). Plano, secao 5.2.
//
//   T1/T7  "Travar série com…" / "Gerenciar trava": um modal so. Pessoas da
//          conta (rosto, onde estao, selo SEM PIN), Modo (Juntos | Separados,
//          com a explicacao), "Pedir PIN para Assistir mesmo assim", a linha de
//          estado viva e os botoes. Abre do cadeado na linha de acoes da pagina
//          da serie e do cartao da trava no Perfil.
//   ESPERA o episodio alem do limite: miniatura borrada, rostos de quem falta,
//          "Esperando Jerry e Sandra", "Assistir mesmo assim" (com o PIN de
//          QUALQUER outro membro que tenha PIN, quando a trava pede).
//   T6     Perfil, pagina 2: "Travas de série", um cartao por trava com
//          rostos, posicoes e a frase; OK gerencia.
//
// O modelo e trava.h; a rede, travarede.h. FIO PRINCIPAL em tudo.
#ifndef NV_TRAVAUI_H
#define NV_TRAVAUI_H
#include <SDL2/SDL.h>
#include "catalogo.h"

// 1 quando ha perfis bastantes na conta (dois ou mais) para travar algo.
int  travaui_disponivel(void);
// 1 quando o perfil ativo esta numa trava desta serie.
int  travaui_tem_trava(const char *imdb);

// O GATE. (t, e) da serie esta segurado para o perfil ativo? `idxCat` (o
// indice do catalogo, -1 = nao se sabe) da a contagem de episodios por
// temporada. `perfis` (opcional) recebe os profile_index de quem falta; devolve
// 1 segurado, 0 liberado. Ja visto pelo proprio perfil (vistoep) = liberado.
int  travaui_segura(const char *imdb, int idxCat, int t, int e, int *perfis, int *n);
// "Esperando Jerry e Sandra" para a linha do cartao do episodio.
void travaui_esperando(const int *perfis, int n, char *dst, size_t tam);
// Os rostos de `perfis` em fila, discos de `d` com sobreposicao (cartao do
// episodio, cartao do Perfil).
void travaui_rostos(float x, float y, float d, const int *perfis, int n, float a);

// Abre o modal. Com trava desta serie para o perfil ativo, abre em Gerenciar.
int  travaui_abrir(const CatItem *ci, int idxCat);
int  travaui_abrir_gerenciar(const char *id);
// O episodio alem do limite: a folha de espera.
int  travaui_abrir_espera(int idxCat, int t, int e);
// app.c, ANTES de tocar o que a pagina do titulo pediu: se o episodio alvo
// esta segurado, abre a folha de espera e devolve 1 (nao toca).
int  travaui_segurar_detalhe(void);
// posplay.c: o proximo episodio esta segurado? Se sim, guarda o aviso "Voce
// alcancou Jerry" para a volta do player e devolve 1 (o cartao nao oferece).
int  travaui_proximo_segura(const char *imdb, int idxCat, int t, int e);

int  travaui_aberta(void);
void travaui_evento(const SDL_Event *e);
void travaui_atualizar(float dt, Uint32 agora);
void travaui_desenhar(Uint32 agora);

// PERFIL, PAGINA 2 ("Travas de série"). perfil.c entra com BAIXO no fim da
// primeira pagina e desenha isto no lugar dela. `evento` devolve 0 quando a
// tecla e para o perfil (CIMA no topo = voltar a pagina 1; Voltar).
void travaui_perfil_entrar(void);
int  travaui_perfil_evento(const SDL_Event *e);
void travaui_perfil_atualizar(float dt);
void travaui_perfil_desenhar(float x, float y, float w, float h, float a);

#ifdef NV_SHOT_HOOKS
// Captura: foco direto numa linha/coluna do modal, PIN digitado.
void travaui_shot_foco(int linha, int coluna);
void travaui_shot_pin(const char *digitos, int estado);
#endif

#endif
