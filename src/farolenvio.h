// FAROL — envio do registro (PLANO.md §2.6, §2.7, §3.3, §5 issue 18).
//
// `farol_enviar(entrada, opcoes)` faz o envio inteiro num fio proprio, com
// prazo rigido de 30 s, cancelavel e SEM nenhum I/O bloqueante no fio da UI:
//
//   1. Monta o FarolRegistro v1 (farolpacote.h) DENTRO do fio de envio, com o
//      JSON e os 256 eventos mais recentes do anel (farol_eventos) no heap e
//      pilha propria de FAROLENVIO_PILHA — a montagem usa ~36 KB de pilha, e o
//      padrao de pthread no WASM/fibras e pequeno.
//   2. Consulta GET /v1/farol/conhecido?fp=&c=&p=&v=&so_maior= (contrato de
//      servidor/recomendacoes/src/farol.js). `corrigido` (correcao numa versao
//      MAIOR que a desta TV) encerra em FAROLENVIO_JA_CORRIGIDO com versao e
//      nota — o registro NAO e enviado.
//   3. POST /v1/farol/registro; le `codigo` (6 caracteres, exibido como
//      NV-XXXX-XX) e o `conhecido` opcional, registra farol_registrar_envio no
//      anel e poe o codigo na lista de enviados (resolverregra_enviados_add,
//      arquivo FAROLENVIO_ENVIADOS_ARQ, o mesmo do resolver).
//   4. Sem internet (rede_saude_offline) ou falha de rede/prazo, guarda UM
//      pacote pendente em FAROLENVIO_PENDENTE_ARQ (<= 35 KB, vale 24 h).
//      farolenvio_passo() (por quadro, sem I/O) reenvia quando a rede volta;
//      o pendente some depois do envio aceito, de 422, de "ja corrigido" ou
//      depois de 24 h. O reenvio e seguro: o servidor deduplica por (conta,fp)
//      e devolve o MESMO codigo.
//
// PRIVACIDADE / OPT-IN: nada sai sem uma chamada explicita de farol_enviar —
// o pendente so existe porque a pessoa pediu o envio. `incluir_log` nasce 0:
// so com 1 o registro geral da sessao (avisos_enviar_registro_atual) e pedido
// depois de um envio aceito, e sempre a partir de farolenvio_passo (fio da UI).
// Nenhum token, URL ou corpo vai para log.
//
// Portabilidade: sem SDL neste header, sem _Thread_local (TPK40), pthread puro
// (fibras no NV_UM_FIO), compativel com webOS/TPK/TPK40/Android/WASM/Linux.
#ifndef NV_FAROLENVIO_H
#define NV_FAROLENVIO_H

#include "farolpacote.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FAROLENVIO_PRAZO_MS           30000u          /* prazo total do envio */
#define FAROLENVIO_PRAZO_CONHECIDO_MS 8000u           /* parte do prazo para o GET */
#define FAROLENVIO_PILHA              (256u * 1024u)  /* pilha do fio de envio */
#define FAROLENVIO_PENDENTE_ARQ       "farol-pendente.json"
#define FAROLENVIO_ENVIADOS_ARQ       "resolver-enviados.txt" /* = RESOLVER_ARQ */
#define FAROLENVIO_PENDENTE_MAX       (35u * 1024u)   /* teto do arquivo pendente */
#define FAROLENVIO_PENDENTE_VALIDADE_S (24L * 3600L)
#define FAROLENVIO_REENVIO_S          60L             /* espera entre reenvios */
#define FAROLENVIO_REENVIO_LIMITE_S   (15L * 60L)     /* espera apos 429 */

typedef enum {
  FAROLENVIO_OCIOSO = 0,
  FAROLENVIO_ENVIANDO,
  FAROLENVIO_OK,            /* aceito: `codigo` novo */
  FAROLENVIO_JA_CORRIGIDO,  /* conhecido `corrigido`: nada enviado */
  FAROLENVIO_PENDENTE,      /* sem internet / sem resposta: guardado */
  FAROLENVIO_FALHOU,        /* ver `motivo` (429, 422, 5xx, conta, ...) */
  FAROLENVIO_CANCELADO
} FarolEnvioEstado;

/* 1..7 tem o MESMO valor de AVISOS_ENVIO_* (avisos.h; _Static_assert em
 * avisos.c), para o painel de envio ler as duas fontes do mesmo jeito. */
