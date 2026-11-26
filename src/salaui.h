// SESSAO: A SALA DECIDE PELO CELULAR — a tela da TV e a pagina do celular.
//
// PRIMEIRA ETAPA (pesquisa de mercado 4.6): tudo na REDE LOCAL. A TV sobe o
// servidor de celular.h (o mesmo de "Digitar pelo celular": token no endereco,
// Host/Origin conferidos, nada sai da TV), mostra o QR, e cada celular que
// abre a pagina recebe os MESMOS cartoes. A regra (quem entrou, quem votou, o
// primeiro titulo com sim de todos) e de sala.h.
//
// O QUE A PAGINA EXPOE: so o baralho desta rodada (titulo, ano/genero curto e
// o endereco https do cartaz) e os nomes que os proprios visitantes digitaram.
// Nada da conta, do perfil, do historico ou dos add-ons. O baralho sai do
// catalogo que a Home ja mostra (filmes primeiro; series so se faltar); num
// perfil infantil so entram titulos de classificacao livre/10 anos
// (perfil_infantil_classif_ok).
//
// O QUE NAO E (ainda): sala pela internet (servidor de retransmissao), fila
// compartilhada, reacoes na tela e tocar sozinho. O vencedor abre a PAGINA do
// titulo; quem aperta Reproduzir e a pessoa.
//
// Sem servidor local (.wgt): salaui_disponivel() = 0 e a acao nao aparece.
// FIO PRINCIPAL.
#ifndef NV_SALAUI_H
#define NV_SALAUI_H
#include <SDL2/SDL.h>

int  salaui_disponivel(void);
void salaui_abrir(void);
void salaui_fechar(void);
int  salaui_aberta(void);
int  salaui_evento(const SDL_Event *e);     // 1 = consumiu
void salaui_atualizar(float dt);
void salaui_desenhar(void);
// O titulo vencedor, 1 UMA VEZ depois do OK: o app abre a pagina dele.
int  salaui_pediu_titulo(char *imdb, size_t n);
#endif
