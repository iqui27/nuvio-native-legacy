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
  TELA_PLAYER, TELA_SOCIAL, TELA_ADDONS, TELA_AGENDA,
  TELA_LIVETV_DIAG,  // diagnostico da Live TV (livetvdiag.c)
  TELA_PLUGINS       // plugins Nuvio (pluginsui.c, F09)
} Tela;

int  app_iniciar(const char *dirArte);
void app_evento(const SDL_Event *e);
void app_atualizar(float dt, Uint32 agora);
void app_desenhar(Uint32 agora);
int  app_quer_sair(void);
// Nome curto da tela em cena ("home", "player", "episodios"...), para o log.
const char *app_tela_nome(void);
// 1 quando CH+/CH- trocam de canal: guia aberto, ou um canal tocando (tela
// cheia ou no canto). Fora disso, no Android, CH+ e CH- fazem o papel das
// teclas AZUL e VERMELHA, que o controle nao tem (main.c).
int  app_zap_ativo(void);
// A Central de controle (central.h) pode abrir agora? Nao no login, na escolha
// de perfil, com o registro, o Spotlight ou um teclado na frente.
int  app_central_pode(void);
void app_encerrar(void);

// 1 = a home e o que esta na frente (sem detalhe, player, login ou escolha de
// perfil por cima). E a amostra do nivel de GPU (gpunivel.h).
int  app_na_home(void);
// Home owns the D-pad without keyboard-capturing layers (FPS scene guard).
int  app_home_recebe_teclas(void);
// A tela atual e o login (a abertura dissolve o logo sobre o fundo dele).
int  app_no_login(void);

// Porta de teste: abre o titulo (imdb) como se viesse de uma recomendacao.
void app_abrir_titulo(const char *imdb);
// Porta de teste: a funcao que app_atualizar chama antes de entregar a URL
// guardada ao player (tocarFonteGuardada). 1 = o player recebeu a URL.
int app_teste_tocar_guardada(void);
// Test hooks for the sheet latch (#513). cwTocar is Continue Watching only.
void app_teste_cw_tocar(int temporada, int episodio);
void app_teste_gravar_folha(int temporada, int episodio, int tem);
int app_teste_consumir_folha(int *temporada, int *episodio);
#endif
