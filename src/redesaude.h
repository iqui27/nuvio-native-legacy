// SAUDE DA REDE: "sem internet" e "internet de volta" para a ilha do relogio.
//
// Mockup aprovado pelo dono em 02/10 (estados "Sem internet" e "Conexao
// voltou", selo "sinal novo"). Ate aqui nao existia sinal GLOBAL de
// conectividade: rede.c responde pedido a pedido e cada modulo engolia a sua
// falha — a home ficava vazia e a pessoa nao sabia se era o app, o addon ou a
// casa sem internet. Este modulo SO CONTA: rede.c informa cada pedido que
// terminou (ok de transporte, ou falha de transporte), e o estado sai daqui.
//
// O QUE CONTA COMO FALHA: so TRANSPORTE — DNS, conexao recusada, prazo, TLS que
// nao fechou, conexao caida no meio (CURLcode 6, 7, 28, 35, 52, 55, 56; no
// Tizen-wasm, XHR com status 0). HTTP 4xx/5xx NAO e falha de rede: o servidor
// respondeu. Pedido CANCELADO (o fio desistiu, o teto cortou) nao conta nada.
// Endereco local (127.x, localhost, 10.x, 192.168.x — o proxy do proprio app,
// o servidor P2P da casa) nao conta para nenhum lado: ele responde com a
// internet fora e ficaria "voltando" a toda hora.
//
// HISTERESE, para nao piscar num canto de TV:
//   - SEM INTERNET exige 3 falhas SEGUIDAS (nenhum sucesso no meio), de pelo
//     menos 2 hosts diferentes, ao longo de pelo menos 3 s. Um addon lento so
//     da timeout num host so; um site fora do ar nao derruba os outros.
//   - DE VOLTA e o primeiro sucesso depois disso.
//   - Depois de voltar, 10 s de carencia antes de poder declarar "sem" de novo.
//
// Fio-seguro (rede.c chama de qualquer fio). O nucleo (rede_saude_passo) e
// puro e sem relogio proprio, para o teste (tests/redesaude.c).
#ifndef NV_REDESAUDE_H
#define NV_REDESAUDE_H

#define REDE_SAUDE_FALHAS   3
#define REDE_SAUDE_HOSTS    2
#define REDE_SAUDE_JANELA   3000u
#define REDE_SAUDE_CARENCIA 10000u
// #382: depois do boot o resolvedor da TV leva minutos para responder (19 curl 28
// "Resolving timed out" em hosts distintos, sem a casa estar fora). Enquanto
// nenhum pedido deu certo, nos primeiros 60 s, timeout de DNS nao conta.
// CURLcode 28 sem resolver chega aqui como REDE_CURL_DNS_LENTO.
#define REDE_CURL_DNS_LENTO 1028
#define REDE_SAUDE_ARRANQUE 60000u

typedef struct {
  int falhas;               // falhas de transporte seguidas
  unsigned primeira;        // ms da primeira falha da sequencia, incluindo retry
  unsigned ultimoOk;        // a janela nunca comeca antes do ultimo sucesso
  int temOk;
  unsigned hosts[4];        // hosts distintos da sequencia (hash)
  int nHosts;
  int offline;              // 1 = "sem internet" declarado
  unsigned voltou;          // ms em que voltou (carencia)
  int voltouOk;
  unsigned seq;             // sobe a cada troca de estado
  unsigned inicio;          // ms da primeira nota (0 = ainda nenhuma)
  int temInicio;
} RedeSaude;

// O nucleo: um pedido terminou. `ok` 1 = transporte ok, 0 = falha de
// transporte. `host` = hash do host (0 = ignorar). Devolve 1 se o estado mudou.
int  rede_saude_passo(RedeSaude *s, int ok, unsigned host, unsigned agoraMs);
// Igual, para falha que e so timeout de DNS (ignorada no arranque).
int  rede_saude_passo_dns(RedeSaude *s, unsigned host, unsigned agoraMs);

// Para rede.c: o resultado de um pedido. `codigo` e o CURLcode (0 = ok) ou,
// no wasm, 0 para resposta e 6 para "sem resposta". Codigos que nao sao de
// transporte (cancelamento, teto) sao ignorados aqui mesmo.
void rede_saude_nota(int codigo, const char *url);
// Resultado final com o instante da primeira falha do retry (0 = agora).
void rede_saude_nota_desde(int codigo, const char *url, unsigned primeiraMs);
// Estado atual e o contador de trocas (quem avisa compara com o que viu).
int      rede_saude_offline(void);
unsigned rede_saude_seq(void);

// PEDIDOS POR HOST (a aba Rede do painel de registro, 03/10). Cada pedido que
// terminou conta para o host dele: pedidos, falhas (transporte ou HTTP >= 400)
// e o tempo dos ultimos 16 (o "tipico" e a mediana). So o HOST e guardado: o
// caminho pode levar a chave do debrid. Enderecos locais ficam de fora.
#define REDE_HOSTS_MAX 24
typedef struct {
  char host[64];
  int pedidos, falhas;
  unsigned tipicoMs;       // mediana dos ultimos medidos; 0 = sem medida
} RedeHost;
void rede_hosts_nota(const char *url, int codigo, int http, unsigned ms);
// Copia ate `max` hosts, os de mais pedidos primeiro. Devolve quantos.
int  rede_hosts_ler(RedeHost *dst, int max);
// Para a tela "sem internet" do envio de registro: falhas de transporte
// seguidas e em quantos hosts (ate 4), o ultimo host que respondeu e ha quanto
// tempo. Devolve 0 se nenhum pedido respondeu ainda nesta sessao.
#include <stddef.h>
int  rede_saude_resumo(int *falhas, int *hosts, char *ultHost, size_t tam, unsigned *haMs);

#endif
