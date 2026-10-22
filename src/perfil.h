// Tela de perfil e estatisticas de Trakt ou Simkl.
//
// O modulo e deliberadamente "burro" sobre rede: ele recebe um snapshot de
// dados pronto, guarda uma copia e desenha. Isso permite ao app buscar o
// historico em uma worker sem jamais bloquear o quadro de SDL/GLES da TV.
#ifndef NV_PERFIL_H
#define NV_PERFIL_H

#include <SDL2/SDL.h>
#include "perfildados.h"
#include <stddef.h>

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
// Botao Conquistas do cabecalho (resumo): OK nele pede a tela de conquistas;
// quem a abre e o roteador. O segundo diz se o foco esta nele (testes).
int  perfil_pediu_conquistas(void);
int  perfil_foco_conquistas(void);

void perfil_evento(const SDL_Event *e);
void perfil_atualizar(float dt, Uint32 agora);
void perfil_desenhar(Uint32 agora);

// Retorna 1 uma vez apos OK num destaque. `saida`, se nao NULL, recebe copia
// estavel do item para o app abrir detalhes sem depender do storage interno.
int perfil_item_selecionado(PerfilDestaque *saida);

// Retorna 1 uma vez apos OK no seletor do duelo ("Voce e Fulano"): `id`
// recebe o pessoaId de socialvis para o app abrir o perfil do amigo.
int perfil_pediu_amigo(char *id, size_t tam);
// Depois de perfil_abrir, na volta do perfil do amigo: reabre o duelo.
void perfil_voltar_ao_duelo(void);

#endif
