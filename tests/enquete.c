// Enquete na ilha (N3): o cliente contra um SERVIDOR DE MENTIRA em C que imita
// servidor/recomendacoes/src/enquete.js (a mesma forma de JSON, 409/410, opt-out,
// contagem so depois do voto). Sem rede, sem janela.
//
//   bash tests/enquete.sh
//
// Inclui o .c de proposito (funcoes estaticas), como tests/recomenda.c, e troca
// os tres pontos de contato por #define ANTES do include.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NV_REC_URL "http://fake.test"
#define rede_baixar_st         teste_get
#define rede_postar_st         teste_post
#define recomenda_cabecalhos   teste_cabecalhos
// A AZUL no relogio com a bolinha (ilha.c so a entrega com a pilula na tela).
static int pontoTeste;
static int teste_ponto_pediu(void) { int p = pontoTeste; pontoTeste = 0; return p; }
static void pontoPedirTeste(void) { pontoTeste = 1; }
#define ilha_ponto_pediu       teste_ponto_pediu
#include "../src/enquete.c"

#include "enquete_servidor.inc"

// --- roteiro -----------------------------------------------------------------------
static Uint32 T0 = 1000;
static void gira(int quadros) {   // o laco principal: um quadro a cada 10 ms, relogio do teste adiantado
  int i;
  for (i = 0; i < quadros; i++) { T0 += 50; enquete_passo(T0); usleep(10000); }
}
static void novaSessao(void) {    // "fechou e abriu o app": estado em memoria zera, disco fica
  gen++;
  memset(&cur, 0, sizeof cur);
  buscando = 0; proxima = 0; pediuVoto = 0; abrindo = 0; nVistos = -1;
  ilha_retirar(CH_CONVITE); ilha_retirar(CH_OPCOES); ilha_retirar(CH_RESULTADO);
  ilha_ponto_enquete(0); ponto = 0;
  gira(2);
}
static void serv(const char *id, long long dur) {
  memset(&S, 0, sizeof S);
  snprintf(S.id, sizeof S.id, "%s", id);
  S.ativa = 1; S.fim = (long long)time(NULL) + dur;
  S.base[0] = 4; S.base[1] = 9; S.base[2] = 2;
}
static void espera(int *v, int alvo) { int i; for (i = 0; i < 300 && *v != alvo; i++) gira(1); }

