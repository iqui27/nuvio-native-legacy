// Folha de AUDIO E LEGENDA, aberta pelos icones do player.
//
// Duas colunas num painel so, como no aparelho: a esquerda o audio, a direita
// a legenda. Separar em duas telas obrigaria a sair e voltar para conferir o
// par escolhido, que e justamente o que se quer comparar.
//
// A lista de legendas junta as EMBUTIDAS no arquivo (o pipeline as enxerga) com
// as do OpenSubtitles (baixadas pelo addon). Sao coisas diferentes na origem e
// a mesma coisa para quem assiste, entao aparecem juntas, marcadas.
#ifndef NV_FAIXAS_H
#define NV_FAIXAS_H
#include <SDL2/SDL.h>
#include "addons.h"

// Zera o que e da SESSAO e nao do aparelho — hoje, qual legenda externa esta
// valendo. Chamada pelo player quando uma reproducao nova comeca.
void faixas_reiniciar(void);
#ifdef NV_ANDROID
void faixas_reiniciar_preparada(void);
#endif

void faixas_abrir(void);
// Abre com o foco JA na coluna pedida: 0 = audio, 1 = legenda. O player tem um
// icone para cada, e abrir sempre no audio fazia os dois parecerem o mesmo
// botao.
void faixas_abrir_em(int col);
int  faixas_aberta(void);
// A folha de legenda esta como BARRA DE ESTILO no topo (foco na coluna
// Estilo): o player desenha a legenda onde ela vai tocar, com uma linha de
// previa quando nao ha fala naquele momento.
int  faixas_estilo_topo(void);
// Abertura animada da folha (0..1): o player apaga o OSD por baixo dela.
float faixas_anim(void);
void faixas_evento(const SDL_Event *e);
void faixas_atualizar(float dt, Uint32 agora);
// OK na pilula "Nenhuma legenda em ...": abre a lista de legendas. 1 = tratou a tecla.
int  faixas_pilula_tecla(const SDL_Event *e);
void faixas_desenhar(Uint32 agora);
// PONTEIRO (#99): as abas "Faixas | Estilo" da folha de legenda (o mesmo que
// ESQUERDA/DIREITA entre a lista e a barra de estilo). 0 = lista, 1 = estilo.
void faixas_aba_estilo(int estilo);
// Testes: o foco da folha. Devolve a linha focada da coluna atual (-1 = folha
// fechada); `col` 0 = audio, 1 = lista de legendas, 2 = barra de estilo;
// `vol`/`vel` = foco nas linhas de volume/velocidade da folha de audio.
int  faixas_teste_foco(int *col, int *vol, int *vel);
// Testes: o item da aba Estilo cujo controle (slider, paleta, fontes) esta aberto; -1 = a grade.
int  faixas_estilo_controle(void);

// F04: what the subtitle selector (legendasui.c) needs from the primary.
// Combined index of the active primary (embedded first, then addons; -1 none).
int  faixas_legenda_ativa(void);
// Primary collector rendering waits until native fallback hands over ownership.
int  faixas_legenda_overlay(void);
// Opaque identity of the active external primary ("" = none).
const char *faixas_legenda_externa_id(void);
// Choose the primary: embedded track `i` (-1 = off) / an addon entry COPY.
void faixas_escolher_embutida(int i);
void faixas_escolher_externa(const Legenda *copia);
// The ASS/collector state label of embedded track `i` (NULL = nothing to say).
const char *faixas_legenda_marca(int i);

#endif
