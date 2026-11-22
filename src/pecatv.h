// "PECA COMO FALA", PELO CELULAR (pesquisa 4.5): a parte da TV.
//
// O cartao de pareamento (QR de um endereco que nao muda), o "pensando" e os
// TRES titulos confirmados, por cima de qualquer tela. Tambem e quem junta as
// pecas: pecaserv (o celular), pecafio (Gemini + Cinemeta), o catalogo (o
// historico e os salvos que viram contexto) e os arquivos em dados:
//   peca_token.txt   o token do endereco; existir = pareado
//   gemini.txt       a chave pessoal do Gemini (credencial: so nesta TV,
//                    nunca na conta, no ajustes.txt nem em log)
// Tudo morre com "Peça como fala" desligado em Ajustes: sem porta, sem pedido.
#ifndef NV_PECATV_H
#define NV_PECATV_H
#include <SDL.h>

// 1 onde da para ouvir a rede local (nao no .wgt nem no Windows).
int  pecatv_disponivel(void);
// Abre o cartao do QR; pareia (gera o token e sobe a porta) se preciso.
void pecatv_parear(void);
int  pecatv_pareado(void);
int  pecatv_tem_chave(void);
int  pecatv_aberto(void);
void pecatv_fechar(void);
// `podeMostrar` = 0 com o player ou o login na tela: o pedido ainda e
// atendido e o celular ve os titulos, mas nada cobre a TV nem abre titulo.
void pecatv_atualizar(float dt, int podeMostrar);
int  pecatv_evento(const SDL_Event *e);
void pecatv_desenhar(void);
#endif