int main(void) {
  const char *d;
  dados_iniciar(NULL);
  d = dados_dir();
  char *tmp;
  assert(d && d[0] && strstr(d, "nuvio-enq-dados"));   // so escreve na pasta do teste
  // Missing identity performs no HTTP and does not retry every minute.
  mtx = SDL_CreateMutex();
  Op op = { .tipo = OP_BUSCAR, .gen = gen };
  semIdentidade = 1;
  executar(&op);
  assert(S.nGet == 0);
  agoraAtual = T0;
  aplicar(&caixa);
  assert(proxima - agoraAtual > 60000u);
  semIdentidade = 0;
  getStatus = 401;
  executar(&op); /* Also checks room for a full bearer. */
  aplicar(&caixa);
  assert(S.nGet == 1 && proxima - agoraAtual > 60000u);
  getStatus = 0;
  caixaCheia = 0; proxima = 0;
  S.nGet = 0;
  // 1. Nada antes de o app assentar (nao bloqueia a abertura).
  serv("logo-1", 3 * 86400);
  enquete_passo(T0); enquete_passo(T0 + 100);
  assert(S.nGet == 0 && !ilha_tem(CH_CONVITE));
  // 2. Depois de ESPERA_MS: busca, convite UMA vez, bolinha ligada.
  T0 += ESPERA_MS + 100;
  espera(&S.nGet, 1);
  gira(40);
  assert(S.nGet == 1 && cur.tem && !strcmp(cur.id, "logo-1") && cur.n == 3 && !strcmp(cur.texto[1], "Clássico renovado"));
  assert(ilha_tem(CH_CONVITE) && ponto == 1 && foiVisto("logo-1"));
  tmp = dados_ler(ARQ_VISTO); assert(tmp && strstr(tmp, "logo-1")); free(tmp);
  // 3. "Agora nao": a bolinha fica, nada mais abre. Reabre depois, SEM convite novo.
  enquete_acao(CH_CONVITE, 2);
  assert(ponto == 1 && !ilha_tem(CH_OPCOES));
  novaSessao();
  S.nGet = 0;
  T0 += ESPERA_MS + 100;
  espera(&S.nGet, 1);
  gira(40);
  assert(cur.tem && !ilha_tem(CH_CONVITE) && ponto == 1);   // convite dito uma vez so
  // 4. Responder -> opcoes; votar na 2 -> resultado com contagem agregada.
  enquete_acao(CH_CONVITE, 1);
  assert(ilha_tem(CH_OPCOES));
  enquete_acao(CH_OPCOES, 2);
  espera(&S.nVoto, 1);
  gira(40);
  assert(S.voto == 2 && cur.voto == 2 && cur.total == 16 && cur.votos[1] == 10 && ponto == 0 && ilha_tem(CH_RESULTADO));
  // 5. Votar de novo nao chama o servidor.
  enquete_acao(CH_OPCOES, 1);
  gira(10);
  assert(S.nVoto == 1);
  // 6. Quem ja votou em outra TV: sem convite, sem bolinha.
  novaSessao();
  S.nGet = 0; T0 += ESPERA_MS + 100;
  espera(&S.nGet, 1); gira(40);
  assert(cur.tem && cur.voto == 2 && !ilha_tem(CH_CONVITE) && ponto == 0);
  // 7. Falha no voto: aviso de erro, nada gravado, pode tentar de novo.
  serv("logo-2", 86400);
  novaSessao(); T0 += ESPERA_MS + 100; espera(&S.nGet, 1); gira(40);
  assert(ilha_tem(CH_CONVITE));
  enquete_acao(CH_CONVITE, 1);
  S.vote500 = 1;
  enquete_acao(CH_OPCOES, 1);
  espera(&S.nVoto, 1); gira(30);
  assert(cur.voto == 0 && ilha_tem("enquete:e1") && ponto == 1 && !pediuVoto);
  S.vote500 = 0;
  enquete_acao(CH_OPCOES, 3);
  espera(&S.nVoto, 2); gira(40);
  assert(S.voto == 3 && cur.voto == 3);
  // 8. Enquete vencida: nada.
  serv("logo-3", -10);
  novaSessao(); T0 += ESPERA_MS + 100; espera(&S.nGet, 1); gira(30);
  assert(!cur.tem && !ilha_tem(CH_CONVITE) && ponto == 0);
  // 9. Nao receber mais: conta (servidor) + espelho local + Desfazer pela ilha.
  serv("logo-4", 86400);
  novaSessao(); T0 += ESPERA_MS + 100; espera(&S.nGet, 1); gira(40);
  assert(ilha_tem(CH_CONVITE) && ajustes_enquetes());
  enquete_acao(CH_CONVITE, 3);
  espera(&S.nOptout, 1); gira(40);
  assert(S.optout == 1 && !ajustes_enquetes() && ponto == 0 && ilhaacao_tem_desfazer());
  tmp = dados_ler(ARQ_PEND); assert(!tmp); // confirmado pelo servidor: pendente apagado
  ilhaacao_botao(1);                        // Desfazer
  espera(&S.nOptout, 2); gira(10);
  assert(S.optout == 0 && ajustes_enquetes());
  // 10. Mudou em outra TV (servidor diz opt-out, sem pendencia local): o espelho local acompanha.
  S.optout = 1; S.nGet = 0;
  novaSessao(); T0 += ESPERA_MS + 100; espera(&S.nGet, 1); gira(30);
  assert(!ajustes_enquetes() && !cur.tem);
  S.optout = 0; ajustes_espelhar_enquetes(1);
  // 11. Opt-out feito pelo Ajustes sem rede fica pendente e vence o espelho do servidor.
  gravarPendente(perfilAtual(), 1);         // o toque foi feito, o POST nao chegou
  S.optout = 0; S.nGet = 0;
  novaSessao(); T0 += ESPERA_MS + 100; espera(&S.nGet, 1); gira(40);
  assert(S.optout == 1);
  S.optout = 0; ajustes_espelhar_enquetes(1); dados_apagar(ARQ_PEND);
  // 12. Trocar de perfil com pedido em voo: o resultado velho some e nada abre.
  serv("logo-5", 86400); S.delayMs = 400;
  novaSessao(); T0 += ESPERA_MS + 100;
  gira(5);
  assert(buscando);
  enquete_perfil_trocado();
  S.delayMs = 0;
  usleep(600000);
  gira(20);
  assert(!ilha_tem(CH_CONVITE) && !cur.tem && ponto == 0);
  // ...e o perfil novo busca de novo, depois de esperar o app assentar.
  S.nGet = 0; T0 += ESPERA_MS + 100;
  espera(&S.nGet, 1); gira(40);
  assert(cur.tem && ilha_tem(CH_CONVITE));
  // 13. Prazo: texto em dias/amanha/hoje.
  { Enq e; char b[64];
    memset(&e, 0, sizeof e);
    e.fim = (long long)time(NULL) + 3 * 86400 - 60; prazoTexto(&e, b, sizeof b); assert(strstr(b, "3"));
    e.fim = (long long)time(NULL) + 3600; prazoTexto(&e, b, sizeof b); assert(b[0]);
    e.fim = (long long)time(NULL) - 5; prazoTexto(&e, b, sizeof b); assert(!b[0]); }
  // 14. DISPENSAR (06/10): "Agora nao" deixa a bolinha; a AZUL nela reabre o
  // convite com "Dispensar"; dispensada, a bolinha apaga e nao volta no
  // arranque seguinte. A proxima enquete convida de novo.
  serv("logo-6", 86400);
  novaSessao(); T0 += ESPERA_MS + 100; espera(&S.nGet, 1); gira(40);
  assert(ilha_tem(CH_CONVITE) && cur.tem && !strcmp(cur.id, "logo-6"));
  enquete_acao(CH_CONVITE, 2);              // Agora nao
  gira(2);
  assert(ponto == 1 && !conviteDoPonto);
  ilha_retirar(CH_CONVITE);
  pontoPedirTeste();                        // a AZUL no relogio com a bolinha
  gira(2);
  assert(ilha_tem(CH_CONVITE) && conviteDoPonto && ponto == 1);
  enquete_acao(CH_CONVITE, 2);              // Dispensar
  assert(ponto == 0 && avisodisp_tem("enquete:logo-6"));
  S.nGet = 0;
  novaSessao(); T0 += ESPERA_MS + 100; espera(&S.nGet, 1); gira(40);
  assert(cur.tem && !strcmp(cur.id, "logo-6") && !ilha_tem(CH_CONVITE) && ponto == 0);
  serv("logo-7", 86400);
  novaSessao(); T0 += ESPERA_MS + 100; espera(&S.nGet, 1); gira(40);
  assert(ilha_tem(CH_CONVITE) && !strcmp(cur.id, "logo-7"));
  puts("enquete: sem bloqueio, convite uma vez, bolinha, voto unico, resultado, opt-out com desfazer, perfil, prazo e dispensar ok");
  return 0;
}
