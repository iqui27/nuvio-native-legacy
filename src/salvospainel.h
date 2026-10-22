// PAINEL "SALVOS": a camada que desliza da direita na tecla AZUL.
//
// Ele SUBSTITUI o painel "Sua atividade" que perfil.c desenhava no mesmo lugar
// (perfil_abrir_lateral / perfil_lateral, removidos). A troca foi pedida assim:
// o atalho mais rapido do controle passa a responder "o que eu guardei para
// ver?" em vez de "quantos minutos assisti este mes?" — a primeira e uma
// pergunta que leva a uma acao, a segunda e um relatorio, e relatorio tem tela
// propria (MENU_PERFIL, agora abrindo TELA_PERFIL direto).
//
// Ele nao e uma tela: e uma camada por cima do conteudo, e enquanto esta
// visivel toma o D-pad. Mesma disciplina de menu.h — sem encerrar (usa os
// caches de text.c/tex_cache.c) e sem quer_sair (fechar o painel nunca fecha o
// app).
#ifndef NV_SALVOSPAINEL_H
#define NV_SALVOSPAINEL_H
#include <SDL2/SDL.h>
#include <stddef.h>

void spainel_abrir(void);
void spainel_fechar(void);
// NASCER DA ILHA (ilha.h, com o relogio na tela): o painel abre crescendo do
// retangulo (x, y, w, h) — a pilula, ou o modal dela — ate a forma dele, em vez
// de deslizar da borda. `spainel_recolher_para` e chamado por quadro com o
// retangulo atual da pilula (ok = 0 quando ela nao esta na tela): e para la
// que ele encolhe ao fechar. Sem ilha, o painel e o de sempre.
void spainel_abrir_de(float x, float y, float w, float h);
void spainel_recolher_para(int ok, float x, float y, float w, float h);
// 1 enquanto o painel visivel nasceu da ilha (a pilula fica coberta por ele).
int  spainel_da_ilha(void);
// 1 enquanto a camada e dona do D-pad. Cai para 0 no instante da escolha, com a
// animacao de saida ainda rodando — e esse o sinal para o conteudo voltar a
// responder as teclas, senao o D-pad fica morto durante o recolhimento (a mesma
// regra de menu_aberto).
int  spainel_aberto(void);
// 1 enquanto ainda ha pixel para desenhar, incluindo a saida.
int  spainel_visivel(void);

void spainel_evento(const SDL_Event *e);
void spainel_atualizar(float dt, Uint32 agora);
void spainel_desenhar(Uint32 agora);

// O que fica ATRAS do painel. Chame no lugar de desenhar o fundo direto:
// com o painel inteiro na tela e `podeParar`, `fundo` e pintado uma vez num
// FBO com o veu por cima e os quadros seguintes so copiam; `rev` diferente da
// ultima refaz a copia. Fora disso, `fundo` e desenhado direto. Ver a nota
// em salvospainel.c.
void spainel_fundo(int podeParar, unsigned rev, void (*fundo)(void *), void *ctx);
// Quantas vezes a lista foi montada desde o arranque (tests/salvospainel.sh).
int  spainel_n_reconstrucoes(void);
// Para os testes de captura: o tipo da linha em foco na aba Amigos ("pedido",
// "encontrar", "adicionar", "previa", "alcance", "trakt", "simkl",
// "letterboxd", "aparecer", "amigo", "rec", "sug", "" fora da lista).
const char *spainel_foco_social(void);
// Quantas vezes o fundo parado foi pintado no FBO.
int  spainel_n_fundos(void);
// Quantas linhas da aba Salvos estao em "Continuar" (com progresso). Testes.
int  spainel_n_continuar(void);

// IMDb do titulo que o dono escolheu, ou NULL. Consumido uma vez. Quem resolve
// o id no catalogo e abre o detalhe e o roteador (app.c) — o painel nao conhece
// nem detail.c nem a descoberta, exatamente como perfil.c nao conhecia.
const char *spainel_pediu_abrir(void);
// OK na linha de um amigo (aba Amigos): o id da pessoa, uma vez, para app.c
// abrir o perfil (amigoperfil.h). O painel ja fechou.
int spainel_pediu_perfil(char *id, size_t tam);
// 1 uma vez quando a pessoa pediu a tela Agenda completa pela aba Agenda.
int spainel_pediu_agenda(void);
// Para os testes: abre direto numa aba (0 Salvos, 1 Atividade, 2 Amigos,
// 3 Avisos), com o foco na primeira linha da lista.
void spainel_ir_aba(int aba);
// Testes: a aba aberta e o indice do foco (< 0: abas ou barra).
int spainel_aba_atual(void);
int spainel_foco_indice(void);
// Abre (se preciso) na aba Atividade e poe o foco no primeiro evento de amigo
// do titulo `imdb` (a lista rola ate ele). Sem evento desse titulo, so a aba.
void spainel_abrir_titulo(const char *imdb);

#endif
