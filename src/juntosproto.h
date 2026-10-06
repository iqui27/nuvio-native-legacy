// WATCH TOGETHER — as mensagens do websocket da sala (JSON, uma por quadro).
//
// Contrato: secao 7.3 do plano e servidor/juntos/src/logica.js (quem manda o
// que). Este modulo so monta e le; nao decide nada. Puro (sem SDL, sem rede):
// tests/juntosproto.c.
//
// O texto que aparece na TV sai de chaves traduzidas (idioma_*.h), nunca do
// servidor. A unica excecao e o chat do celular.
#ifndef NV_JUNTOSPROTO_H
#define NV_JUNTOSPROTO_H
#include <stddef.h>
#include "juntossync.h"

#define JSP_MEMBROS 8      // 5 TVs (SALA_MAX) com folga
#define JSP_ID      96
#define JSP_NOME    48

// Impressao digital do release (6.9). NUNCA leva URL.
typedef struct {
  char hash[41];      // infoHash em minusculas, "" se nao ha
  int idx;            // fileIdx; -1 se nao ha
  long long bytes;    // 0 se desconhecido
  char arquivo[121];  // nome do arquivo (sem caminho)
  char binge[81];     // behaviorHints.bingeGroup
  int altura;         // 1080, 2160...; 0 se desconhecida
  long long durMs;    // 0 enquanto desconhecida
  char rotulo[61];    // "1080p WEB-DL" para mostrar no cartao
} JsImpressao;

typedef struct {
  char id[JSP_ID], nome[JSP_NOME], av[256];
  char st[16];        // entrando procurando carregando pronto tocando pausado semfonte erro ausente
  int pct;
  char nivel;         // 'A', 'B' ou 0
  char plat[12];
  char fonte[12];     // mesma parecida diferente nenhuma
  int anf, on, alc, cel;
} JsMembro;

typedef struct {
  int seq;
  double s;           // relogio do servidor quando montou (ms)
  JsRel rel;
  char ep[64], autor[JSP_ID], op[16];
  double delta;       // op == "seek": quanto pulou (s)
  double autoMs;      // lobby do proximo episodio: comeca sozinho neste instante (servidor)
  char esperando[JSP_MEMBROS][JSP_ID]; int nEsperando;
  double prazo;       // servidor ms
  JsMembro m[JSP_MEMBROS]; int n;
  int controleAnf;    // 1 = "so o anfitriao controla"
  int max;
  char titulo[128], poster[512], codigo[8];
  int espera;
  JsImpressao imp;
} JsSala;

typedef enum {
  JSM_NADA = 0, JSM_PONG, JSM_SALA, JSM_MEMBRO, JSM_AVISO, JSM_REACAO, JSM_FRASE, JSM_CHAT,
  JSM_PEDIDO, JSM_PEDIR, JSM_RECUSADO, JSM_FIM, JSM_ERRO, JSM_AGUARDANDO, JSM_ACEITO
} JsMsgTipo;

// Uma mensagem recebida, ja lida. JSM_SALA preenche `sala`; JSM_MEMBRO, `membro`.
typedef struct {
  JsMsgTipo tipo;
  double c, s;              // pong
  char k[32];               // aviso/reacao/frase/recusado/erro/fim(motivo)
  char quem[JSP_ID];        // aviso, pedido, pedir, chat (de), reacao (de)
  char nome[JSP_NOME];      // pedido, aviso de saida
  char av[256];             // pedido
  char txt[256];            // chat
  int prazo, dur;           // aviso esperando (s), fim (s de sala)
  JsMembro membro;
} JsMsg;

// Le uma mensagem. `sala` so e escrita em JSM_SALA (pode ser NULL para ignorar).
JsMsgTipo jsp_ler(const char *json, JsMsg *m, JsSala *sala);

// Montagem. Todas devolvem o tamanho escrito (ou -1 se nao coube).
int jsp_ping(char *b, size_t n, double c);
int jsp_ola(char *b, size_t n, const char *plat, char nivel, double rtt, const char *app);
// `prep`: ms que esta TV leva para estar tocando depois de um seek (latencia de
// seek + de play medidas). O servidor agenda o play com folga para a mais lenta.
int jsp_estado(char *b, size_t n, const char *st, int pct, double pos, double dur, const char *fonte, double rtt,
               double prep);
// op: play pausa seek proximo comecar encerrar controle espera. `pos` < 0 = sem
// posicao; `ep` so para proximo; `valor` so para controle/espera (texto).
int jsp_cmd(char *b, size_t n, const char *op, double pos, int seqBase, const char *ep, const char *valor);
int jsp_simples(char *b, size_t n, const char *t, const char *k);    // reacao, frase, pedir
int jsp_aceitar(char *b, size_t n, const char *quem, int ok);
int jsp_sair(char *b, size_t n);

// A impressao em JSON para POST /v1/sala ("imp":{...}), sem URL por construcao.
int jsp_impressao(char *b, size_t n, const JsImpressao *i);
// Le "imp":{...} de uma resposta (convites, entrar) dentro de [ini,fim).
void jsp_ler_impressao(const char *ini, const char *fim, JsImpressao *i);

// Escapa texto para JSON (aspas, barra, controle). Devolve dst.
char *jsp_esc(char *dst, size_t n, const char *s);

#endif