typedef enum {
  FAROLENVIO_M_NENHUM = 0,
  FAROLENVIO_M_OK = 1,            /* AVISOS_ENVIO_OK */
  FAROLENVIO_M_SERVIDOR = 2,      /* AVISOS_ENVIO_SERVIDOR: 5xx / resposta sem codigo */
  FAROLENVIO_M_OFFLINE = 3,       /* AVISOS_ENVIO_OFFLINE: rede_saude_offline */
  FAROLENVIO_M_PRAZO = 4,         /* AVISOS_ENVIO_PRAZO: 30 s sem resposta */
  FAROLENVIO_M_CONEXAO = 5,       /* AVISOS_ENVIO_CONEXAO: transporte caiu */
  FAROLENVIO_M_CONTA = 6,         /* AVISOS_ENVIO_CONTA: sem login / 401 */
  FAROLENVIO_M_INDISPONIVEL = 7,  /* AVISOS_ENVIO_INDISPONIVEL: build sem servidor */
  FAROLENVIO_M_LIMITE = 8,        /* 429: o ultimo codigo fica preservado */
  FAROLENVIO_M_RECUSADO = 9,      /* 422: pacote fora do esquema */
  FAROLENVIO_M_CORRIGIDO = 10,    /* conhecido `corrigido` */
  FAROLENVIO_M_CANCELADO = 11,
  FAROLENVIO_M_PACOTE = 12,       /* montagem do pacote falhou */
  FAROLENVIO_M_EXPIROU = 13       /* pendente passou de 24 h e foi apagado */
} FarolEnvioMotivo;

typedef struct {
  int incluir_log;  /* 0 (padrao): so o FarolRegistro; 1: pede tambem o log da sessao */
  int res_sintoma;  /* RES_S_* guardado na lista de enviados (padrao RES_S_GERAL) */
} FarolEnvioOpcoes;

typedef struct {
  FarolEnvioEstado estado;
  FarolEnvioMotivo motivo;
  int      http;                 /* ultimo HTTP do POST (0 = sem resposta) */
  int      doPendente;           /* 1 = resultado da retentativa do pendente */
  char     codigo[8];            /* ultimo codigo aceito ("" = nenhum); preservado em 429/erro */
  char     codigoExibir[12];     /* "NV-XXXX-XX" */
  long     codigoQuando;         /* epoch do ultimo codigo */
  int      codigoNovo;           /* 1 = `codigo` veio DESTE resultado */
  char     conhecidoEstado[16];  /* "corrigido" | "conhecido" | "contorno" | "" */
  char     conhecidoVersao[48];
  char     notaPt[256];
  char     notaEn[256];
  int      temPendente;
  long     pendenteCriado;       /* epoch; 0 = sem pendente */
  long     pendenteVence;
  size_t   bytes;                /* tamanho do pacote montado/enviado */
  int      linhaEscritos, linhaCortados;
  char     fp[FAROL_PACOTE_FP_HEX_N];
  int      incluirLog;           /* opcao do ultimo envio */
  uint64_t geracao;              /* sobe a cada resultado publicado */
} FarolEnvioInfo;

void farolenvio_opcoes_padrao(FarolEnvioOpcoes *o);

/* Descobre (em fio proprio) um pendente deixado por uma sessao anterior.
 * Chamar uma vez no arranque, depois de dados_iniciar. */
void farolenvio_iniciar(void);

/* Pedido EXPLICITO da pessoa. Copia `in` (strings inclusive) e devolve 1 se o
 * fio comecou; 0 se ja ha um envio em voo ou argumento invalido. `op` NULL =
 * farolenvio_opcoes_padrao. Nao bloqueia. */
int  farol_enviar(const FarolPacoteEntrada *in, const FarolEnvioOpcoes *op);

/* Por quadro, no fio da UI, sem I/O: reenvia o pendente quando a rede volta
 * (e a espera venceu), apaga o vencido, e pede o log se `incluir_log`. */
void farolenvio_passo(void);

/* Reenvio manual do pendente ("Tentar de novo"). 1 = comecou. */
int  farolenvio_retentar(void);
/* Descarta o pendente (em fio; sem I/O na UI). 0 = ha envio em voo. */
int  farolenvio_descartar_pendente(void);

void farolenvio_cancelar(void);   /* nao bloqueia; o fio encerra em < 300 ms */
void farolenvio_encerrar(void);   /* cancela e espera os fios (saida/testes) */
int  farolenvio_em_voo(void);
FarolEnvioEstado farolenvio_info(FarolEnvioInfo *o);

/* Cabecalhos de conta do envio (avisos.c, o mesmo idHead do registro).
 * Chamado SO dentro do fio de envio: a copia do token pode renovar por HTTP.
 * cab[0..2] = Authorization, X-Nuvio-Auth, Content-Type; cab[3] = NULL. */
int  avisos_registro_cabecalhos(const char **cab, char *aut, size_t nAut,
                                char *via, size_t nVia, char *chave, size_t nChave);

/* Testes / integracao. */
void farolenvio_definir_url(const char *baseUrl);      /* NULL = padrao do app */
void farolenvio_teste_relogio(long long (*s)(void));   /* epoch em s; NULL = time() */
void farolenvio_teste_prazos(unsigned totalMs, unsigned conhecidoMs); /* 0 = padrao */
void farolenvio_teste_esquecer(void);                  /* zera o estado (sem fio em voo) */

#ifdef __cplusplus
}
#endif

#endif /* NV_FAROLENVIO_H */
