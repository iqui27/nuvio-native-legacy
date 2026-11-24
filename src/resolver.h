// RESOLVER UM PROBLEMA (2.1): a pessoa escolhe o sintoma, o app testa na ordem
// em que o video passa e para na primeira falha, e o envio do registro devolve
// um codigo curto. Mockup: design/2.1/resolver.html (5A, 5B, 5C). A ordem dos
// testes e a guarda de 30 dias estao em resolverregra.h (testada sem SDL); aqui
// so a tela e a juncao com o resto do app.
//
// O QUE ESTA LIGADO DE VERDADE: internet (redesaude), add-ons (contagem e
// motivo da ultima busca vazia), debrid (conta sem plano / recusa), o ultimo
// erro de fonte do player (so em memoria, ate 6 h), a recusa da TV ao abrir o
// player, o envio do registro (o mesmo de Ajustes › Enviar registro) e a lista
// de registros enviados, guardada 30 dias em resolver-enviados.txt.
// O QUE AINDA NAO: o teste de Audio nao tem de onde ler (aparece "sem teste"),
// "Esconder fontes que esta TV nao toca" e "Mandar o codigo pro celular"
// aparecem apagados com "Em breve", e o estado "respondido" de cada registro
// precisa do servidor. Nenhum teste toca midia nem muda Ajustes.
#ifndef NV_RESOLVER_H
#define NV_RESOLVER_H
#include <SDL2/SDL.h>
#include <stdint.h>

#define RESOLVER_ARQ "resolver-enviados.txt"

// Abre a tela de qualquer lugar (Ajustes › Ajuda).
void resolver_abrir(void);
int  resolver_aberto(void);
float resolver_escala(void);   // fator de desenho (<1 quando a grade 1920x1080 nao cabe)
void resolver_evento(const SDL_Event *e);
void resolver_atualizar(float dt, Uint32 agora);
// Desenha a tela inteira, ou o fim do esmaecer de saida por cima do app.
void resolver_desenhar(Uint32 agora);

#define RESOLVER_PEDIU_NADA   0
#define RESOLVER_PEDIU_FONTES 1   // abrir Ajustes › Fontes e addons
#define RESOLVER_PEDIU_DEBRID 2   // abrir o login da conta de cache (addonguia, QR)
#define RESOLVER_PEDIU_PLAYER_EXT 3   // despachar a fonte falha ao player externo (Android)
int  resolver_pedido(void);
uint64_t resolver_tentativa_externo(void);

// ---- Para a captura e os testes.
enum { RESOLVER_FASE_SINTOMA = 0, RESOLVER_FASE_TESTES, RESOLVER_FASE_ENVIO };
int  resolver_fase(void);
int  resolver_foco(void);
int  resolver_sintoma(void);
int  resolver_enviados_n(void);
void resolver_teste_esquecer(void);
#ifdef RESOLVER_TESTE
#include "resolverregra.h"
#include "farolregra.h"
// Fixa os fatos lidos do app (o harness nao tem rede, add-ons nem player).
void resolver_teste_fatos(const ResolverFatos *f);
// Fixa os fatos do motor do Farol (plataforma, erro normalizado, contagens do
// anel) — com ela fixada, a coleta ao vivo nao roda.
void resolver_teste_fatos_farol(const FarolFatos *f);
// O cartao da fase 1 como montado pela cadeia inteira de producao:
// FAROL_C_* do veredito, estado[] por etapa, frases e as acoes EFETIVAS dos
// botoes (ja com a queda para a alternativa quando a da regra nao executa).
int          resolver_teste_veredito(void);
const int   *resolver_teste_estados(void);
const char  *resolver_teste_titulo(void);
const char  *resolver_teste_corpo(void);
int          resolver_teste_prim(void);
int          resolver_teste_sec(void);
// Roda a cadeia de producao inteira com o sintoma informado.
// sem simular teclas. `sintoma` = RES_S_*.
void resolver_teste_testar(int sintoma);
#endif

#endif
