// O FAROL (2.1; antes "Resolver um problema"): a pessoa escolhe o sintoma, o
// app testa na ordem em que o video passa e para na primeira falha, e o envio
// do registro devolve um codigo curto. Mockup: design/2.1/resolver.html (5A,
// 5B, 5C); telas Inicio, Cadeia e Veredito do PLANO 3.2 (issue 19). A ordem
// dos testes e a guarda de 30 dias estao em resolverregra.h (testada sem SDL);
// o julgamento e da fachada farol.h (farol_julgar/farol_veredito); aqui so a
// tela e a juncao com o resto do app.
//
// O QUE ESTA LIGADO DE VERDADE: internet (redesaude), add-ons (contagem e
// motivo da ultima busca vazia), debrid (conta sem plano / recusa), o ultimo
// erro de fonte do player (so em memoria, ate 6 h), a recusa da TV ao abrir o
// player, os playbooks e eventos de farol.h (farol_julgar, farol_veredito,
// farol_registrar_acao/resultado), a sonda ativa de fonte (farolsonda.h), o
// envio do registro (gancho da #22 via Ajustes › Enviar registro) e a lista de
// registros enviados, guardada 30 dias em resolver-enviados.txt.
// O QUE AINDA NAO: "Achar o que ver" (#25) aparece reservado e fora do foco
// com "Em breve", acoes sem executor (#15-#18, #20, #33, #34) caem na
// alternativa ou no envio de registro, "Esconder fontes que esta TV nao toca"
// e "Mandar o codigo pro celular" aparecem apagados com "Em breve", e o estado
// "respondido" de cada registro precisa do servidor. Nenhum teste toca midia
// nem muda Ajustes.
#ifndef NV_RESOLVER_H
#define NV_RESOLVER_H
#include <SDL2/SDL.h>
#include <stdint.h>
#include "farolachaui.h"

#define RESOLVER_ARQ "resolver-enviados.txt"

// Abre a tela de qualquer lugar (Ajustes › Ajuda).
void resolver_abrir(void);
// Abre ja testando `sintoma` (RES_S_*): a tela cai no veredito, sem a pessoa
// escolher o sintoma (o aviso do Farol na ilha, issue 20). Fora da faixa = abrir.
void resolver_abrir_sintoma(int sintoma);
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
// OK num cartaz de "Achar o que ver" (#25): devolve 1 UMA vez com a identidade
// completa do cartaz selecionado (`FarolAchaPedido`) para validacao no
// catalogo vivo em `app.c` antes de abrir o detalhe e fechar o Farol.
int  resolver_pediu_abrir(FarolAchaPedido *pedido);
int  resolver_resolver_pedido(const FarolAchaPedido *pedido, unsigned *revisaoOut);
int  resolver_pedido_bate(const CatItem *c, const FarolAchaPedido *pedido);
void resolver_fechar(void);
void resolver_recarregar_achar(void);
uint64_t resolver_tentativa_externo(void);

// ---- Para a captura e os testes.
// FAROL (issue 19, PLANO 3.2): Inicio (SINTOMA), Cadeia (TESTES: a luz percorre
// as etapas e, com a sonda ativa no ar, segura a linha em "testando…") e
// Veredito (o cartao com a acao; o foco nasce nela). ENVIO e o do registro.
// ACHAR e a tela 7 ("Achados", #25).
enum { RESOLVER_FASE_SINTOMA = 0, RESOLVER_FASE_TESTES, RESOLVER_FASE_ENVIO, RESOLVER_FASE_VEREDITO, RESOLVER_FASE_ACHAR };
#define RESOLVER_FASE_INICIO RESOLVER_FASE_SINTOMA
#define RESOLVER_FASE_CADEIA RESOLVER_FASE_TESTES
// Ids de foco que nao sao cartao de sintoma (os cartoes usam o RES_S_* do
// sintoma, inclusive RES_S_CORES).
enum {
  RESOLVER_B_BANNER = 50, RESOLVER_B_ENVIAR, RESOLVER_B_LISTA, RESOLVER_B_PRINC,
  RESOLVER_B_SEC, RESOLVER_B_OUTRO, RESOLVER_B_PRONTO, RESOLVER_B_REPETIR,
  RESOLVER_B_ACHAR,
  RESOLVER_B_ACHAR_CURTO = RESOLVER_B_ACHAR,
  RESOLVER_B_ACHAR_NAO_VI,
  RESOLVER_B_ACHAR_RETOMAR,
  RESOLVER_B_ACHAR_SURPRESA,
  RESOLVER_B_ACHAR_VOLTAR,
  RESOLVER_B_ACHAR_VOZ,
  RESOLVER_B_LIN0 = 200
};
int  resolver_fase(void);
float resolver_luz(void);           // onde a luz da Cadeia esta (0..RES_T_N, em etapas)
int  resolver_testando(void);       // 1 = a sonda ativa segura a Cadeia ("testando…")
int  resolver_mandar_registro(void); // 1 = o Veredito oferece "Mandar registro" (#22)
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
// Roda a cadeia de producao inteira com o sintoma informado e cai direto no
// Veredito, sem simular teclas. `sintoma` = RES_S_*.
void resolver_teste_testar(int sintoma);
// O FAROL_A_* da ultima acao despachada cujo resultado ainda nao foi medido
// (0 = nenhuma): o proximo julgamento do mesmo sintoma fecha a conta.
int  resolver_teste_acao_pendente(void);
#endif

#endif
