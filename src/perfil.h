// Tela de perfil e estatisticas do Trakt.
//
// O modulo e deliberadamente "burro" sobre rede: ele recebe um snapshot de
// dados pronto, guarda uma copia e desenha. Isso permite ao app buscar Trakt em
// uma worker sem jamais bloquear o quadro de SDL/GLES da TV.
#ifndef NV_PERFIL_H
#define NV_PERFIL_H

#include <SDL2/SDL.h>
#include <stdint.h>

#define PERFIL_MAX_GENEROS    8
#define PERFIL_MAX_DESTAQUES  6
#define PERFIL_MAX_DIAS       42

typedef struct {
  char nome[40];
  int  quantidade;
  uint32_t cor;              // 0xRRGGBB; zero usa a paleta da tela
} PerfilGenero;

typedef struct {
  char id[64];               // imdb/trakt/slug, devolvido sem interpretacao
  char titulo[128];
  char detalhe[96];          // ex.: "T2E3 · Solo" ou "Filme"
  char poster[768];
  char backdrop[768];
  int  plays;
  int  minutos;
} PerfilDestaque;

typedef struct {
  char nome[96];
  char usuario[64];
  char avatar[768];
  char periodo[48];          // ex.: "SETEMBRO 2026"

  int minutos;
  int plays;
  int filmes;
  int episodios;

  int streakAtual;
  int streakAnterior;
  int diasAtivosMes;
  int diasAtivosAno;
  int primeiroDiaSemana;     // 0=domingo..6=sabado
  int nDias;
  unsigned short atividade[PERFIL_MAX_DIAS];

  int nGeneros;
  PerfilGenero generos[PERFIL_MAX_GENEROS];
  int nDestaques;
  PerfilDestaque destaques[PERFIL_MAX_DESTAQUES];
  // Zero e o padrao seguro para produtores antigos. O snapshot mensal nao
  // comprova cobertura anual nem uma sequencia que cruza a virada do mes.
  int parcial;
  int anoCompleto;
  int streakCompleto;
  char aviso[160];
} PerfilDados;

typedef enum {
  PERFIL_ESTADO_CARREGANDO = 0,
  PERFIL_ESTADO_ATUALIZANDO,
  PERFIL_ESTADO_PRONTO,
  PERFIL_ESTADO_STALE,
  PERFIL_ESTADO_ERRO,
  PERFIL_ESTADO_SEM_ATIVIDADE,
  PERFIL_ESTADO_PRIVADO,
  PERFIL_ESTADO_DESCONECTADO,
  PERFIL_ESTADO_INDISPONIVEL
} PerfilEstado;

// Identificador opaco de uma obra selecionada. O roteador resolve o item no
// catalogo atual antes de abrir o detalhe, sem inventar metadados.
typedef struct {
  char id[64];
  char titulo[128];
} PerfilItemSelecionado;

int  perfil_iniciar(void);
void perfil_encerrar(void);

void perfil_abrir(void);
// perfil_abrir_lateral / perfil_lateral / perfil_pediu_completo FORAM REMOVIDAS.
//
// Eram o painel "Sua atividade": um cartao de 776px na direita, com "Fechar",
// "Atualizar" e "Ver perfil completo", aberto pela tecla AZUL e pelo item
// "Perfil e Stats" do menu. Os dois caminhos agora vao direto ao que
// prometiam — a AZUL abre o painel de Salvos (salvospainel.h) e o item do menu
// abre esta tela inteira. Sem chamador, o modo lateral era codigo morto
// desenhando um terceiro estado que ninguem mais alcancava.
void perfil_fechar(void);
int  perfil_aberto(void);
int  perfil_quer_sair(void);       // consome o pedido de voltar

// Enquanto carrega, a tela preserva a estrutura com esqueletos. Um snapshot
// NULL/sem atividade produz o estado vazio, nunca numeros inventados.
void perfil_definir_carregando(int carregando);
void perfil_definir_dados(const PerfilDados *dados);
// Preserva o ultimo snapshot ao falhar uma atualizacao. Sem snapshot, OK
// pede nova tentativa. O chamador faz a rede e consome a flag abaixo.
void perfil_definir_erro(const char *mensagem);
void perfil_definir_estado(PerfilEstado estado, const char *mensagem);
PerfilEstado perfil_estado(void);
int  perfil_pediu_atualizar(void);

void perfil_evento(const SDL_Event *e);
void perfil_atualizar(float dt, Uint32 agora);
void perfil_desenhar(Uint32 agora);

// Retorna 1 uma vez apos OK num destaque. `saida`, se nao NULL, recebe copia
// estavel do item para o app abrir detalhes sem depender do storage interno.
int perfil_item_selecionado(PerfilDestaque *saida);

#endif
