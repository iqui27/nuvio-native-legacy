// A conta: tokens em disco, login de TV por codigo, renovacao e saida.
//
// Este e o modulo que troca os arquivos de segredo do dono (art/trakt.txt,
// art/addons.txt) por uma sessao de verdade. Enquanto ele nao existir, o .ipk
// nao pode ser distribuido: ele entrega o token Trakt e as chaves de debrid de
// quem o montou.
//
// FLUXO, lido de js/core/auth/qrLoginService.js. Nao e o login por e-mail e
// senha do web, e nao por preferencia: o teclado do busca.c so tem "a-z0-9",
// sem maiuscula, sem "@" e sem ponto. Um e-mail nao e digitavel neste app hoje.
//   1. sessao ANONIMA (POST /auth/v1/signup; se recusar, /auth/v1/token?
//      grant_type=anonymous). Sem ela o servidor responde 401 as duas RPC
//      seguintes.
//   2. start_tv_login_session -> devolve um CODIGO.
//   3. a pessoa abre a URL no celular e digita o codigo.
//   4. poll_tv_login_session ate autorizar.
//   5. /functions/v1/tv-logins-exchange -> access_token + refresh_token.
//
// O passo 1 e o que mais surpreende: parece que o login comeca do zero, mas a
// TV ja precisa estar autenticada como ALGUEM para pedir um codigo.
#ifndef NV_SESSAO_H
#define NV_SESSAO_H

typedef enum {
  SES_DESLOGADO = 0,
  SES_PEDINDO,      // buscando codigo
  SES_AGUARDANDO,   // codigo na tela, esperando a pessoa autorizar
  SES_TROCANDO,     // autorizado, trocando pelo token
  SES_LOGADO,
  SES_ERRO
} SesEstado;

// Carrega a sessao gravada, se houver. Chamar depois de dados_iniciar e
// nuvem_configurar.
void sessao_iniciar(void);

SesEstado   sessao_estado(void);
int         sessao_logada(void);          // 1 so com sessao de USUARIO
const char *sessao_codigo(void);          // codigo a exibir; "" fora do fluxo
const char *sessao_url_login(void);       // URL a exibir
const char *sessao_erro(void);            // ultima falha, para a tela mostrar
const char *sessao_usuario(void);         // `sub` do JWT; "" quando deslogado

// TOKEN DE ACESSO CRU, para quem precisa se autenticar em um servidor que NAO
// e o Supabase do Nuvio. Hoje so o servico de recomendacoes (recomenda.c), que
// manda `Authorization: Bearer <isto>` e pergunta ao proprio Supabase de quem
// e — e por isso ele precisa do token e nao do `sub`.
//
// Devolve "" quando nao ha sessao de USUARIO: uma sessao anonima (ver o passo 1
// do fluxo acima) tem token e nao tem dono, e mandar aquele token a outro
// servidor criaria uma "pessoa" nova a cada arranque. Quem chama nunca deve
// gravar o valor em disco nem em log.
const char *sessao_token(void);

// Comeca o fluxo de login num fio proprio (as chamadas bloqueiam). Idempotente
// enquanto um fluxo estiver em andamento.
void sessao_login_comecar(void);

// Faz um passo do fluxo. Chamar uma vez por quadro; nao bloqueia. E aqui que o
// poll e reagendado — nao existe temporizador escondido dentro do modulo.
void sessao_passo(unsigned agoraMs);

void sessao_cancelar(void);

// Apaga tokens do disco e da memoria. Depois disto o app volta a nao ter conta
// nenhuma — e e isso que o usuario espera de "sair".
void sessao_sair(void);

// RPC AUTENTICADA como o usuario. Devolve o corpo (free pelo chamador) ou NULL.
// Em 401 renova o token e repete UMA vez; se a renovacao falhar, a sessao cai
// para SES_DESLOGADO em vez de ficar num limbo em que o app parece logado e
// nada sincroniza. BLOQUEIA — chamar de um fio de sync, nunca do laco de
// desenho.
char *sessao_rpc(const char *funcao, const char *corpoJson, int *status);

// Mesma coisa para /functions/v1/<nome>.
char *sessao_funcao(const char *nome, const char *corpoJson, int *status);

// LEITURA de tabela como o usuario. Existe porque nem toda superficie tem RPC:
// a lista de addons so sai da tabela `addons`, e ler com a chave anonima
// devolve 401 "permission denied for table addons" — o RLS precisa do token de
// quem esta pedindo para saber quais linhas sao dele.
char *sessao_tabela(const char *tabela, const char *consulta, int *status);

#endif
