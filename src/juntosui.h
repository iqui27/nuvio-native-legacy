// WATCH TOGETHER — a interface (plano, secao 5), no Glass UI do app.
//
//  - FOLHA "Assistir juntos" (pagina do titulo): quem vai assistir (amigos),
//    "So eu controlo", Abrir sala, Entrar com codigo.
//  - CONVITE: aviso na ilha do relogio com o rosto de quem chamou, que cresce
//    no modal Entrar / Agora nao / Nao receber convites de X.
//  - CARTAO DA SALA: quem esta e em que estado (de verdade, reportado por cada
//    TV), a fonte de cada um, o codigo, Comecar, o QR do celular e Sair.
//  - NO PLAYER: os rostos de quem esta junto no canto quando o OSD sobe, os
//    avisos da sala ("Rafa pausou", "Esperando Fabi"), as reacoes que chegam
//    (rosto + icone) e a regua de reacoes ao SEGURAR OK com o OSD escondido.
//    VOLTAR pergunta "Sair da sessao?".
//
// FIO PRINCIPAL. juntos.c chama as funcoes de evento; app.c chama as de tela.
#ifndef NV_JUNTOSUI_H
#define NV_JUNTOSUI_H
#include <SDL2/SDL.h>
#include "juntosmotor.h"

typedef struct {
  char id[40], ep[64], titulo[160], poster[512];
  char deId[96], deNome[64], deAv[256];
  JsImpressao imp;
} JuntosConvite;

// --- de juntos.c -----------------------------------------------------------------------
void juntosui_convite(const JuntosConvite *c);
void juntosui_evento(const JsEvento *e);
void juntosui_erro(const char *k);
void juntosui_sala_abriu(void);
void juntosui_sala_fechou(void);
void juntosui_fonte_diferente(int veredito, int pal);
void juntosui_perguntar_sair(void);
void juntosui_acao(const char *chave, int botao);

// --- de app.c ---------------------------------------------------------------------------
// O botao "Assistir juntos" do detalhe: abre a folha para este titulo/episodio.
void juntosui_abrir_folha(int indiceCatalogo, int temporada, int episodio);
int  juntosui_evento_sdl(const SDL_Event *e);   // 1 = consumiu
void juntosui_atualizar(Uint32 agora);
void juntosui_desenhar(Uint32 agora);
int  juntosui_aberta(void);                      // alguma camada nossa com foco
// "Sair da sessao" confirmado com o player aberto: 1 uma vez, e o app fecha o
// player pelo caminho normal do VOLTAR.
int  juntosui_pediu_fechar_player(void);
// "Abrir sala" na folha: 1 uma vez, e o app reproduz o titulo como o
// Reproduzir do detalhe (a sala nasce quando a fonte tocar).
int  juntosui_pediu_reproduzir(void);

#endif
