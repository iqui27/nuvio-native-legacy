// Roteador de telas.
//
// Ate agora o main.c decidia sozinho entre home e detalhe, com um if. Com
// menu, busca, biblioteca, ajustes e player, esse if viraria um emaranhado onde
// cada tela precisa saber das outras. Aqui a regra fica num lugar so: existe
// uma tela CORRENTE, uma PILHA de retorno, e cada tela apenas diz "quero sair"
// ou "abre este titulo".
#ifndef NV_APP_H
#define NV_APP_H
#include <SDL2/SDL.h>

// TELA_LOGIN e TELA_ESCOLHA_PERFIL sao a excecao a ordem de prioridade: quando
// uma delas esta ativa NADA mais desenha nem recebe tecla. Sem conta nao ha
// catalogo do usuario, nao ha addons e nao ha progresso — deixar a home
// aparecer por tras seria mostrar o conteudo de exemplo do pacote como se
// fosse dele.
//
// TELA_ESCOLHA_PERFIL e a escolha de perfil DA CONTA; TELA_PERFIL, que ja
// existia, e a tela de estatisticas do Trakt. Nomes proximos, coisas
// diferentes.
typedef enum {
  TELA_LOGIN, TELA_ESCOLHA_PERFIL,
  TELA_HOME, TELA_EXPLORAR, TELA_GUIA, TELA_BUSCA, TELA_BIBLIOTECA, TELA_PERFIL, TELA_AJUSTES, TELA_DIAGNOSTICO,
  TELA_PLAYER, TELA_SOCIAL, TELA_ADDONS, TELA_AGENDA
} Tela;

int  app_iniciar(const char *dirArte);
void app_evento(const SDL_Event *e);
void app_atualizar(float dt, Uint32 agora);
void app_desenhar(Uint32 agora);
int  app_quer_sair(void);
void app_encerrar(void);

// Porta de teste: abre o titulo (imdb) como se viesse de uma recomendacao.
void app_abrir_titulo(const char *imdb);
#endif
