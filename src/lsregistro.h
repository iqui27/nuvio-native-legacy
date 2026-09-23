// Regras do registro no hub LS2 (LSRegister), sem nada de webOS dentro: o
// video.c chama a lib por dlopen, e o que da para testar no Mac fica aqui.
//
// POR QUE EXISTE (registros 1720-1774, webOS 4, 964 MB, 1.4.1): 1327
// LSRegister recusados em 36 sessoes, um a cada 15 s enquanto o app ficava
// aberto, e a linha do log com `msg=` em lixo. Duas coisas nossas:
//   1. o LSError era lido errado. O campo `message` e um PONTEIRO (luna-service2,
//      include/public/luna-service2/lunaservice.h: `int error_code; char
//      *message; ...`), e o log imprimia os bytes do ponteiro como texto — por
//      isso o lixo mudava a cada tentativa: era o endereco da mensagem nova;
//   2. o trailer tentava de novo para sempre (recuo de 15 s sem teto), e cada
//      recusa deixava a mensagem alocada, porque ninguem chamava LSErrorFree.
// O codigo 4294966269 e -1027 = LS_ERROR_CODE_PERMISSION (-3 - 1024,
// lunaservice-errors.h): o hub disse "Invalid permissions", nao "nome em uso".
#ifndef NV_LSREGISTRO_H
#define NV_LSREGISTRO_H

// Codigos publicos do luna-service2 (_LS_ERROR_CODE_OFFSET = 1024).
#define LSR_UNKNOWN_ERROR   (-1 - 1024)
#define LSR_OOM             (-2 - 1024)
#define LSR_PERMISSION      (-3 - 1024)
#define LSR_DUPLICATE_NAME  (-4 - 1024)
#define LSR_CONNECT_FAILURE (-5 - 1024)
#define LSR_DEPRECATED      (-6 - 1024)
#define LSR_NOT_PRIVILEGED  (-7 - 1024)
#define LSR_NOT_PROXY_PRIV  (-8 - 1024)
#define LSR_PROTOCOL        (-9 - 1024)
#define LSR_EAGAIN          (-10 - 1024)

// Nome curto do codigo ("PERMISSION"), ou NULL se nao e um dos publicos.
const char *lsreg_nome_codigo(int codigo);

// 1 quando a recusa nao muda sozinha dentro da sessao (permissao, privilegio,
// protocolo): o hub decidiu pelo papel do processo, e repetir so gasta.
int lsreg_recusa_da_sessao(int codigo);

// POLITICA DE NOVA TENTATIVA. `automatico` = ninguem pediu (o trailer que se
// oferece no detalhe); o contrario e a pessoa apertando play.
//   - automatico: recuo 3 s, 8 s, 15 s, 30 s, 60 s e DESISTE depois de 5
//     falhas (2 se a recusa e da sessao);
//   - pedido da pessoa: sempre tenta — e uma chamada, e ela quer ver o erro.
typedef struct {
  int falhas;          // seguidas, desde o ultimo sucesso
  int ultimoCodigo;    // da ultima recusa (0 = nenhuma)
  unsigned proximaMs;  // antes disto o automatico nao tenta
} LsRegEstado;

int  lsreg_pode_tentar(const LsRegEstado *e, unsigned agoraMs, int automatico);
void lsreg_falhou(LsRegEstado *e, int codigo, unsigned agoraMs);
void lsreg_deu_certo(LsRegEstado *e);
// 1 quando o automatico ja desistiu nesta sessao (para o log dizer uma vez).
int  lsreg_desistiu(const LsRegEstado *e);

#endif
