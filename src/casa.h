// NUVIO CASA (pesquisa de mercado 4.4), PRIMEIRA ETAPA: os aparelhos da casa se
// ajudam. Um aparelho com o motor P2P embutido (p2pmotor.h) vira AJUDANTE das
// TVs da mesma rede que nao tem motor: a TV acha o ajudante sozinha e toca o
// torrent pelo motor dele, sem ninguem digitar endereco.
//
// DECISAO DO DONO: aprovado apesar do veto a "proxy universal de midia" do
// docs/issues/PLANO-MERCADO.md. O que mantem isto longe do veto:
//   - o ajudante so repassa o servidor do PROPRIO motor (127.0.0.1); nunca
//     busca um endereco que o cliente mandou;
//   - so atende rede privada, com o token que a descoberta entregou;
//   - nenhum servidor nosso no caminho.
//
// UM CASO SO, ponta a ponta: P2P. O ajudante fala o mesmo recorte do servidor
// de streaming do Stremio que p2p.c ja consome:
//   GET  <base>/settings              -> {"values":{"serverVersion":"nuvio-casa/1"}}
//   POST <base>/<hash>/create?idx=&s=&e= -> sobe o torrent no motor; {"files":[...]}
//   GET  <base>/<hash>/0              -> o video, com Range (repasse ao motor)
// com <base> = http://<ip>:<porta>/<token>. Conversao de audio, Dolby Vision,
// cache e legendas da ideia original NAO estao aqui.
//
// DESCOBERTA: a TV manda CASA_PERGUNTA por UDP em broadcast na porta
// CASA_PORTA_UDP; o ajudante responde so a quem perguntou com porta, token e
// nome. O IP do ajudante e o de ORIGEM do pacote, nunca um campo do texto.
//
// Este arquivo e a regra pura (sem socket): testada em tests/casa.c. A rede
// mora em casa_rede.c.
#ifndef NV_CASA_H
#define NV_CASA_H
#include <stdint.h>

#define CASA_PORTA_UDP  47470
#define CASA_PORTA_HTTP 47471      // preferida; ocupada, o sistema escolhe outra
#define CASA_VERSAO     "nuvio-casa/1"
#define CASA_PERGUNTA   "NUVIO-CASA/1 ?"
#define CASA_VALIDADE_S 90         // ajudante sem resposta ha mais que isto some
#define CASA_SONDA_S    30         // intervalo entre perguntas
#define CASA_OCIOSO_S   120        // ajudante: sem pedido ha tanto, solta o motor

typedef struct {
  uint32_t ip;                     // ordem de host; preenchido por quem recebeu
  int porta;
  char token[33];                  // 16 a 32 hexadecimais
  char nome[48];
  int p2p;                         // capacidade anunciada
  long visto;                      // segundos (relogio de quem recebeu)
} CasaAjudante;

// 10/8, 172.16/12, 192.168/16, 127/8, 100.64/10 e 169.254/16 (ordem de host).
int casa_ip_privado(uint32_t ip);

// 1 quando o pacote e a pergunta desta versao (espacos e \r\n no fim valem).
int casa_e_pergunta(const char *msg, int len);
// "NUVIO-CASA/1 ! porta=47471 token=<hex> cap=p2p nome=<nome>". O nome perde
// tudo que nao e letra, numero, espaco, '-', '_' ou '.'. Tamanho, 0 se nao coube.
int casa_montar_resposta(int porta, const char *token, const char *nome, int p2p,
                         char *out, unsigned n);
// 1 se a resposta e valida; preenche porta, token, nome e p2p (ip/visto nao).
int casa_ler_resposta(const char *msg, int len, CasaAjudante *a);

// Entre os conhecidos, o que serve agora: anuncia P2P, IP privado, visto ha
// menos de CASA_VALIDADE_S. Empate: o visto por ultimo; ainda empate, o menor
// IP (a escolha nao pula de um para outro a cada sonda). Indice ou -1.
int casa_escolher(const CasaAjudante *l, int n, long agora);

// http://a.b.c.d:porta/token. 1 se coube.
int casa_base(const CasaAjudante *a, char *out, unsigned n);

// ---- lado do ajudante: o pedido HTTP ----
enum { CASA_ROTA_NADA = 0, CASA_ROTA_SETTINGS, CASA_ROTA_CRIAR, CASA_ROTA_ARQUIVO };
typedef struct {
  int rota;
  char hash[41];                   // minusculas
  int idx, temporada, episodio;    // create: da consulta (-1/0/0 sem ela)
} CasaPedido;
// `metodo` e `alvo` sao os dois primeiros campos da linha de pedido. So casa o
// caminho exato com o token certo: /<token>/settings (GET), /<token>/<hash>/create
// (POST) e /<token>/<hash>/<n> (GET, HEAD). Qualquer outra coisa: CASA_ROTA_NADA.
int casa_rota(const char *metodo, const char *alvo, const char *token, CasaPedido *p);
// O Host do pedido tem de ser um IPv4 literal privado com a porta do ajudante:
// um nome (DNS rebinding) nao passa.
int casa_host_ok(const char *host, int porta);
// As entradas "tracker:..."/"dht:..." do corpo do create (p2p_corpo_criar), uma
// por linha, como p2pmotor_resolver recebe. Quantas entraram.
int casa_fontes_do_corpo(const char *corpo, char *out, unsigned n);
// {"files":[{"path":"casa.mkv","name":"casa.mkv","length":<tam>}]}: um arquivo
// so, o que o motor ja escolheu; p2p_escolher_arquivo devolve 0.
int casa_json_arquivos(uint64_t tam, char *out, unsigned n);
// So http://127.0.0.1:<porta>/<caminho>: o unico destino que o repasse aceita.
// Devolve a porta e aponta `caminho` para a barra, ou 0.
int casa_url_do_motor(const char *url, const char **caminho);
#endif
