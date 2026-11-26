// FAROLESCOLHA_REDE — cliente assincrono POST /v1/farol/escolha (issue 54,
// PLANO-GOSTO secoes 4.2 P5, 5.3 e 5.4).
//
// Traduz uma frase curta falada/digitada no Assistente P5 em respostas
// fechadas do banco de 9 perguntas (src/farolescolha.h) + opcional "falta".
// Sem SDL, sem estado por fio (sem TLS), sem log de frase/titulo/perfil/gosto/token.
//
// PRIVACIDADE E GUARDAS RIGIDAS (0 POSTs antes de abrir rede):
//   - Bloqueia com 0 POSTs e cai no banco local se: perfil infantil,
//     !farolsinal_recomendar(), !farolconsent_llm_ativo() ou
//     farolconsent_estado() != FAROL_CONSENT_SIM, modo seguro (privado),
//     player aberto, offline, sem URL, ou frase vazia/invalida/com URL.
//   - Corpo JSON estritamente restrito as 7 chaves de CHAVES_ESCOLHA do
//     servidor (rotaFarolEscolha): v, idioma, frase, perguntas, gosto,
//     infantil, consentiu. Nunca envia Diario, notas, perfis, amigos,
//     add-ons nem vetor completo de gosto.
//
// FIOS, PRAZO E CANCELAMENTO (R1/R8):
//   - farolescolha_rede_pedir() volta em < 50 ms na UI: auth
//     (recomenda_cabecalhos) e HTTP rodam no fio proprio, sob rede_parar_trocar,
//     contabilizados no prazo total de 6000 ms (saldo em long long).
//   - Geracao monotonica descarta qualquer resposta de pedido cancelado/antigo.
//   - Validacao estrita da resposta: aceita so origem=="llm" com pares validos
//     pertencentes as perguntas enviadas e opcional falta valida nao repetida.
#ifndef NV_FAROLESCOLHA_REDE_H
#define NV_FAROLESCOLHA_REDE_H

#include <stddef.h>
#include <stdint.h>
#include "farolescolha.h"
#include "farolgosto.h"

#define FAROL_ESCOLHA_PRAZO_MS    6000u
#define FAROL_ESCOLHA_FRASE_MAX    200
#define FAROL_ESCOLHA_FRASE_BYTES  804
#define FAROL_ESCOLHA_CORPO_MAX  12288u
#define FAROL_ESCOLHA_GEN_MAX        6
#define FAROL_ESCOLHA_TEMA_MAX       6
#define FAROL_ESCOLHA_PESSOA_MAX     4

typedef enum {
  FER_OCIOSO = 0,
  FER_BUSCANDO,
  FER_PRONTO,      // origem=="llm" valido (respostas e/ou falta)
  FER_FALLBACK,    // qualquer falha, 429, 5xx, prazo, offline, suprimido, origem:"regras"
  FER_CANCELADO
} FarolEscRedeEstado;

typedef enum {
  FER_MOT_NENHUM = 0,
  FER_MOT_OK,
  FER_MOT_REGRAS,
  FER_MOT_SUPRIMIDO,
  FER_MOT_FRASE_INVALIDA,
  FER_MOT_OFFLINE,
  FER_MOT_PRAZO,
  FER_MOT_LIMITE,
  FER_MOT_ERRO,
  FER_MOT_CANCELADO
} FarolEscRedeMotivo;

typedef struct {
  int  perg;            // FESC_* (0..FESC_N-1)
  int  opc;             // indice da opcao no banco (0..nOpc-1)
  char perguntaId[24];
  char opcaoId[24];
} FarolEscRedePar;

typedef struct {
  int             nRespostas;
  FarolEscRedePar respostas[FESC_N];
  int             temFalta;      // 1 se falta != null e valida
  int             faltaPerg;     // FESC_* ou -1
  char            faltaId[24];   // "" se sem falta
} FarolEscRedeResp;

typedef struct {
  FarolEscRedeEstado estado;
  FarolEscRedeMotivo motivo;
  int                http;
  uint64_t           geracao;
} FarolEscRedeInfo;

// ------------------------------------------------------------ funcoes puras --
// Normaliza `src` em `dst`: trim + colapsa espacos, rejeita controles,
// UTF-8 invalido, URLs ("://" ou "www.", sem caixa) e vazio; corta em
// <= 200 codepoints UTF-8 sem cortar sequencia multibyte. 1 = valida.
int farolescolha_rede_normalizar_frase(const char *src, char *dst, size_t cap);

// Monta o corpo JSON estrito de POST /v1/farol/escolha (apenas CHAVES_ESCOLHA).
// `perguntas` tem 1..FESC_N ids FESC_*; `gosto` opcional (NULL = {}).
// Devolve comprimento escrito ou 0 em erro/invalido.
size_t farolescolha_rede_montar_corpo(const char *frase, const char *idioma,
                                      const int *perguntas, int nPerguntas,
                                      const FarolGosto *gosto,
                                      char *dst, size_t cap);

// Valida estritamente a resposta JSON de POST /v1/farol/escolha:
// exige origem=="llm", respostas com pares (perguntaId, opcaoId) validos e
// pertencentes a `perguntas[0..nPerguntas-1]`, falta opcional valida e nao
// repetida em respostas (ou null), e ao menos 1 resposta ou falta.
// Devolve 1 se valida (preenche `out`); 0 se fallback (`*ehRegrasOut = 1` se
// origem=="regras").
int farolescolha_rede_validar_resposta(const char *json, size_t len,
                                       const int *perguntas, int nPerguntas,
                                       FarolEscRedeResp *out, int *ehRegrasOut);

// ------------------------------------------------ guardas e controle async --
// 1 se o envio remoto esta autorizado agora (consentimento SIM + !infantil +
// !privado + recomendar + !player_aberto).
int farolescolha_rede_autorizado(void);

// Inicia POST /v1/farol/escolha em fio proprio (retorna em < 50 ms na UI).
// `esc` fornece as perguntas ainda abertas (FESC_SEM_RESP). `gosto` opcional
// (se NULL, le farolsinal_modelo). Devolve geracao > 0 se disparou rede;
// 0 se bloqueado/invalido/offline (0 POSTs, estado FER_FALLBACK).
uint64_t farolescolha_rede_pedir(const char *frase, const FarolEsc *esc,
                                 const FarolGosto *gosto);
uint64_t farolescolha_rede_pedir_perguntas(const char *frase,
                                           const int *perguntas, int nPerguntas,
                                           const FarolGosto *gosto);

void farolescolha_rede_cancelar(void);
void farolescolha_rede_encerrar(void);

FarolEscRedeEstado farolescolha_rede_estado(FarolEscRedeInfo *info);
// Copia a resposta pronta se estado == FER_PRONTO e geracao == geracaoEsperada
// (ou geracaoEsperada == 0). Devolve 1 se copiou.
int farolescolha_rede_resposta(uint64_t geracaoEsperada, FarolEscRedeResp *out);
int farolescolha_rede_em_voo(void);

// ------------------------------------------------------------ ganchos de teste
void farolescolha_rede_definir_url(const char *baseUrl);
void farolescolha_rede_definir_auth(const char *autorizacao, const char *via,
                                    const char *perfilCab);
void farolescolha_rede_teste_zera(void);

#endif // NV_FAROLESCOLHA_REDE_H
