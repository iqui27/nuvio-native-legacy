// COMEÇAR (2.1): primeiro uso guiado em 4 passos — perfil, trazer suas coisas,
// acompanhar (trackers) e o resumo com o tour opcional. Mockup:
// design/2.1/comecar.html. A regra de quando aparece e o que fica gravado esta
// em comecarregra.h (testada sem SDL); aqui so a tela.
//
// O QUE ESTA LIGADO DE VERDADE: o login do Trakt e do Simkl (o mesmo codigo de
// Ajustes), o nome digitado no teclado da TV e o que ficou gravado em
// comecar.txt. O QUE AINDA NAO: nome e cor so valem para a saudacao do resumo —
// nao criam nem renomeiam o perfil da conta. Conta Stremio e AIOManager
// aparecem apagados, com "Em breve". Nao ha "perfil infantil": o app nao tem
// filtro de classificacao, e a tela nao promete o que nao cumpre (NOTAS.md,
// pergunta 11).
//
// SAIDAS: Voltar no passo 1 pula o fluxo todo (grava feito, mais nada); Voltar
// nos passos 2-4 volta um passo; Voltar segurado ou "Pular tudo" pulam de
// qualquer passo. So concluir e pular gravam; fechar o app no meio nao deixa nada.
#ifndef NV_COMECAR_H
#define NV_COMECAR_H
#include <SDL2/SDL.h>

#define COMECAR_ARQ "comecar.txt"

// Uma vez no arranque, ANTES do login e do sync: anota se a instalacao ja tinha
// dados de pessoa (sessao, perfis, progresso). Quem tem nunca ve o Começar.
void comecar_marcar_dados(void);
// Uma vez, com a home de pe. `loginNovo` = 1 quando a conta entrou agora por
// login, 0 quando a sessao veio restaurada (ver comecarregra_decidir).
void comecar_decidir(int loginNovo);
// Abre o fluxo completo de qualquer lugar (Ajustes › Sobre e ajuda).
void comecar_abrir(void);

int  comecar_aberto(void);
void comecar_evento(const SDL_Event *e);
void comecar_atualizar(float dt, Uint32 agora);
// Desenha a tela inteira, ou o fim do esmaecer de saida por cima do app.
void comecar_desenhar(Uint32 agora);

#define COMECAR_PEDIU_NADA 0
#define COMECAR_PEDIU_TOUR 1   // abrir o tour (novidades20_abrir)
int  comecar_pedido(void);

// ---- Para a captura e os testes.
int  comecar_passo(void);        // 0..3
int  comecar_foco(void);
void comecar_teste_esquecer(void);

#endif
